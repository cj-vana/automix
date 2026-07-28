#include "AutomixFonts.h"
#include "AutomixTheme.h"
#include <AutoMixBinary.h>

namespace
{
juce::Typeface::Ptr load (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}
} // namespace

AutomixFonts::AutomixFonts()
    : archivoMedium_ (load (AutoMixBinary::ArchivoMedium_ttf,
                            AutoMixBinary::ArchivoMedium_ttfSize)),
      archivoSemiBold_ (load (AutoMixBinary::ArchivoSemiBold_ttf,
                              AutoMixBinary::ArchivoSemiBold_ttfSize)),
      archivoBold_ (load (AutoMixBinary::ArchivoBold_ttf,
                          AutoMixBinary::ArchivoBold_ttfSize)),
      plexMedium_ (load (AutoMixBinary::IBMPlexMonoMedium_ttf,
                         AutoMixBinary::IBMPlexMonoMedium_ttfSize)),
      plexSemiBold_ (load (AutoMixBinary::IBMPlexMonoSemiBold_ttf,
                           AutoMixBinary::IBMPlexMonoSemiBold_ttfSize))
{
}

juce::Typeface::Ptr AutomixFonts::pickSans (Weight w) const
{
    switch (w)
    {
        case Weight::bold:     return archivoBold_;
        case Weight::semiBold: return archivoSemiBold_;
        case Weight::medium:   break;
    }
    return archivoMedium_;
}

juce::Typeface::Ptr AutomixFonts::pickMono (Weight w) const
{
    switch (w)
    {
        case Weight::bold:
        case Weight::semiBold: return plexSemiBold_;
        case Weight::medium:   break;
    }
    return plexMedium_;
}

juce::Font AutomixFonts::sans (float height, Weight weight, float tracking) const
{
    return juce::Font (juce::FontOptions {}
                           .withTypeface (pickSans (weight))
                           .withHeight (height)
                           .withKerningFactor (tracking));
}

juce::Font AutomixFonts::mono (float height, Weight weight, float tracking) const
{
    return juce::Font (juce::FontOptions {}
                           .withTypeface (pickMono (weight))
                           .withHeight (height)
                           .withKerningFactor (tracking));
}

void drawLabel (juce::Graphics& g,
                const juce::String& text,
                juce::Rectangle<int> area,
                juce::uint32 colour,
                float height,
                float tracking,
                juce::Justification justification)
{
    g.setColour (AutomixTheme::colour (colour));
    g.setFont (fonts().sans (height, AutomixFonts::Weight::semiBold, tracking));
    g.drawText (text, area, justification, false);
}

void drawValue (juce::Graphics& g,
                const juce::String& text,
                juce::Rectangle<int> area,
                juce::uint32 colour,
                float height,
                AutomixFonts::Weight weight,
                juce::Justification justification)
{
    g.setColour (AutomixTheme::colour (colour));
    g.setFont (fonts().mono (height, weight));
    g.drawText (text, area, justification, false);
}
