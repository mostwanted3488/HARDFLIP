#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout HardflipProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto db = [] { return AudioParameterFloatAttributes().withLabel ("dB"); };
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "drive", 1 },  "Drive",  NormalisableRange<float> (0.0f, 24.0f, 0.01f), 6.0f, db()));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "thresh", 1 }, "Thresh", NormalisableRange<float> (-24.4f, 0.0f, 0.01f), -6.0f, db()));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "post", 1 },   "Post",   NormalisableRange<float> (-12.0f, 0.0f, 0.01f), 0.0f, db()));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 },    "Mix",    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 },   "Mode",   StringArray { "Soft", "Hard" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

HardflipProcessor::HardflipProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HARDFLIP", createLayout())
{
    pDrive  = apvts.getRawParameterValue ("drive");
    pThresh = apvts.getRawParameterValue ("thresh");
    pPost   = apvts.getRawParameterValue ("post");
    pMix    = apvts.getRawParameterValue ("mix");
    pHard   = apvts.getRawParameterValue ("mode");
    pBypass = apvts.getRawParameterValue ("bypass");
    bypassParam = dynamic_cast<AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool HardflipProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::stereo() || in == AudioChannelSet::mono());
}

void HardflipProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = jmax (1, samplesPerBlock);

    // 4x, linear-phase half-band FIR, integer latency
    os = std::make_unique<dsp::Oversampling<float>> (2, 2, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) maxBlock);
    os->reset();
    const float latency = os->getLatencyInSamples();
    setLatencySamples ((int) std::lround (latency));

    dryDelay.prepare ({ sampleRate, (uint32) maxBlock, 2 });
    dryDelay.setMaximumDelayInSamples ((int) std::ceil (latency) + 8);
    dryDelay.setDelay (latency);
    dryDelay.reset();
    dryBuf.setSize (2, maxBlock, false, true, false);

    const double osRate = sampleRate * 4.0;
    sDrive.reset (osRate, 0.02);  sDrive.setCurrentAndTargetValue (hardflip::dbToGain (pDrive->load()));
    sThresh.reset (osRate, 0.02); sThresh.setCurrentAndTargetValue (hardflip::dbToGain (pThresh->load()));
    sPost.reset (osRate, 0.02);   sPost.setCurrentAndTargetValue (hardflip::dbToGain (pPost->load()));
    sHard.reset (osRate, 0.015);  sHard.setCurrentAndTargetValue (pHard->load() > 0.5f ? 1.0f : 0.0f);
    sMix.reset (sampleRate, 0.02);
    sMix.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 0.0f : pMix->load() * 0.01f);
    sMeterDrive.reset (sampleRate, 0.02); sMeterDrive.setCurrentAndTargetValue (sDrive.getCurrentValue());

    inRing.fill (0.0f); outRing.fill (0.0f);
    ringPos.store (0);
}

void HardflipProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (2, buffer.getNumChannels()), numSamples = buffer.getNumSamples();
    for (int ch = buffer.getNumChannels(); --ch >= 2;) buffer.clear (ch, 0, numSamples);
    if (numCh == 0 || numSamples == 0 || os == nullptr) return;

    const float drive = hardflip::dbToGain (pDrive->load());
    sDrive.setTargetValue (drive);
    sMeterDrive.setTargetValue (drive);
    sThresh.setTargetValue (hardflip::dbToGain (pThresh->load()));
    sPost.setTargetValue (hardflip::dbToGain (pPost->load()));
    sHard.setTargetValue (pHard->load() > 0.5f ? 1.0f : 0.0f);
    // bypass = mix fades to the (latency-aligned) dry signal, so toggling it never clicks or shifts timing
    sMix.setTargetValue (pBypass->load() > 0.5f ? 0.0f : jlimit (0.0f, 1.0f, pMix->load() * 0.01f));

    for (int pos = 0; pos < numSamples; pos += maxBlock)
        processChunk (buffer, pos, jmin (maxBlock, numSamples - pos), numCh);
}

void HardflipProcessor::processChunk (AudioBuffer<float>& buffer, int start, int n, int numCh)
{
    for (int ch = 0; ch < numCh; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, start, n);

    // ---- wet: drive -> clip -> post, at 4x
    dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) start, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int nUp = (int) up.getNumSamples();
    float* u[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
    for (int i = 0; i < nUp; ++i)
    {
        const float d = sDrive.getNextValue(), t = sThresh.getNextValue(), p = sPost.getNextValue(), h = sHard.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            u[ch][i] = hardflip::clip (u[ch][i] * d, t, h) * p;
    }
    os->processSamplesDown (block);

    // ---- dry/wet mix with the dry delayed by the oversampling latency, meters, scope
    float* w[2] = { buffer.getWritePointer (0, start), numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };
    float pkIn = 0.0f, pkOut = 0.0f;
    int rp = ringPos.load();
    for (int i = 0; i < n; ++i)
    {
        const float m = sMix.getNextValue(), md = sMeterDrive.getNextValue();
        float inMono = 0.0f, outMono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            dryDelay.pushSample (ch, dryBuf.getSample (ch, i));
            const float dry = dryDelay.popSample (ch);
            float y = w[ch][i] * m + dry * (1.0f - m);
            if (! std::isfinite (y)) y = 0.0f;
            w[ch][i] = y;
            pkIn = jmax (pkIn, std::abs (dry * md));
            pkOut = jmax (pkOut, std::abs (y));
            inMono += dry * md;
            outMono += y;
        }
        const float k = numCh > 1 ? 0.5f : 1.0f;
        inRing[(size_t) rp] = inMono * k;
        outRing[(size_t) rp] = outMono * k;
        rp = (rp + 1) % kScope;
    }
    ringPos.store (rp);

    auto raise = [] (std::atomic<float>& a, float v)
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    };
    raise (inPk, pkIn);
    raise (outPk, pkOut);
}

void HardflipProcessor::getScope (std::array<float, kScope>& in, std::array<float, kScope>& out) const
{
    const int rp = ringPos.load();
    for (int i = 0; i < kScope; ++i)
    {
        const size_t j = (size_t) ((rp + i) % kScope);
        in[(size_t) i] = inRing[j];
        out[(size_t) i] = outRing[j];
    }
}

AudioProcessorEditor* HardflipProcessor::createEditor() { return new HardflipEditor (*this); }

void HardflipProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void HardflipProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HardflipProcessor(); }
