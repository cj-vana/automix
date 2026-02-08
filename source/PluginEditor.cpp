#include "PluginEditor.h"

AutomixEditor::AutomixEditor (AutomixProcessor& p)
    : AudioProcessorEditor (p), processor_ (p)
{
    setSize (1200, 700);
    setResizable (true, true);
    setResizeLimits (800, 400, 2400, 1400);
}

AutomixEditor::~AutomixEditor() = default;

void AutomixEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    g.setColour (juce::Colour (0xffdfe6e9));
    g.setFont (24.0f);
    g.drawText ("AutoMix", getLocalBounds().removeFromTop (50),
                juce::Justification::centred, true);

    g.setFont (14.0f);
    g.drawText ("Dugan-Style Automixer — Phase 0 Scaffold",
                getLocalBounds().reduced (20),
                juce::Justification::centred, true);
}

void AutomixEditor::resized()
{
}
