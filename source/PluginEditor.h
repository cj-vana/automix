#pragma once

#include "PluginProcessor.h"

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

    AutomixProcessor& processor_;

    AutomixProcessor::ChannelMeterData channelMeters_[AutomixProcessor::kMaxChannels];
    AutomixProcessor::GlobalMeterData  globalMeter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomixEditor)
};
