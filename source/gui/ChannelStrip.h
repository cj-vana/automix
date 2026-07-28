#pragma once

#include "../PluginProcessor.h"
#include "AutomixFonts.h"
#include "AutomixTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

/// One channel column of the mixer bay, following design option 1a.
///
/// Top to bottom: channel number and a state badge, the channel label, twin
/// vertical bars (input level filling upward in lime, gain reduction filling
/// downward in coral), the applied gain, the weight, and S/M/B.
///
/// The two bars run side by side rather than overlaid so neither has to be read
/// through the other, and gain reduction hangs from the top the way it does on
/// a compressor, so "more bar" reads as "more taken away".
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (AutomixProcessor& processor, int channelIndex);
    ~ChannelStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /// Push a meter frame. `audioRunning` false means the host stopped calling
    /// processBlock, which is not the same as silence: the bars fall to the
    /// floor rather than freezing at their last reading.
    void updateMeters (const AutomixProcessor::ChannelMeterData&, bool audioRunning);

    bool isOpen() const { return isOpen_; }

    /// Share of the total mix this channel currently holds, 0.0 to 1.0. The
    /// gain-sharing gains are normalised to sum to one, so the applied linear
    /// gain is the share.
    float share() const { return share_; }

private:
    void paintHeaderRow (juce::Graphics&);
    void paintLabel (juce::Graphics&);
    void paintBars (juce::Graphics&);
    void paintReadouts (juce::Graphics&);

    AutomixProcessor& processor_;
    const int channel_;

    juce::Slider weightSlider_;
    juce::TextButton soloButton_ { "S" };
    juce::TextButton muteButton_ { "M" };
    juce::TextButton bypassButton_ { "B" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> weightAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_;

    // Displayed values, eased toward the incoming reading so the bars have
    // ballistics instead of flickering block to block.
    float displayInputDb_ = AutomixTheme::meterFloorDb;
    float displayGainDb_  = 0.0f;
    float peakHoldDb_     = AutomixTheme::meterFloorDb;
    int   peakHoldFrames_ = 0;
    float share_          = 0.0f;
    bool  isOpen_         = false;

    juce::Rectangle<int> headerRow_, labelRow_, barsArea_, gainRow_, weightRow_, buttonRow_;
    juce::Rectangle<int> dynamicBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};
