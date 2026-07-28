#include "PluginEditor.h"

namespace
{
using namespace AutomixTheme;

constexpr int headerHeight  = 92;
constexpr int footerHeight  = 26;
constexpr int scaleWidth    = 38;
constexpr int minStripWidth = 56;
constexpr int maxStripWidth = 150;

/// Comfortable strip width used to pick the initial window size, so a two-input
/// device does not open a window built for thirty-two.
constexpr int nominalStripWidth = 74;

/// Frames of the 30 Hz timer with no new audio block before the meters are
/// treated as stale. Two frames of slack absorbs a late block without the
/// display flickering to silence.
constexpr int idleFramesBeforeStale = 3;

void styleKnob (juce::Slider& s, juce::Component& parent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    s.setColour (juce::Slider::rotarySliderFillColourId, colour (micOpen));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, colour (meterWell));
    s.setColour (juce::Slider::thumbColourId, colour (textPrimary));
    s.setColour (juce::Slider::textBoxTextColourId, colour (textDim));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    parent.addAndMakeVisible (s);
}

void drawKnobLabel (juce::Graphics& g, const juce::Slider& s, const juce::String& text)
{
    g.setColour (colour (textFaint));
    g.setFont (labelFont (8.5f, true));
    g.drawText (text,
                juce::Rectangle<int> (s.getX(), s.getY() - 12, s.getWidth(), 10),
                juce::Justification::centred,
                false);
}

/// Small all-caps caption used to name a zone of the panel.
void drawCaption (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    g.setColour (colour (textFaint));
    g.setFont (labelFont (8.5f, true));
    g.drawText (text, area, juce::Justification::centredLeft, false);
}
} // namespace

void MeterScale::paint (juce::Graphics& g)
{
    // Same vertical zones as a strip, so the ticks line up with the segments.
    const auto layout = StripLayout::compute (getLocalBounds().reduced (0, 5));
    const auto meter = layout.meter.reduced (0, 2);

    g.setColour (colour (textFaint));
    g.setFont (monoFont (8.5f));

    for (float db = 0.0f; db >= meterFloorDb; db -= 10.0f)
    {
        const int y = meter.getBottom()
                      - juce::roundToInt (dbToNorm (db) * (float) meter.getHeight());

        g.setColour (colour (hairline));
        g.fillRect (getWidth() - 6, y, 5, 1);

        g.setColour (colour (textFaint));
        g.drawText (juce::String (juce::roundToInt (db)),
                    juce::Rectangle<int> (0, y - 6, getWidth() - 8, 12),
                    juce::Justification::centredRight,
                    false);
    }

    g.setColour (colour (textFaint));
    g.setFont (labelFont (7.5f, true));
    g.drawText ("dBFS", layout.readout, juce::Justification::centredRight, false);
}

AutomixEditor::AutomixEditor (AutomixProcessor& p)
    : AudioProcessorEditor (p), processor_ (p)
{
    // The header needs roughly 900px before its groups start colliding, so that
    // is the floor regardless of channel count.
    const int channels = juce::jlimit (1, AutomixProcessor::kMaxChannels,
                                       processor_.getTotalNumInputChannels());
    const int wanted = scaleWidth + channels * nominalStripWidth + 24;
    setSize (juce::jlimit (940, 1680, wanted), 660);
    setResizable (true, true);
    setResizeLimits (900, 500, 2600, 1400);

    addAndMakeVisible (scale_);

    stripViewport_.setViewedComponent (&stripContainer_, false);
    stripViewport_.setScrollBarsShown (false, true);
    stripViewport_.setScrollBarThickness (10);
    addAndMakeVisible (stripViewport_);

    bypassButton_.setClickingTogglesState (true);
    bypassButton_.setColour (juce::TextButton::buttonColourId, colour (buttonIdle));
    bypassButton_.setColour (juce::TextButton::buttonOnColourId, colour (muteOn));
    bypassButton_.setColour (juce::TextButton::textColourOffId, colour (textDim));
    bypassButton_.setColour (juce::TextButton::textColourOnId, colour (0xff0b0c0d));
    bypassButton_.setTooltip ("Bypass the automixer. Crossfades over 15 ms, so it is safe mid-show.");
    addAndMakeVisible (bypassButton_);

    nomAttenButton_.setClickingTogglesState (true);
    nomAttenButton_.setColour (juce::TextButton::buttonColourId, colour (buttonIdle));
    nomAttenButton_.setColour (juce::TextButton::buttonOnColourId, colour (gainReduce));
    nomAttenButton_.setColour (juce::TextButton::textColourOffId, colour (textDim));
    nomAttenButton_.setColour (juce::TextButton::textColourOnId, colour (0xff0b0c0d));
    nomAttenButton_.setTooltip ("Extra -10*log10(NOM) attenuation on top of gain sharing. "
                                "Off by default: gain sharing already holds the loop gain "
                                "constant, so this trades level for feedback margin.");
    addAndMakeVisible (nomAttenButton_);

    styleKnob (attackSlider_, *this);
    styleKnob (releaseSlider_, *this);
    styleKnob (holdSlider_, *this);
    attackSlider_.setTooltip ("How fast a channel's gain rises when someone starts talking.");
    releaseSlider_.setTooltip ("How fast a channel's gain falls when they stop.");
    holdSlider_.setTooltip ("How long the last open mic stays open after everyone goes quiet.");

    auto& apvts = processor_.apvts;
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    bypassAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::globalBypassID, bypassButton_);
    nomAttenAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::nomAttenID, nomAttenButton_);
    attackAttach_ = std::make_unique<SliderAttach> (
        apvts, AutomixParams::attackMsID, attackSlider_);
    releaseAttach_ = std::make_unique<SliderAttach> (
        apvts, AutomixParams::releaseMsID, releaseSlider_);
    holdAttach_ = std::make_unique<SliderAttach> (
        apvts, AutomixParams::holdMsID, holdSlider_);

    rebuildStrips (juce::jmax (1, processor_.getTotalNumInputChannels()));

    lastBlockCounter_ = processor_.getBlockCounter();
    startTimerHz (30);
}

AutomixEditor::~AutomixEditor()
{
    stopTimer();
}

void AutomixEditor::rebuildStrips (int numChannels)
{
    numChannels = juce::jlimit (1, AutomixProcessor::kMaxChannels, numChannels);
    if (numChannels == stripCount_)
        return;

    strips_.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        stripContainer_.addAndMakeVisible (strips_.add (new ChannelStrip (processor_, ch)));

    stripCount_ = numChannels;
    layoutStrips();
}

void AutomixEditor::timerCallback()
{
    // A channel count change means the host renegotiated the bus layout.
    rebuildStrips (juce::jmax (1, processor_.getTotalNumInputChannels()));

    const uint32_t counter = processor_.getBlockCounter();
    if (counter != lastBlockCounter_)
    {
        lastBlockCounter_ = counter;
        idleFrames_ = 0;
    }
    else if (idleFrames_ < idleFramesBeforeStale)
    {
        ++idleFrames_;
    }
    const bool running = idleFrames_ < idleFramesBeforeStale;

    int openCount = 0;
    for (int ch = 0; ch < strips_.size(); ++ch)
    {
        const auto data = processor_.getChannelMeterData (ch);
        strips_[ch]->updateMeters (data, running);
        if (running && data.isActive)
            ++openCount;
    }

    const auto global = processor_.getGlobalMeterData();
    const bool statusChanged = openCount != openChannelCount_
                               || running != audioRunning_
                               || std::abs (global.nomCount - globalMeter_.nomCount) > 0.05f
                               || std::abs (global.nomAttenuationDb
                                            - globalMeter_.nomAttenuationDb) > 0.05f;

    openChannelCount_ = openCount;
    globalMeter_ = global;
    audioRunning_ = running;

    // The strips repaint themselves; only the header's numbers need us.
    if (statusChanged)
    {
        repaint (statusBounds_);
        repaint (footerBounds_);
    }
}

void AutomixEditor::resized()
{
    auto area = getLocalBounds();
    headerBounds_ = area.removeFromTop (headerHeight);
    footerBounds_ = area.removeFromBottom (footerHeight);

    scale_.setBounds (area.removeFromLeft (scaleWidth));
    stripViewport_.setBounds (area);

    auto header = headerBounds_.reduced (16, 0);
    header.removeFromLeft (168);                        // wordmark, painted
    header.removeFromLeft (8);

    bypassButton_.setBounds (header.removeFromLeft (92).withSizeKeepingCentre (92, 40));
    header.removeFromLeft (20);
    statusBounds_ = header.removeFromLeft (210);

    controlBounds_ = header.removeFromRight (juce::jmin (header.getWidth(), 320));
    auto controls = controlBounds_.reduced (8, 16);

    nomAttenButton_.setBounds (controls.removeFromRight (76).withSizeKeepingCentre (76, 26));
    controls.removeFromRight (12);

    const int knobWidth = juce::jmax (1, controls.getWidth() / 3);
    attackSlider_.setBounds (controls.removeFromLeft (knobWidth).reduced (2, 0));
    releaseSlider_.setBounds (controls.removeFromLeft (knobWidth).reduced (2, 0));
    holdSlider_.setBounds (controls.reduced (2, 0));

    layoutStrips();
}

void AutomixEditor::layoutStrips()
{
    if (strips_.isEmpty())
        return;

    const int available = juce::jmax (0, stripViewport_.getWidth());
    const int fitted = available / strips_.size();
    const int stripWidth = juce::jlimit (minStripWidth, maxStripWidth, fitted);
    const int totalWidth = stripWidth * strips_.size();

    // Only reserve room for the scrollbar when the bay actually overflows.
    const int viewportHeight = stripViewport_.getHeight();
    const int containerHeight = totalWidth > available
                                    ? juce::jmax (0, viewportHeight - 10)
                                    : viewportHeight;

    stripContainer_.setBounds (0, 0, juce::jmax (totalWidth, available), containerHeight);

    for (int i = 0; i < strips_.size(); ++i)
        strips_[i]->setBounds (i * stripWidth, 0, stripWidth, containerHeight);
}

void AutomixEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour (background));

    // The bay sits in a well so the strips read as inserted into a frame.
    auto bay = getLocalBounds()
                   .withTrimmedTop (headerHeight)
                   .withTrimmedBottom (footerHeight);
    g.setColour (colour (0xff0e1011));
    g.fillRect (bay);

    paintHeader (g);
    paintFooter (g);
}

void AutomixEditor::paintHeader (juce::Graphics& g)
{
    auto area = headerBounds_;

    g.setColour (colour (headerFill));
    g.fillRect (area);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (area.getX(), area.getBottom() - 2, area.getWidth(), 2);
    g.setColour (colour (micOpen).withAlpha (0.5f));
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);

    // ---- Identity ----
    auto wordmark = area.reduced (16, 0).removeFromLeft (168);
    auto title = wordmark.removeFromTop (area.getHeight() / 2 + 10).withTrimmedTop (22);
    g.setColour (colour (textPrimary));
    g.setFont (labelFont (24.0f, true));
    g.drawText ("AUTOMIX", title, juce::Justification::topLeft, false);

    g.setColour (colour (textFaint));
    g.setFont (monoFont (8.5f));
    g.drawText ("GAIN-SHARING AUTOMIXER", wordmark, juce::Justification::topLeft, false);
    g.drawText ("v" AUTOMIX_VERSION, wordmark.withTrimmedTop (11),
                juce::Justification::topLeft, false);

    // ---- Status: the two numbers an operator actually watches ----
    auto status = statusBounds_;
    drawWell (g, status.reduced (0, 18), headerWell);

    auto inner = status.reduced (10, 22);
    auto openRow = inner.removeFromTop (inner.getHeight() / 2 + 4);

    drawCaption (g, openRow.removeFromTop (11), "OPEN MICS");

    const bool anyOpen = openChannelCount_ > 0;
    g.setColour (anyOpen ? colour (micOpen) : colour (textFaint));
    g.setFont (monoFont (28.0f, true));
    auto bigNumber = openRow.removeFromLeft (48);
    g.drawText (juce::String (openChannelCount_).paddedLeft ('0', 2),
                bigNumber, juce::Justification::centredLeft, false);

    g.setColour (colour (textDim));
    g.setFont (monoFont (12.0f));
    g.drawText ("of " + juce::String (stripCount_), openRow,
                juce::Justification::centredLeft, false);

    drawCaption (g, inner.removeFromLeft (30), "NOM");
    g.setColour (colour (textPrimary));
    g.setFont (monoFont (11.0f));
    g.drawText (juce::String (globalMeter_.nomCount, 1) + "    "
                    + juce::String (globalMeter_.nomAttenuationDb, 1) + " dB",
                inner, juce::Justification::centredLeft, false);

    // ---- Control group ----
    // No group caption here: the three knob labels sit on the same line and a
    // "RESPONSE" heading collided with them.
    drawWell (g, controlBounds_.reduced (0, 12), headerWell);

    drawKnobLabel (g, attackSlider_, "ATTACK");
    drawKnobLabel (g, releaseSlider_, "RELEASE");
    drawKnobLabel (g, holdSlider_, "HOLD");
}

void AutomixEditor::paintFooter (juce::Graphics& g)
{
    auto area = footerBounds_;
    g.setColour (colour (headerFill));
    g.fillRect (area);
    g.setColour (colour (hairline));
    g.fillRect (area.getX(), area.getY(), area.getWidth(), 1);

    auto row = area.reduced (16, 0);

    // The status dot is drawn rather than typed. A literal bullet glyph depends
    // on the source file's encoding surviving the toolchain, and it does not
    // reliably: it rendered as "å".
    const auto statusColour = audioRunning_ ? colour (levelLow) : colour (textFaint);
    auto dot = row.removeFromLeft (14).withSizeKeepingCentre (7, 7).toFloat();
    g.setColour (statusColour);
    if (audioRunning_)
        g.fillEllipse (dot);
    else
        g.drawEllipse (dot, 1.0f);

    g.setFont (labelFont (9.0f, true));
    g.setColour (statusColour);
    g.drawText (audioRunning_ ? "AUDIO" : "NO AUDIO",
                row.removeFromLeft (76), juce::Justification::centredLeft, false);

    g.setColour (colour (textFaint));
    g.setFont (labelFont (9.0f));
    g.drawText (juce::String (stripCount_) + " channels", row.removeFromLeft (110),
                juce::Justification::centredLeft, false);

    // Legend: without it the two ladders in every strip are unexplained.
    auto legend = row.removeFromRight (300);
    g.setFont (labelFont (8.5f, true));

    auto swatch = [&g, &legend] (juce::uint32 c, const juce::String& text)
    {
        auto box = legend.removeFromLeft (12).withSizeKeepingCentre (7, 7);
        g.setColour (colour (c));
        g.fillRect (box);
        g.setColour (colour (textFaint));
        g.drawText (text, legend.removeFromLeft (juce::jmin (86, legend.getWidth())),
                    juce::Justification::centredLeft, false);
    };

    swatch (levelLow, "INPUT LEVEL");
    swatch (gainReduce, "GAIN REDUCTION");
    swatch (micOpen, "OPEN");
}
