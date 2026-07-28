#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// The design's two typefaces, compiled into the binary.
///
/// Archivo carries the labels; IBM Plex Mono carries every number. That split
/// is not decoration. Tabular figures keep a readout from shifting sideways as
/// digits change, which is the difference between a legible moving number and a
/// twitching one, and the design leans on it for the whole stat row.
///
/// Loaded once through a SharedResourcePointer so every editor instance shares
/// one set of typefaces rather than re-parsing the TTFs per window.
class AutomixFonts
{
public:
    AutomixFonts();

    enum class Weight { medium, semiBold, bold };

    /// Label face. `tracking` is in em, matching the design's letter-spacing.
    juce::Font sans (float height, Weight weight = Weight::medium, float tracking = 0.0f) const;

    /// Numeric face.
    juce::Font mono (float height, Weight weight = Weight::medium, float tracking = 0.0f) const;

private:
    juce::Typeface::Ptr archivoMedium_, archivoSemiBold_, archivoBold_;
    juce::Typeface::Ptr plexMedium_, plexSemiBold_;

    juce::Typeface::Ptr pickSans (Weight) const;
    juce::Typeface::Ptr pickMono (Weight) const;
};

/// Shorthand for the shared instance.
inline const AutomixFonts& fonts()
{
    static juce::SharedResourcePointer<AutomixFonts> instance;
    return *instance;
}

/// All-caps label in the design's tracked style, drawn in one call because the
/// panel is mostly made of these.
void drawLabel (juce::Graphics&,
                const juce::String& text,
                juce::Rectangle<int> area,
                juce::uint32 colour,
                float height,
                float tracking,
                juce::Justification = juce::Justification::centredLeft);

/// Middle dot, the design's separator.
///
/// Built from explicit UTF-8 bytes rather than written literally. A raw "·" in
/// a narrow string literal gets re-interpreted as Latin-1 somewhere between the
/// source file and the glyph, and renders as "Å·".
inline juce::String middot()
{
    return juce::String::fromUTF8 ("\xc2\xb7");
}

/// Numeric readout in IBM Plex Mono.
void drawValue (juce::Graphics&,
                const juce::String& text,
                juce::Rectangle<int> area,
                juce::uint32 colour,
                float height,
                AutomixFonts::Weight = AutomixFonts::Weight::semiBold,
                juce::Justification = juce::Justification::centredLeft);
