#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Design tokens for the AutoMix interface.
///
/// These come from design option 1a, "Broadcast console: dense, data-forward,
/// dark", in the AutoMix editor redesign project. The panel is read from a few
/// feet away, mid-show, by someone doing something else, so the palette is
/// almost entirely neutral and spends its two chromatic colours carefully:
/// lime for signal and open state, coral for gain reduction and attenuation.
/// Nothing else is coloured, which is what lets a single open mic register in
/// peripheral vision.
namespace AutomixTheme
{

// ---- Surfaces ----
inline constexpr juce::uint32 pageBg  = 0xff0a0b0d;
inline constexpr juce::uint32 panelBg = 0xff101317;
inline constexpr juce::uint32 wellBg  = 0xff080a0c;
inline constexpr juce::uint32 chipBg  = 0xff191d22;

// ---- Text ramp ----
inline constexpr juce::uint32 textBright   = 0xfff2f6f8;
inline constexpr juce::uint32 textPrimary  = 0xffdde3e8;
inline constexpr juce::uint32 textSecond   = 0xffb6bec5;
inline constexpr juce::uint32 textDim      = 0xff8a949c;
inline constexpr juce::uint32 textFaint    = 0xff68737c;
inline constexpr juce::uint32 textFainter  = 0xff5a646e;
inline constexpr juce::uint32 textFaintest = 0xff4e5760;
inline constexpr juce::uint32 textGhost    = 0xff3e464e;

// ---- The two chromatic colours ----
/// Signal, open mics, anything the operator should look at first.
inline constexpr juce::uint32 accent = 0xffdfff4f;
/// Gain reduction and attenuation: the automixer taking something away.
inline constexpr juce::uint32 warn = 0xffff8a5c;

// ---- Borders ----
inline juce::Colour border()       { return juce::Colours::white.withAlpha (0.07f); }
inline juce::Colour borderHeader() { return juce::Colours::white.withAlpha (0.08f); }
inline juce::Colour borderWell()   { return juce::Colours::white.withAlpha (0.06f); }
inline juce::Colour chipOutline()  { return juce::Colours::white.withAlpha (0.10f); }
inline juce::Colour ghostFill()    { return juce::Colours::white.withAlpha (0.05f); }
inline juce::Colour badgeFill()    { return juce::Colours::white.withAlpha (0.07f); }

inline juce::Colour colour (juce::uint32 argb) { return juce::Colour (argb); }

// ---- Metrics ----
inline constexpr int headerHeight = 46;
inline constexpr int panelRadius  = 4;
inline constexpr int bodyPadX     = 16;
inline constexpr int bodyPadTop   = 12;
inline constexpr int bodyPadBot   = 14;
inline constexpr int panelGap     = 11;
inline constexpr int topRowHeight = 142;
inline constexpr int historyHeight = 146;
inline constexpr int statsWidth   = 236;
inline constexpr int responseWidth = 322;

// ---- Meter scale ----
inline constexpr float meterFloorDb       = -60.0f;
inline constexpr float meterCeilingDb     = 0.0f;
inline constexpr float maxGainReductionDb = 40.0f;

/// Position of a dB value on a meter, 0.0 at the floor and 1.0 at the top.
inline float dbToNorm (float db)
{
    return juce::jlimit (0.0f, 1.0f,
                         (db - meterFloorDb) / (meterCeilingDb - meterFloorDb));
}

/// Rounded panel with a hairline border, the repeating container of this design.
inline void drawPanel (juce::Graphics& g,
                       juce::Rectangle<int> area,
                       juce::uint32 fill = panelBg)
{
    g.setColour (colour (fill));
    g.fillRoundedRectangle (area.toFloat(), (float) panelRadius);
    g.setColour (border());
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), (float) panelRadius, 1.0f);
}

/// Recessed track that meters and bars sit in.
inline void drawWell (juce::Graphics& g, juce::Rectangle<int> area, float radius = 1.0f)
{
    g.setColour (colour (wellBg));
    g.fillRoundedRectangle (area.toFloat(), radius);
}

} // namespace AutomixTheme
