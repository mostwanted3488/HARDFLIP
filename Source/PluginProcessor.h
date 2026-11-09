#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "DSP/ClipDsp.h"

// HARDFLIP: soft / hard clipper with drive, threshold, output (post) gain and dry/wet mix.
// The clip runs at 4x oversampling (linear-phase FIR), the dry path is delayed by the same
// latency so MIX never comb-filters, and the latency is reported to the host.
class HardflipProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kScope = 1024;   // host samples kept for the IN vs OUT scope

    HardflipProcessor();
    ~HardflipProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HARDFLIP"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // meters for the editor: highest peak since the last call (linear), then reset
    float takeInPeak()  { return inPk.exchange (0.0f); }
    float takeOutPeak() { return outPk.exchange (0.0f); }
    // last kScope samples (mono) of the driven input and of the output, time-aligned, oldest first
    void getScope (std::array<float, kScope>& in, std::array<float, kScope>& out) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n, int numCh);

    std::atomic<float> *pDrive = nullptr, *pThresh = nullptr, *pPost = nullptr, *pMix = nullptr, *pHard = nullptr, *pBypass = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuf;
    juce::SmoothedValue<float> sDrive, sThresh, sPost, sHard;   // oversampled rate
    juce::SmoothedValue<float> sMix, sMeterDrive;                // host rate
    int maxBlock = 512;

    std::array<float, kScope> inRing {}, outRing {};
    std::atomic<int> ringPos { 0 };
    std::atomic<float> inPk { 0.0f }, outPk { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardflipProcessor)
};
