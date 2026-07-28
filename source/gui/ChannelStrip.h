#pragma once

#include "../PluginProcessor.h"
#include "AutomixTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

/// Vertical zones of a channel strip.
///
/// Shared so the dB scale beside the bay lands on the same pixels as the meters
/// it labels. Computing it in two places is how a scale drifts one row off and
/// quietly lies about the levels.
struct StripLayout
{
    juce::Rectangle<int> cap;         // channel number band
    juce::Rectangle<int> lamp;        // open indicator
    juce::Rectangle<int> meter;       // level + gain reduction well
    juce::Rectangle<int> readout;     // dB text
    juce::Rectangle<int> weight;      // fader
    juce::Rectangle<int> weightLabel;
    juce::Rectangle<int> buttons;     // mute / solo / bypass

    static StripLayout compute (juce::Rectangle<int> bounds);
};

/// One vertical channel strip: open lamp, segmented level meter with a
/// gain-reduction ladder beside it, dB readout, weight fader, and M/S/B.
///
/// Meter values arrive from the audio thread via the processor's atomics. The
/// strip repaints only the region those values affect, so a 32-channel bay at
/// 30 Hz does not repaint the whole window sixty times a second.
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (AutomixProcessor& processor, int channelIndex);
    ~ChannelStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /// Push a new meter frame. `audioRunning` false means the host has stopped
    /// calling processBlock, which is not the same as the input being silent —
    /// the meters fall to the floor instead of freezing at their last reading.
    void updateMeters (const AutomixProcessor::ChannelMeterData& data, bool audioRunning);

private:
    void paintCap (juce::Graphics&);
    void paintLamp (juce::Graphics&);
    void paintMeter (juce::Graphics&);
    void paintReadout (juce::Graphics&);

    AutomixProcessor& processor_;
    const int channel_;

    juce::Slider weightSlider_;
    juce::TextButton muteButton_ { "M" };
    juce::TextButton soloButton_ { "S" };
    juce::TextButton bypassButton_ { "B" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> weightAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_;

    // Displayed values, decayed toward the incoming reading so the meter has
    // ballistics instead of flickering between blocks.
    float displayInputDb_ = AutomixTheme::meterFloorDb;
    float displayGainDb_  = 0.0f;
    float displayFloorDb_ = AutomixTheme::meterFloorDb;
    float peakHoldDb_     = AutomixTheme::meterFloorDb;
    int   peakHoldFrames_ = 0;
    bool  isOpen_         = false;

    StripLayout layout_;
    juce::Rectangle<int> dynamicBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};
