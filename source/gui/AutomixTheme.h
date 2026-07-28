#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Visual constants for the AutoMix interface.
///
/// The panel is styled after a broadcast automixer front panel rather than a
/// general-purpose plugin GUI: the operator is reading it from a few feet away,
/// under stage lighting, while doing something else. That drives the choices.
///
/// - Amber marks an open mic. It is the convention on automixer hardware and it
///   survives peripheral vision better than a colour shift in the meter.
/// - Level meters use the green/amber/red ladder because an operator already
///   knows how to read one, and they are drawn as discrete segments rather than
///   a continuous fill — a segmented ladder is easier to read at a glance and
///   is what the hardware this replaces looks like.
/// - Gain reduction gets a deliberately different hue so it cannot be confused
///   with level, and dims when a channel is closed so thirty-two idle strips do
///   not shout.
/// - Numerics are monospaced. Tabular figures keep a readout from shifting
///   sideways as digits change, which is what makes a moving number legible.
namespace AutomixTheme
{

// ---- Surfaces ----
inline constexpr juce::uint32 background    = 0xff0b0c0d;
inline constexpr juce::uint32 headerFill    = 0xff17191c;
inline constexpr juce::uint32 headerWell    = 0xff101214;
inline constexpr juce::uint32 stripFill     = 0xff141618;
inline constexpr juce::uint32 stripFillAlt  = 0xff181a1d;
inline constexpr juce::uint32 stripCap      = 0xff1e2125;
inline constexpr juce::uint32 meterWell     = 0xff070808;
inline constexpr juce::uint32 segmentOff    = 0xff191c1f;
inline constexpr juce::uint32 hairline      = 0xff26292d;
inline constexpr juce::uint32 bevel         = 0xff32373c;

// ---- Text ----
inline constexpr juce::uint32 textPrimary = 0xffe8e5e0;
inline constexpr juce::uint32 textDim     = 0xff8b9196;
inline constexpr juce::uint32 textFaint   = 0xff555b60;

// ---- State ----
inline constexpr juce::uint32 micOpen     = 0xffffa92b;
inline constexpr juce::uint32 micOpenSoft = 0x59ffa92b;
inline constexpr juce::uint32 lampOff     = 0xff1c1e20;

// ---- Gain reduction ----
inline constexpr juce::uint32 gainReduce    = 0xff5b8cb0;
inline constexpr juce::uint32 gainReduceDim = 0xff2c3d4a;

// ---- Level ladder ----
inline constexpr juce::uint32 levelLow  = 0xff4fa85a;
inline constexpr juce::uint32 levelMid  = 0xffe0a32e;
inline constexpr juce::uint32 levelHigh = 0xffd84a3f;
inline constexpr juce::uint32 noiseFloorTick = 0xff8a6f3a;

// ---- Buttons ----
inline constexpr juce::uint32 buttonIdle = 0xff23272a;
inline constexpr juce::uint32 muteOn     = 0xffd84a3f;
inline constexpr juce::uint32 soloOn     = 0xffe0c93a;
inline constexpr juce::uint32 bypassOn   = 0xff6e8ca8;

// ---- Meter scale ----
inline constexpr float meterFloorDb   = -60.0f;
inline constexpr float meterCeilingDb = 0.0f;
inline constexpr float maxGainReductionDb = 40.0f;

/// Segment pitch for the LED-style ladder, in pixels.
inline constexpr int segmentHeight = 3;
inline constexpr int segmentGap    = 1;

inline juce::Colour colour (juce::uint32 argb) { return juce::Colour (argb); }

/// Monospaced face for anything numeric. Menlo ships with macOS, so this
/// resolves without bundling a font; the fallback only matters off-platform.
inline juce::Font monoFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions {}
                           .withName ("Menlo")
                           .withHeight (height)
                           .withStyle (bold ? "Bold" : "Regular"));
}

inline juce::Font labelFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions {}
                           .withHeight (height)
                           .withStyle (bold ? "Bold" : "Regular"));
}

/// Position of a dB value on the meter, 0.0 at the floor and 1.0 at the top.
/// Linear in dB, which is how console meters are scaled.
inline float dbToNorm (float db)
{
    return juce::jlimit (0.0f,
                         1.0f,
                         (db - meterFloorDb) / (meterCeilingDb - meterFloorDb));
}

/// Ladder colour for a level. The thresholds are where an operator expects the
/// meter to start warning them, not evenly spaced thirds.
inline juce::Colour levelColour (float db)
{
    if (db >= -3.0f)  return colour (levelHigh);
    if (db >= -12.0f) return colour (levelMid);
    return colour (levelLow);
}

/// Sink a rectangle into the panel: dark well, dark top edge, light bottom
/// edge. One pixel each way is enough to read as recessed without becoming
/// a skeuomorphic bevel.
inline void drawWell (juce::Graphics& g, juce::Rectangle<int> area, juce::uint32 fill = meterWell)
{
    g.setColour (colour (fill));
    g.fillRect (area);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (area.getX(), area.getY(), area.getWidth(), 1);
    g.setColour (colour (bevel).withAlpha (0.35f));
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);
}

} // namespace AutomixTheme
