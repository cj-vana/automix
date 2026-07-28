#include "ChannelStrip.h"

namespace
{
using namespace AutomixTheme;

// Bar ballistics, in frames of the editor's 30 Hz refresh.
constexpr float riseRate     = 0.65f;
constexpr float fallRate     = 0.22f;
constexpr int   peakHoldTime = 22;
constexpr float peakFallRate = 0.10f;

float ballistic (float current, float target)
{
    return current + (target > current ? riseRate : fallRate) * (target - current);
}

void styleStateButton (juce::TextButton& b, juce::uint32 onColour)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, ghostFill());
    b.setColour (juce::TextButton::buttonOnColourId, colour (onColour));
    b.setColour (juce::TextButton::textColourOffId, colour (textFainter));
    b.setColour (juce::TextButton::textColourOnId, colour (pageBg));
}
} // namespace

ChannelStrip::ChannelStrip (AutomixProcessor& processor, int channelIndex)
    : processor_ (processor), channel_ (channelIndex)
{
    weightSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    weightSlider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    weightSlider_.setColour (juce::Slider::backgroundColourId, colour (wellBg));
    weightSlider_.setColour (juce::Slider::trackColourId, colour (textFainter));
    weightSlider_.setColour (juce::Slider::thumbColourId, colour (textSecond));
    weightSlider_.setTooltip ("Channel " + juce::String (channel_ + 1)
                             + " weight. Biases this mic's share of the mix.");
    addAndMakeVisible (weightSlider_);

    styleStateButton (soloButton_, accent);
    styleStateButton (muteButton_, warn);
    styleStateButton (bypassButton_, textDim);
    soloButton_.setTooltip ("Solo. Only soloed channels take part in gain sharing.");
    muteButton_.setTooltip ("Mute. Removes this channel from the mix and from gain sharing.");
    bypassButton_.setTooltip ("Bypass. Passes this channel through at unity, unmixed.");
    addAndMakeVisible (soloButton_);
    addAndMakeVisible (muteButton_);
    addAndMakeVisible (bypassButton_);

    auto& apvts = processor_.apvts;
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    weightAttach_ = std::make_unique<SliderAttach> (
        apvts, AutomixParams::channelParamID (channel_, "weight"), weightSlider_);
    soloAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::channelParamID (channel_, "solo"), soloButton_);
    muteAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::channelParamID (channel_, "mute"), muteButton_);
    bypassAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::channelParamID (channel_, "bypass"), bypassButton_);
}

ChannelStrip::~ChannelStrip() = default;

void ChannelStrip::updateMeters (const AutomixProcessor::ChannelMeterData& data,
                                bool audioRunning)
{
    const float targetInput = audioRunning ? juce::jmax (data.inputRmsDb, meterFloorDb)
                                           : meterFloorDb;
    const float targetGain  = audioRunning ? data.gainDb : 0.0f;
    const bool  targetOpen  = audioRunning && data.isActive;

    const float newInput = ballistic (displayInputDb_, targetInput);
    const float newGain  = ballistic (displayGainDb_, targetGain);

    float newPeak = peakHoldDb_;
    int   newHold = peakHoldFrames_;
    if (newInput >= newPeak)
    {
        newPeak = newInput;
        newHold = peakHoldTime;
    }
    else if (newHold > 0)
    {
        --newHold;
    }
    else
    {
        newPeak += peakFallRate * (newInput - newPeak);
    }

    // The gains are normalised to sum to one, so the linear gain of an open
    // channel is exactly its share of the mix.
    const float newShare = targetOpen ? juce::Decibels::decibelsToGain (targetGain, -100.0f)
                                      : 0.0f;

    const bool changed = std::abs (newInput - displayInputDb_) > 0.1f
                         || std::abs (newGain - displayGainDb_) > 0.1f
                         || std::abs (newPeak - peakHoldDb_) > 0.1f
                         || targetOpen != isOpen_;

    displayInputDb_ = newInput;
    displayGainDb_  = newGain;
    peakHoldDb_     = newPeak;
    peakHoldFrames_ = newHold;
    share_          = newShare;
    isOpen_         = targetOpen;

    // Repaint only the region these values drive. At 32 strips and 30 Hz the
    // difference between this and a full repaint is the difference between an
    // idle GUI and a busy one.
    if (changed)
        repaint (dynamicBounds_);
}

void ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced (4, 6);

    headerRow_ = area.removeFromTop (10);
    area.removeFromTop (5);
    labelRow_ = area.removeFromTop (21);
    area.removeFromTop (5);

    buttonRow_ = area.removeFromBottom (13);
    area.removeFromBottom (4);
    weightRow_ = area.removeFromBottom (10);
    area.removeFromBottom (3);
    gainRow_ = area.removeFromBottom (13);
    area.removeFromBottom (5);
    barsArea_ = area;

    dynamicBounds_ = headerRow_.getUnion (labelRow_).getUnion (barsArea_).getUnion (gainRow_);

    // Weight rides as a thin track beside its label rather than as a fader: at
    // this column width a fader would be unusable, and the number is what the
    // operator actually reads. Taking the track off weightRow_ here leaves the
    // caption band for paintReadouts.
    weightSlider_.setBounds (weightRow_.removeFromRight (
        juce::jlimit (16, 30, weightRow_.getWidth() / 3)));

    auto buttons = buttonRow_;
    const int w = buttons.getWidth() / 3;
    soloButton_.setBounds (buttons.removeFromLeft (w).reduced (1, 0));
    muteButton_.setBounds (buttons.removeFromLeft (w).reduced (1, 0));
    bypassButton_.setBounds (buttons.reduced (1, 0));
}

void ChannelStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    // An open channel lifts slightly out of the bay and takes a lime edge. That
    // pairing is the fastest read on the panel: which mics are live, right now.
    g.setColour (isOpen_ ? juce::Colour (accent).withAlpha (0.07f)
                         : juce::Colour (panelBg));
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);
    g.setColour (isOpen_ ? juce::Colour (accent).withAlpha (0.45f) : border());
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 3.0f, 1.0f);

    paintHeaderRow (g);
    paintLabel (g);
    paintBars (g);
    paintReadouts (g);
}

void ChannelStrip::paintHeaderRow (juce::Graphics& g)
{
    auto row = headerRow_;

    drawValue (g, juce::String (channel_ + 1).paddedLeft ('0', 2),
               row.removeFromLeft (16), textFainter, 8.0f,
               AutomixFonts::Weight::medium);

    // The badge says why a channel is out of the mix, which is the question
    // asked when one goes quiet unexpectedly.
    juce::String badge;
    juce::uint32 badgeColour = textDim;
    if (muteButton_.getToggleState())        { badge = "M"; badgeColour = warn; }
    else if (bypassButton_.getToggleState()) { badge = "B"; badgeColour = textDim; }
    else if (soloButton_.getToggleState())   { badge = "S"; badgeColour = accent; }

    if (badge.isNotEmpty())
    {
        auto chip = row.removeFromRight (11);
        g.setColour (badgeFill());
        g.fillRoundedRectangle (chip.toFloat(), 2.0f);
        drawValue (g, badge, chip, badgeColour, 8.0f,
                   AutomixFonts::Weight::semiBold, juce::Justification::centred);
    }
}

void ChannelStrip::paintLabel (juce::Graphics& g)
{
    // The plugin has no channel-naming feature, so the design's "HOST MIC"
    // slot carries the only identity that actually exists.
    drawLabel (g, "CH " + juce::String (channel_ + 1), labelRow_,
               isOpen_ ? accent : textFainter, 9.0f, 0.03f,
               juce::Justification::topLeft);
}

void ChannelStrip::paintBars (juce::Graphics& g)
{
    if (barsArea_.isEmpty())
        return;

    const int barWidth = juce::jlimit (7, 11, (barsArea_.getWidth() - 3) / 2);
    const int totalWidth = barWidth * 2 + 3;
    auto area = barsArea_.withSizeKeepingCentre (totalWidth, barsArea_.getHeight());

    auto inputCol = area.removeFromLeft (barWidth);
    area.removeFromLeft (3);
    auto grCol = area;

    drawWell (g, inputCol);
    drawWell (g, grCol);

    // Input level fills upward.
    const float inNorm = dbToNorm (displayInputDb_);
    if (inNorm > 0.0f)
    {
        const int h = juce::roundToInt (inNorm * (float) inputCol.getHeight());
        g.setColour (isOpen_ ? colour (accent) : colour (accent).withAlpha (0.35f));
        g.fillRoundedRectangle (inputCol.removeFromBottom (h).toFloat(), 1.0f);
    }

    // Peak marker sits above the fill so a transient stays visible after the
    // level has fallen away from it.
    if (peakHoldDb_ > meterFloorDb)
    {
        const int y = barsArea_.getBottom()
                      - juce::roundToInt (dbToNorm (peakHoldDb_) * (float) barsArea_.getHeight());
        g.setColour (colour (textSecond).withAlpha (0.8f));
        g.fillRect (inputCol.getX(), y, barWidth, 1);
    }

    // Gain reduction fills downward, and only where there is signal to reduce:
    // a silent channel is held fully closed, which is true but would peg every
    // idle column and drown out the ones doing something.
    const bool hasSignal = displayInputDb_ > meterFloorDb + 1.0f;
    const float reductionDb =
        hasSignal ? juce::jlimit (0.0f, maxGainReductionDb, -displayGainDb_) : 0.0f;
    const int grHeight =
        juce::roundToInt ((reductionDb / maxGainReductionDb) * (float) grCol.getHeight());
    if (grHeight > 0)
    {
        g.setColour (isOpen_ ? colour (warn) : colour (warn).withAlpha (0.35f));
        g.fillRoundedRectangle (grCol.removeFromTop (grHeight).toFloat(), 1.0f);
    }
}

void ChannelStrip::paintReadouts (juce::Graphics& g)
{
    const bool silent = displayInputDb_ <= meterFloorDb + 1.0f;
    const juce::String gainText =
        silent ? juce::String ("--") : juce::String (displayGainDb_, 1);

    drawValue (g, gainText, gainRow_,
               silent ? textFaintest : (isOpen_ ? textPrimary : textFainter),
               10.0f, AutomixFonts::Weight::semiBold, juce::Justification::centred);

    // The weight track is thin at this column width, so the number carries the
    // value and the track is only there to grab. weightRow_ was already trimmed
    // by resized() to exclude the track, so what is left is the caption band.
    auto caption = weightRow_;
    drawLabel (g, "WT", caption.removeFromLeft (14),
               textFaintest, 7.0f, 0.06f, juce::Justification::centredLeft);
    drawValue (g, juce::String (weightSlider_.getValue(), 2), caption,
               textDim, 7.5f, AutomixFonts::Weight::medium,
               juce::Justification::centredRight);
}
