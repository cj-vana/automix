#include "PluginEditor.h"

namespace
{
using namespace AutomixTheme;

constexpr int minStripWidth = 46;
constexpr int maxStripWidth = 112;
constexpr int nominalStrip  = 66;

/// Frames of the 30 Hz timer with no new audio block before the meters are
/// treated as stale. Two frames of slack absorbs a late block without the
/// display flickering to silence.
constexpr int idleFramesBeforeStale = 3;

/// Height below which the secondary rows are dropped. The mixer bay is the one
/// thing that must survive at 800×400.
constexpr int minHeightForTopRow  = 470;
constexpr int minHeightForHistory = 660;

void styleChip (juce::TextButton& b, juce::uint32 onColour, juce::uint32 onText)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, colour (chipBg));
    b.setColour (juce::TextButton::buttonOnColourId, colour (onColour));
    b.setColour (juce::TextButton::textColourOffId, colour (textDim));
    b.setColour (juce::TextButton::textColourOnId, colour (onText));
}

void styleKnob (juce::Slider& s, juce::Component& parent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, colour (accent));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, colour (wellBg));
    s.setColour (juce::Slider::thumbColourId, colour (textPrimary));
    parent.addAndMakeVisible (s);
}
} // namespace

AutomixEditor::AutomixEditor (AutomixProcessor& p)
    : AudioProcessorEditor (p), processor_ (p)
{
    const int channels = juce::jlimit (1, AutomixProcessor::kMaxChannels,
                                       processor_.getTotalNumInputChannels());
    setSize (juce::jlimit (1000, 1680, 90 + channels * nominalStrip), 700);
    setResizable (true, true);
    setResizeLimits (800, 400, 2400, 1400);

    addAndMakeVisible (shareBar_);
    addAndMakeVisible (history_);

    stripViewport_.setViewedComponent (&stripContainer_, false);
    stripViewport_.setScrollBarsShown (false, true);
    stripViewport_.setScrollBarThickness (9);
    addAndMakeVisible (stripViewport_);

    styleChip (bypassButton_, warn, pageBg);
    bypassButton_.setTooltip ("Bypass the automixer. Crossfades over 15 ms, so it is safe "
                              "to hit mid-show.");
    addAndMakeVisible (bypassButton_);

    styleChip (nomAttenButton_, warn, pageBg);
    nomAttenButton_.setTooltip ("Extra -10*log10(NOM) attenuation on top of gain sharing. "
                                "Off by default: the shares already sum to one, so this "
                                "trades level for feedback margin.");
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

    rebuildStrips (channels);
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
    history_.setChannelCount (numChannels);
    layoutStrips();
}

void AutomixEditor::timerCallback()
{
    refresh();
}

void AutomixEditor::refresh (double frameSeconds)
{
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
    float floorSum = 0.0f;
    int floorCount = 0;
    std::vector<bool> open ((size_t) strips_.size(), false);
    std::vector<GainShareBar::Segment> segments;

    for (int ch = 0; ch < strips_.size(); ++ch)
    {
        const auto data = processor_.getChannelMeterData (ch);
        strips_[ch]->updateMeters (data, running);

        if (running && data.isActive)
        {
            ++openCount;
            open[(size_t) ch] = true;
            segments.push_back ({ ch, strips_[ch]->share() });
        }

        if (data.noiseFloorDb > -119.0f)
        {
            floorSum += data.noiseFloorDb;
            ++floorCount;
        }
    }

    // The shares are normalised to sum to one; the total is the system gain, and
    // seeing it hold at 0 dB as mics open is the whole point of gain sharing.
    float shareTotal = 0.0f;
    for (const auto& s : segments)
        shareTotal += s.share;

    const float newSystemGainDb =
        shareTotal > 0.0001f ? juce::Decibels::gainToDecibels (shareTotal, -60.0f) : 0.0f;
    const float newFloorDb = floorCount > 0 ? floorSum / (float) floorCount : -60.0f;

    shareBar_.setSegments (std::move (segments));

    if (showHistory_)
        history_.pushFrame (open, strips_.size(), frameSeconds);

    const auto global = processor_.getGlobalMeterData();
    const bool statsChanged = openCount != openChannelCount_
                              || running != audioRunning_
                              || std::abs (global.nomCount - globalMeter_.nomCount) > 0.05f
                              || std::abs (global.nomAttenuationDb
                                           - globalMeter_.nomAttenuationDb) > 0.05f
                              || std::abs (newSystemGainDb - systemGainDb_) > 0.05f
                              || std::abs (newFloorDb - noiseFloorDb_) > 0.5f;

    openChannelCount_ = openCount;
    globalMeter_ = global;
    systemGainDb_ = newSystemGainDb;
    noiseFloorDb_ = newFloorDb;
    audioRunning_ = running;

    // The strips and the two panels repaint themselves; only the chrome that
    // holds these numbers needs us.
    if (statsChanged)
    {
        repaint (statusBounds_);
        if (showTopRow_)
        {
            repaint (statsPanel_);
            repaint (sharePanel_);
        }
    }
}

void AutomixEditor::resized()
{
    auto area = getLocalBounds();

    statusBounds_ = area.removeFromTop (headerHeight);

    area.removeFromTop (bodyPadTop);
    area.removeFromBottom (bodyPadBot);
    area = area.withTrimmedLeft (bodyPadX).withTrimmedRight (bodyPadX);

    showTopRow_ = getHeight() >= minHeightForTopRow;
    showHistory_ = getHeight() >= minHeightForHistory;

    // ---- Status bar controls ----
    auto status = statusBounds_.reduced (bodyPadX, 0);
    bypassButton_.setBounds (status.removeFromRight (74).withSizeKeepingCentre (74, 22));
    status.removeFromRight (7);
    nomAttenButton_.setBounds (status.removeFromRight (82).withSizeKeepingCentre (82, 22));

    // ---- Top row ----
    if (showTopRow_)
    {
        auto top = area.removeFromTop (topRowHeight);
        area.removeFromTop (panelGap);

        responsePanel_ = top.removeFromRight (responseWidth);
        top.removeFromRight (panelGap);
        statsPanel_ = top.removeFromRight (statsWidth);
        top.removeFromRight (panelGap);
        sharePanel_ = top;

        // Share bar sits in the middle band of its panel, under the caption and
        // above the legend.
        auto barArea = sharePanel_.reduced (14, 12);
        barArea.removeFromTop (16);
        barArea.removeFromBottom (18);
        shareBar_.setBounds (barArea.withHeight (juce::jmin (38, barArea.getHeight())));

        // The caption takes the first band, then a gap for the per-knob labels
        // that paintResponsePanel draws just above each control.
        auto knobs = responsePanel_.reduced (14, 12);
        knobs.removeFromTop (30);
        knobs.removeFromBottom (14);
        const int knobW = juce::jmax (1, knobs.getWidth() / 3);
        attackSlider_.setBounds (knobs.removeFromLeft (knobW).reduced (6, 0));
        releaseSlider_.setBounds (knobs.removeFromLeft (knobW).reduced (6, 0));
        holdSlider_.setBounds (knobs.reduced (6, 0));
    }
    else
    {
        sharePanel_ = statsPanel_ = responsePanel_ = {};
        shareBar_.setBounds ({});
        attackSlider_.setBounds ({});
        releaseSlider_.setBounds ({});
        holdSlider_.setBounds ({});
    }

    shareBar_.setVisible (showTopRow_);
    attackSlider_.setVisible (showTopRow_);
    releaseSlider_.setVisible (showTopRow_);
    holdSlider_.setVisible (showTopRow_);

    // ---- History ----
    history_.setVisible (showHistory_);
    if (showHistory_)
    {
        history_.setBounds (area.removeFromBottom (historyHeight));
        area.removeFromBottom (panelGap);
    }

    // ---- Mixer bay ----
    bayHeader_ = area.removeFromTop (14);
    area.removeFromTop (6);
    stripViewport_.setBounds (area);

    layoutStrips();
}

void AutomixEditor::layoutStrips()
{
    if (strips_.isEmpty())
        return;

    const int available = juce::jmax (0, stripViewport_.getWidth());
    const int gap = 4;
    const int fitted = (available - gap * (strips_.size() - 1)) / strips_.size();
    const int stripWidth = juce::jlimit (minStripWidth, maxStripWidth, fitted);
    const int totalWidth = stripWidth * strips_.size() + gap * (strips_.size() - 1);

    const int viewportHeight = stripViewport_.getHeight();
    const int containerHeight = totalWidth > available
                                    ? juce::jmax (0, viewportHeight - 9)
                                    : viewportHeight;

    stripContainer_.setBounds (0, 0, juce::jmax (totalWidth, available), containerHeight);

    for (int i = 0; i < strips_.size(); ++i)
        strips_[i]->setBounds (i * (stripWidth + gap), 0, stripWidth, containerHeight);
}

void AutomixEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour (pageBg));

    paintStatusBar (g);

    if (showTopRow_)
    {
        paintSharePanel (g);
        paintStatTiles (g);
        paintResponsePanel (g);
    }

    paintBayHeader (g);
}

void AutomixEditor::paintStatusBar (juce::Graphics& g)
{
    auto area = statusBounds_;

    g.setColour (colour (panelBg));
    g.fillRect (area);
    g.setColour (borderHeader());
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);

    auto row = area.reduced (bodyPadX, 0);

    // A 3×3 mark: five lit cells out of nine, the same shape a gain-share
    // distribution makes.
    auto mark = row.removeFromLeft (13).withSizeKeepingCentre (13, 13);
    const bool lit[9] = { true, true, false, false, true, false, true, false, false };
    for (int i = 0; i < 9; ++i)
    {
        const int cx = mark.getX() + (i % 3) * 5;
        const int cy = mark.getY() + (i / 3) * 5;
        g.setColour (lit[i] ? colour (accent) : colour (0xff2b3238));
        g.fillRect (cx, cy, 3, 3);
    }

    row.removeFromLeft (12);
    g.setColour (colour (textBright));
    g.setFont (fonts().sans (15.0f, AutomixFonts::Weight::bold, 0.18f));
    g.drawText ("AUTOMIX", row.removeFromLeft (108), juce::Justification::centredLeft, false);

    auto divider = row.removeFromLeft (13).withSizeKeepingCentre (1, 16);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (divider);

    // The algorithm is described generically. Naming a commercial automixer
    // here would be a trademark claim the project does not make.
    drawValue (g, "GAIN-SHARING " + middot() + " " + juce::String (stripCount_) + " CH",
               row.removeFromLeft (190), textFaint, 9.0f, AutomixFonts::Weight::medium);

    // Right side: real device facts only. There is no preset system, so the
    // design's preset chip has nothing truthful to show and is left out.
    auto right = row;
    right.removeFromRight (74 + 7 + 82 + 10);   // the two buttons, placed in resized()

    const auto sr = processor_.getSampleRate();
    const auto bs = processor_.getBlockSize();
    if (sr > 0.0)
    {
        auto chip = right.removeFromRight (108).withSizeKeepingCentre (108, 22);
        g.setColour (colour (chipBg));
        g.fillRoundedRectangle (chip.toFloat(), 3.0f);
        g.setColour (chipOutline());
        g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 3.0f, 1.0f);
        drawValue (g, juce::String (sr / 1000.0, 1) + " kHz / " + juce::String (bs),
                   chip, textPrimary, 9.0f, AutomixFonts::Weight::semiBold,
                   juce::Justification::centred);
        right.removeFromRight (8);
    }

    // ACTIVE / BYPASSED state badge.
    const bool bypassed = bypassButton_.getToggleState();
    auto badge = right.removeFromRight (74).withSizeKeepingCentre (74, 22);
    g.setColour (bypassed ? colour (chipBg) : colour (accent));
    g.fillRoundedRectangle (badge.toFloat(), 3.0f);
    if (bypassed)
    {
        g.setColour (chipOutline());
        g.drawRoundedRectangle (badge.toFloat().reduced (0.5f), 3.0f, 1.0f);
    }
    g.setColour (bypassed ? colour (textFaint) : colour (pageBg));
    g.setFont (fonts().sans (9.0f, AutomixFonts::Weight::semiBold, 0.14f));
    g.drawText (bypassed ? "BYPASSED" : (audioRunning_ ? "ACTIVE" : "NO AUDIO"),
                badge, juce::Justification::centred, false);
}

void AutomixEditor::paintSharePanel (juce::Graphics& g)
{
    drawPanel (g, sharePanel_);

    auto inner = sharePanel_.reduced (14, 12);
    auto caption = inner.removeFromTop (14);

    drawLabel (g, "GAIN SHARE DISTRIBUTION", caption.removeFromLeft (200),
               textDim, 10.0f, 0.16f);
    drawValue (g, "SUM = " + juce::String (juce::jmin (1.0f, [this]
               {
                   float t = 0.0f;
                   for (auto* s : strips_) t += s->share();
                   return t;
               }()), 3) + " " + middot() + " CONSTANT SYSTEM GAIN",
               caption, textFaintest, 9.0f, AutomixFonts::Weight::medium,
               juce::Justification::centredRight);

    // Legend along the bottom: the largest contributors, named.
    auto legend = inner.removeFromBottom (14);
    std::vector<std::pair<int, float>> top;
    for (int i = 0; i < strips_.size(); ++i)
        if (strips_[i]->share() > 0.001f)
            top.emplace_back (i, strips_[i]->share());

    std::sort (top.begin(), top.end(),
               [] (const auto& a, const auto& b) { return a.second > b.second; });
    if (top.size() > 4)
        top.resize (4);

    for (const auto& [ch, share] : top)
    {
        auto cell = legend.removeFromLeft (juce::jmin (108, legend.getWidth()));
        auto swatch = cell.removeFromLeft (11).withSizeKeepingCentre (7, 7);
        g.setColour (colour (accent));
        g.fillRect (swatch);
        drawLabel (g, "CH " + juce::String (ch + 1), cell.removeFromLeft (40),
                   textSecond, 10.0f, 0.0f);
        drawValue (g, juce::String (juce::roundToInt (share * 100.0f)) + "%",
                   cell, textFaint, 10.0f, AutomixFonts::Weight::semiBold);
        legend.removeFromLeft (5);
    }
}

void AutomixEditor::paintStatTiles (juce::Graphics& g)
{
    struct Tile
    {
        juce::String top, bottom, value;
        juce::uint32 valueColour;
    };

    // The NOM figure is always computed, but it only reaches the audio path
    // when the operator asks for it. Showing it in coral either way would imply
    // the mix is being attenuated when it is not, so an unapplied figure is
    // greyed and labelled as available rather than active.
    const bool nomApplied = nomAttenButton_.getToggleState();

    const Tile tiles[4] = {
        { "NUMBER OF", "OPEN MICS", juce::String (openChannelCount_), accent },
        { "NOM ATTEN.", nomApplied ? "dB APPLIED" : "dB NOT APPLIED",
          juce::String (globalMeter_.nomAttenuationDb, 1), nomApplied ? warn : textFaintest },
        { "SYSTEM", "GAIN dB", juce::String (systemGainDb_, 1), textPrimary },
        { "NOISE FLOOR", "ADAPTIVE", juce::String (juce::roundToInt (noiseFloorDb_)), textPrimary },
    };

    const int gap = 7;
    const int tileW = (statsPanel_.getWidth() - gap) / 2;
    const int tileH = (statsPanel_.getHeight() - gap) / 2;

    for (int i = 0; i < 4; ++i)
    {
        juce::Rectangle<int> cell (statsPanel_.getX() + (i % 2) * (tileW + gap),
                                   statsPanel_.getY() + (i / 2) * (tileH + gap),
                                   tileW, tileH);
        drawPanel (g, cell);

        auto inner = cell.reduced (11, 9);
        drawLabel (g, tiles[i].top, inner.removeFromTop (10), textFaint, 8.0f, 0.12f);
        drawLabel (g, tiles[i].bottom, inner.removeFromTop (10), textFaint, 8.0f, 0.12f);

        g.setColour (colour (tiles[i].valueColour));
        g.setFont (fonts().mono (22.0f, AutomixFonts::Weight::semiBold));
        g.drawText (tiles[i].value, inner, juce::Justification::bottomLeft, false);
    }
}

void AutomixEditor::paintResponsePanel (juce::Graphics& g)
{
    drawPanel (g, responsePanel_);

    auto inner = responsePanel_.reduced (14, 12);
    drawLabel (g, "RESPONSE", inner.removeFromTop (14), textDim, 10.0f, 0.16f);

    // The design put three A/B/C mix groups here. The plugin has no grouping
    // feature, so this panel carries the timing controls instead rather than
    // showing a control that does nothing.
    struct Knob { const juce::Slider* s; const char* label; juce::String value; };
    const Knob knobs[3] = {
        { &attackSlider_,  "ATTACK",  juce::String (attackSlider_.getValue(), 1) + " ms" },
        { &releaseSlider_, "RELEASE", juce::String (releaseSlider_.getValue(), 0) + " ms" },
        { &holdSlider_,    "HOLD",    juce::String (holdSlider_.getValue(), 0) + " ms" },
    };

    for (const auto& k : knobs)
    {
        if (k.s->getWidth() <= 0)
            continue;

        juce::Rectangle<int> label (k.s->getX(), k.s->getY() - 12, k.s->getWidth(), 10);
        drawLabel (g, k.label, label, textFaint, 8.0f, 0.12f, juce::Justification::centred);

        juce::Rectangle<int> value (k.s->getX(), k.s->getBottom() + 1, k.s->getWidth(), 12);
        drawValue (g, k.value, value, textPrimary, 10.0f,
                   AutomixFonts::Weight::semiBold, juce::Justification::centred);
    }
}

void AutomixEditor::paintBayHeader (juce::Graphics& g)
{
    auto row = bayHeader_;

    drawLabel (g, "CHANNELS", row.removeFromLeft (78), textDim, 10.0f, 0.16f);
    row.removeFromLeft (6);

    auto swatchLabel = [&g, &row] (juce::uint32 c, const juce::String& text)
    {
        auto swatch = row.removeFromLeft (11).withSizeKeepingCentre (6, 6);
        g.setColour (colour (c));
        g.fillRect (swatch);
        drawLabel (g, text, row.removeFromLeft (88), textFaintest, 9.0f, 0.0f);
        row.removeFromLeft (6);
    };

    swatchLabel (accent, "Input level");
    swatchLabel (warn, "Gain reduction");
}
