#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    String mimeFor (const String& name)
    {
        const auto ext = name.fromLastOccurrenceOf (".", false, false).toLowerCase();
        if (ext == "html") return "text/html";
        if (ext == "js")   return "text/javascript";
        if (ext == "ttf")  return "font/ttf";
        if (ext == "json") return "application/json";
        return "application/octet-stream";
    }

    void setParam (AudioProcessorValueTreeState& apvts, const String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const float norm = p->convertTo0to1 (plain);
            if (std::abs (p->getValue() - norm) > 1.0e-6f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (norm);
                p->endChangeGesture();
            }
        }
    }

    float getParam (AudioProcessorValueTreeState& apvts, const String& id)
    {
        auto* p = apvts.getParameter (id);
        return p ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    }
}

bool HardflipEditor::Browser::pageAboutToLoad (const String& url)
{
    return url == getResourceProviderRoot() || url.startsWith (getResourceProviderRoot());
}

HardflipEditor::HardflipEditor (HardflipProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      web (WebBrowserComponent::Options{}
#if JUCE_WINDOWS
               .withBackend (WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2{}
                                            .withUserDataFolder (File::getSpecialLocation (File::tempDirectory).getChildFile ("HARDFLIP-webview")))
#endif
               .withKeepPageLoadedWhenBrowserIsHidden()
               .withNativeIntegrationEnabled()
               .withNativeFunction ("hfGetState", [this] (const Array<var>&, WebBrowserComponent::NativeFunctionCompletion done)
                                    { done (buildStateJson()); })
               .withNativeFunction ("hfSetState", [this] (const Array<var>& a, WebBrowserComponent::NativeFunctionCompletion done)
                                    { if (a.size() > 0) applyStateJson (a[0].toString()); done ("ok"); })
               .withNativeFunction ("hfPoll", [this] (const Array<var>&, WebBrowserComponent::NativeFunctionCompletion done)
                                    { done (buildPollJson()); })
               .withResourceProvider ([this] (const String& url) { return getResource (url); }))
{
    addAndMakeVisible (web);
    setResizable (true, true);
    setResizeLimits (550, 310, 2200, 1240);
    getConstrainer()->setFixedAspectRatio (1100.0 / 620.0);
    setSize (1100, 620);
    web.goToURL (WebBrowserComponent::getResourceProviderRoot());
}

void HardflipEditor::paint (Graphics& g) { g.fillAll (Colour (0xff1a1816)); }
void HardflipEditor::resized() { web.setBounds (getLocalBounds()); }

//==============================================================================
std::optional<WebBrowserComponent::Resource> HardflipEditor::getResource (const String& urlIn)
{
    auto url = urlIn.upToFirstOccurrenceOf ("?", false, false);
    const String name = (url == "/" || url.isEmpty()) ? String ("index.html") : url.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* id = BinaryData::namedResourceList[i];
        if (String (BinaryData::getNamedResourceOriginalFilename (id)) == name)
        {
            int size = 0;
            const char* data = BinaryData::getNamedResource (id, size);
            std::vector<std::byte> bytes ((size_t) size);
            std::memcpy (bytes.data(), data, (size_t) size);
            return WebBrowserComponent::Resource { std::move (bytes), mimeFor (name) };
        }
    }
    return std::nullopt;
}

//==============================================================================
// state: dB values like the parameters; the UI converts to linear itself
String HardflipEditor::buildStateJson()
{
    auto& apvts = proc.apvts;
    auto* o = new DynamicObject();
    o->setProperty ("drive",  (double) getParam (apvts, "drive"));
    o->setProperty ("thresh", (double) getParam (apvts, "thresh"));
    o->setProperty ("post",   (double) getParam (apvts, "post"));
    o->setProperty ("mix",    (double) getParam (apvts, "mix") * 0.01);
    o->setProperty ("hard",   getParam (apvts, "mode") > 0.5f);
    o->setProperty ("bypass", getParam (apvts, "bypass") > 0.5f);
    o->setProperty ("ui",     apvts.state.getProperty ("ui", "").toString());
    return JSON::toString (var (o), true);
}

void HardflipEditor::applyStateJson (const String& json)
{
    auto v = JSON::parse (json);
    if (! v.isObject()) return;
    auto& apvts = proc.apvts;
    auto num = [&v] (const char* k, float lo, float hi, float& out)
    {
        if (! v.hasProperty (k)) return false;
        const double d = (double) v[k];
        if (! std::isfinite (d)) return false;
        out = jlimit (lo, hi, (float) d);
        return true;
    };
    float f = 0.0f;
    if (num ("drive", 0.0f, 24.0f, f))     setParam (apvts, "drive", f);
    if (num ("thresh", -24.4f, 0.0f, f))   setParam (apvts, "thresh", f);
    if (num ("post", -12.0f, 0.0f, f))     setParam (apvts, "post", f);
    if (num ("mix", 0.0f, 1.0f, f))        setParam (apvts, "mix", f * 100.0f);
    if (v.hasProperty ("hard"))            setParam (apvts, "mode", (bool) v["hard"] ? 1.0f : 0.0f);
    if (v.hasProperty ("bypass"))          setParam (apvts, "bypass", (bool) v["bypass"] ? 1.0f : 0.0f);
    if (v.hasProperty ("ui"))              apvts.state.setProperty ("ui", v["ui"].toString(), nullptr);
}

String HardflipEditor::buildPollJson()
{
    std::array<float, HardflipProcessor::kScope> in {}, out {};
    proc.getScope (in, out);

    String s;
    s.preallocateBytes (12000);
    auto arr = [&s] (const char* key, const float* d, int n)
    {
        s << "\"" << key << "\":[";
        for (int i = 0; i < n; i += 2) { if (i) s << ","; s << String (d[i], 4); }   // 512 points
        s << "]";
    };
    s << "{\"in\":" << String (proc.takeInPeak(), 5) << ",\"out\":" << String (proc.takeOutPeak(), 5) << ",";
    arr ("si", in.data(), HardflipProcessor::kScope);  s << ",";
    arr ("so", out.data(), HardflipProcessor::kScope);
    s << "}";
    return s;
}
