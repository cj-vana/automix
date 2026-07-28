#pragma once

#include "../PluginProcessor.h"
#include "AutomixFonts.h"
#include "AutomixTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

/// Thirty seconds of who was open, as one lane per channel.
///
/// The mixer bay answers "what is happening now". This answers "what just
/// happened", which is the question after something went wrong: whether a mic
/// ever opened, whether it was chattering on and off, whether the automixer
/// held one channel the whole time. It needs no new DSP; the activity flag it
/// draws is the same one the strips already show.
class OpenMicHistory : public juce::Component
{
public:
    /// Buckets across the visible window. 30 s over 240 buckets is 125 ms each,
    /// which is fine enough to see a word and coarse enough to draw cheaply.
    static constexpr int numBuckets = 240;
    static constexpr int windowSeconds = 30;

    OpenMicHistory() = default;

    void paint (juce::Graphics&) override;

    /// Record one frame of activity. `openMask` bit i is set when channel i is
    /// open. Buckets latch: any activity within a bucket marks the whole bucket,
    /// so a brief word is never lost between samples.
    void pushFrame (const std::vector<bool>& open, int numChannels, double frameSeconds);

    void setChannelCount (int numChannels);

private:
    std::array<std::array<uint8_t, numBuckets>, AutomixProcessor::kMaxChannels> lanes_ {};
    int    numChannels_ = 0;
    int    writeBucket_ = 0;
    double bucketAccum_ = 0.0;
    bool   dirty_       = false;

    void paintColumn (juce::Graphics&, juce::Rectangle<int>, int firstChannel, int count);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenMicHistory)
};
