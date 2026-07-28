#include "GainShareBar.h"

namespace
{
using namespace AutomixTheme;

/// Alternating tints keep neighbouring segments apart without introducing a
/// second hue. The palette spends its colour on state, not on identity.
///
/// Both variants stay well clear of the well colour: dip the dim one too far
/// and every other segment reads as a gap in the bar rather than as a channel.
juce::Colour segmentColour (int index, float share)
{
    const float lift = juce::jlimit (0.68f, 1.0f, 0.62f + share * 1.6f);
    auto base = juce::Colour (accent).withMultipliedBrightness (index % 2 == 0 ? 1.0f : 0.88f);
    return base.withAlpha (lift);
}
} // namespace

void GainShareBar::setSegments (std::vector<Segment> segments)
{
    if (segments.size() == segments_.size())
    {
        bool same = true;
        for (size_t i = 0; i < segments.size(); ++i)
        {
            if (segments[i].channel != segments_[i].channel
                || std::abs (segments[i].share - segments_[i].share) > 0.002f)
            {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }

    segments_ = std::move (segments);
    repaint();
}

void GainShareBar::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();

    drawWell (g, area, 3.0f);
    g.setColour (borderWell());
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 3.0f, 1.0f);

    if (segments_.empty())
    {
        drawLabel (g, "NO OPEN MICS", area, textFaintest, 9.0f, 0.16f,
                   juce::Justification::centred);
        return;
    }

    auto inner = area.reduced (1);
    const float total = (float) inner.getWidth();
    float x = (float) inner.getX();

    for (size_t i = 0; i < segments_.size(); ++i)
    {
        const auto& s = segments_[i];
        const float w = total * juce::jlimit (0.0f, 1.0f, s.share);
        if (w < 0.5f)
            continue;

        juce::Rectangle<float> seg (x, (float) inner.getY(), w, (float) inner.getHeight());
        g.setColour (segmentColour ((int) i, s.share));
        g.fillRect (seg);

        // Hairline gap in the page colour, so segments read as separate blocks
        // rather than one gradient.
        g.setColour (colour (wellBg));
        g.fillRect (seg.removeFromRight (1.0f));

        // Only label a segment wide enough to hold the text without clipping.
        if (w > 34.0f)
        {
            g.setColour (colour (pageBg).withAlpha (0.85f));
            g.setFont (fonts().mono (9.0f, AutomixFonts::Weight::semiBold));
            g.drawText (juce::String (s.channel + 1) + " " + middot() + " "
                            + juce::String (juce::roundToInt (s.share * 100.0f)) + "%",
                        seg.toNearestInt(), juce::Justification::centred, false);
        }

        x += w;
    }
}
