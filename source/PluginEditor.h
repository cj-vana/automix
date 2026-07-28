#pragma once

#include "PluginProcessor.h"
#include "gui/AutomixTheme.h"
#include "gui/ChannelStrip.h"

/// dB scale beside the meter bay.
///
/// Ticks are derived from the same StripLayout the strips use, so the labels sit
/// on the rows they describe instead of near them.
class MeterScale : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
};

/// The plugin window: status header, scrolling bay of channel strips against a
/// dB scale, and a footer carrying the things you only look at once.
class AutomixEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit AutomixEditor (AutomixProcessor&);
    ~AutomixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildStrips (int numChannels);
    void paintHeader (juce::Graphics&);
    void paintFooter (juce::Graphics&);
    void layoutStrips();

    AutomixProcessor& processor_;

    MeterScale scale_;
    juce::Viewport stripViewport_;
    juce::Component stripContainer_;
    juce::OwnedArray<ChannelStrip> strips_;

    juce::TextButton bypassButton_ { "BYPASS" };
    juce::TextButton nomAttenButton_ { "NOM ATTEN" };
    juce::Slider attackSlider_;
    juce::Slider releaseSlider_;
    juce::Slider holdSlider_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> nomAttenAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttach_;

    juce::TooltipWindow tooltips_ { this, 700 };

    AutomixProcessor::GlobalMeterData globalMeter_;
    int      openChannelCount_ = 0;
    int      stripCount_       = 0;
    uint32_t lastBlockCounter_ = 0;
    int      idleFrames_       = 0;
    bool     audioRunning_     = false;

    juce::Rectangle<int> headerBounds_;
    juce::Rectangle<int> statusBounds_;
    juce::Rectangle<int> controlBounds_;
    juce::Rectangle<int> footerBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomixEditor)
};
