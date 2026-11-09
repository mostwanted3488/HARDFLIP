#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// Hosts the HARDFLIP interface (HTML/JS embedded in the binary) in a native web view and exposes
// the processor to it through three native functions: hfGetState, hfSetState and hfPoll.
class HardflipEditor : public juce::AudioProcessorEditor
{
public:
    explicit HardflipEditor (HardflipProcessor&);
    ~HardflipEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Browser : juce::WebBrowserComponent
    {
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& url) override;   // keep the single-page app pinned
    };

    juce::String buildStateJson();
    void applyStateJson (const juce::String&);
    juce::String buildPollJson();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    HardflipProcessor& proc;
    Browser web;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardflipEditor)
};
