#include "PluginEditor.h"

AutomixEditor::AutomixEditor (AutomixProcessor& p)
    : AudioProcessorEditor (p), processor_ (p)
{
    setSize (1200, 700);
    setResizable (true, true);
    setResizeLimits (800, 400, 2400, 1400);

    startTimerHz (30);
}

AutomixEditor::~AutomixEditor()
{
    stopTimer();
}

void AutomixEditor::timerCallback()
{
    for (int ch = 0; ch < AutomixProcessor::kMaxChannels; ++ch)
        channelMeters_[ch] = processor_.getChannelMeterData (ch);

    globalMeter_ = processor_.getGlobalMeterData();

    repaint();
}

void AutomixEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    g.setColour (juce::Colour (0xffdfe6e9));
    g.setFont (24.0f);
    g.drawText ("AutoMix", getLocalBounds().removeFromTop (50),
                juce::Justification::centred, true);

    g.setFont (14.0f);
    auto info = juce::String::formatted (
        "Dugan-Style Automixer \xe2\x80\x94 v%s \xe2\x80\x94 %d active channels \xe2\x80\x94 NOM: %.1f",
        AUTOMIX_VERSION,
        processor_.getActiveChannelCount(),
        globalMeter_.nomCount);
    g.drawText (info, getLocalBounds().reduced (20),
                juce::Justification::centred, true);
}

void AutomixEditor::resized()
{
}
