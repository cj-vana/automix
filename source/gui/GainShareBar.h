#pragma once

#include "AutomixFonts.h"
#include "AutomixTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

/// The gain-share distribution strip: one horizontal bar split into a segment
/// per open channel, each sized to that channel's share of the mix.
///
/// This is the clearest statement of what a gain-sharing automixer does. The
/// shares are normalised to sum to 1.0, which is what holds the loop gain
/// constant however many mics are open, so the bar is always full and only its
/// division changes. Watching one segment swell as someone leans in, and the
/// rest give way to make room, is the algorithm made visible.
class GainShareBar : public juce::Component
{
public:
    struct Segment
    {
        int   channel = 0;
        float share   = 0.0f;
    };

    GainShareBar() = default;

    void paint (juce::Graphics&) override;

    /// Replace the segment set. Repaints only when the division actually moved.
    void setSegments (std::vector<Segment> segments);

private:
    std::vector<Segment> segments_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainShareBar)
};
