#include "ChannelStrip.h"

namespace
{
using namespace AutomixTheme;

// Meter ballistics, in frames of the editor's 30 Hz refresh.
constexpr float riseRate     = 0.65f; // fraction of the gap closed per frame going up
constexpr float fallRate     = 0.22f; // slower down, so short peaks stay readable
constexpr int   peakHoldTime = 22;    // ~0.7 s before the peak marker starts falling
constexpr float peakFallRate = 0.10f;

/// One-sided smoothing: jump toward a rising value, ease away from a falling one.
float ballistic (float current, float target)
{
    const float rate = target > current ? riseRate : fallRate;
    return current + rate * (target - current);
}

void styleStateButton (juce::TextButton& b, juce::uint32 onColour)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, colour (buttonIdle));
    b.setColour (juce::TextButton::buttonOnColourId, colour (onColour));
    b.setColour (juce::TextButton::textColourOffId, colour (textDim));
    b.setColour (juce::TextButton::textColourOnId, colour (0xff0b0c0d));
}

/// Draw a vertical ladder of discrete segments filling `fraction` of `area`.
///
/// `fromTop` hangs the fill from the top edge, which is how gain reduction
/// reads on a compressor. `segmentColour` is asked for the colour of each lit
/// segment so the level ladder can change hue as it climbs.
template <typename ColourForNorm>
void drawSegments (juce::Graphics& g,
                   juce::Rectangle<int> area,
                   float fraction,
                   bool fromTop,
                   juce::Colour offColour,
                   ColourForNorm segmentColour)
{
    const int pitch = segmentHeight + segmentGap;
    const int count = juce::jmax (1, area.getHeight() / pitch);
    const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, fraction) * (float) count);

    for (int i = 0; i < count; ++i)
    {
        // i counts from the filled end so `lit` is a simple prefix.
        const int y = fromTop ? area.getY() + i * pitch
                              : area.getBottom() - (i + 1) * pitch + segmentGap;

        const bool on = i < lit;
        // Midpoint of this segment as a 0..1 position up the ladder.
        const float norm = ((float) i + 0.5f) / (float) count;

        g.setColour (on ? segmentColour (norm) : offColour);
        g.fillRect (area.getX(), y, area.getWidth(), segmentHeight);
    }
}
} // namespace

StripLayout StripLayout::compute (juce::Rectangle<int> bounds)
{
    StripLayout l;
    auto area = bounds;

    l.cap = area.removeFromTop (17);
    area.removeFromTop (5);
    l.lamp = area.removeFromTop (9);
    area.removeFromTop (6);

    l.buttons = area.removeFromBottom (22);
    area.removeFromBottom (7);
    l.weightLabel = area.removeFromBottom (10);
    l.weight = area.removeFromBottom (74);
    area.removeFromBottom (8);
    l.readout = area.removeFromBottom (15);
    area.removeFromBottom (3);
    l.meter = area;

    return l;
}

ChannelStrip::ChannelStrip (AutomixProcessor& processor, int channelIndex)
    : processor_ (processor), channel_ (channelIndex)
{
    weightSlider_.setSliderStyle (juce::Slider::LinearVertical);
    weightSlider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    weightSlider_.setColour (juce::Slider::backgroundColourId, colour (meterWell));
    weightSlider_.setColour (juce::Slider::trackColourId, colour (gainReduce));
    weightSlider_.setColour (juce::Slider::thumbColourId, colour (textPrimary));
    weightSlider_.setTooltip ("Channel " + juce::String (channel_ + 1)
                             + " weight — bias this mic's share of the mix");
    addAndMakeVisible (weightSlider_);

    styleStateButton (muteButton_, muteOn);
    styleStateButton (soloButton_, soloOn);
    styleStateButton (bypassButton_, bypassOn);
    muteButton_.setConnectedEdges (juce::Button::ConnectedOnRight);
    soloButton_.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    bypassButton_.setConnectedEdges (juce::Button::ConnectedOnLeft);
    muteButton_.setTooltip ("Mute — removes this channel from the mix and from gain-sharing");
    soloButton_.setTooltip ("Solo — only soloed channels participate");
    bypassButton_.setTooltip ("Bypass — passes this channel through at unity, unmixed");
    addAndMakeVisible (muteButton_);
    addAndMakeVisible (soloButton_);
    addAndMakeVisible (bypassButton_);

    auto& apvts = processor_.apvts;
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    weightAttach_ = std::make_unique<SliderAttach> (
        apvts, AutomixParams::channelParamID (channel_, "weight"), weightSlider_);
    muteAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::channelParamID (channel_, "mute"), muteButton_);
    soloAttach_ = std::make_unique<ButtonAttach> (
        apvts, AutomixParams::channelParamID (channel_, "solo"), soloButton_);
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
    const float newFloor = audioRunning ? juce::jmax (data.noiseFloorDb, meterFloorDb)
                                        : meterFloorDb;

    float newPeak = peakHoldDb_;
    int   newHoldFrames = peakHoldFrames_;
    if (newInput >= newPeak)
    {
        newPeak = newInput;
        newHoldFrames = peakHoldTime;
    }
    else if (newHoldFrames > 0)
    {
        --newHoldFrames;
    }
    else
    {
        newPeak += peakFallRate * (newInput - newPeak);
    }

    // Repainting 32 strips at 30 Hz is only cheap if we skip the ones that have
    // not visibly moved. A tenth of a dB is below what the meter can render.
    const bool changed = std::abs (newInput - displayInputDb_) > 0.1f
                         || std::abs (newGain - displayGainDb_) > 0.1f
                         || std::abs (newFloor - displayFloorDb_) > 0.5f
                         || std::abs (newPeak - peakHoldDb_) > 0.1f
                         || targetOpen != isOpen_;

    displayInputDb_ = newInput;
    displayGainDb_  = newGain;
    displayFloorDb_ = newFloor;
    peakHoldDb_     = newPeak;
    peakHoldFrames_ = newHoldFrames;
    isOpen_         = targetOpen;

    if (changed)
        repaint (dynamicBounds_);
}

void ChannelStrip::resized()
{
    layout_ = StripLayout::compute (getLocalBounds().reduced (3, 5));
    dynamicBounds_ = layout_.cap.getUnion (layout_.lamp)
                               .getUnion (layout_.meter)
                               .getUnion (layout_.readout);

    weightSlider_.setBounds (layout_.weight.withSizeKeepingCentre (
        juce::jmin (layout_.weight.getWidth(), 20), layout_.weight.getHeight()));

    auto controls = layout_.buttons;
    const int buttonWidth = controls.getWidth() / 3;
    muteButton_.setBounds (controls.removeFromLeft (buttonWidth));
    soloButton_.setBounds (controls.removeFromLeft (buttonWidth));
    bypassButton_.setBounds (controls);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    g.setColour (colour (channel_ % 2 == 0 ? stripFill : stripFillAlt));
    g.fillRect (bounds);

    g.setColour (colour (hairline));
    g.fillRect (bounds.getRight() - 1, bounds.getY(), 1, bounds.getHeight());

    paintCap (g);
    paintLamp (g);
    paintMeter (g);
    paintReadout (g);

    g.setColour (colour (textFaint));
    g.setFont (labelFont (8.0f, true));
    g.drawText ("WEIGHT", layout_.weightLabel, juce::Justification::centred, false);
}

void ChannelStrip::paintCap (juce::Graphics& g)
{
    // A slightly raised band carrying the channel number, so the eye can count
    // strips without reading every label.
    g.setColour (colour (stripCap));
    g.fillRect (layout_.cap);
    g.setColour (colour (bevel).withAlpha (0.30f));
    g.fillRect (layout_.cap.getX(), layout_.cap.getY(), layout_.cap.getWidth(), 1);

    g.setColour (isOpen_ ? colour (micOpen) : colour (textDim));
    g.setFont (monoFont (12.0f, true));
    g.drawText (juce::String (channel_ + 1).paddedLeft ('0', 2),
                layout_.cap,
                juce::Justification::centred,
                false);
}

void ChannelStrip::paintLamp (juce::Graphics& g)
{
    const auto bar = layout_.lamp.reduced (4, 1).toFloat();

    if (isOpen_)
    {
        g.setColour (colour (micOpenSoft));
        g.fillRoundedRectangle (bar.expanded (2.0f), 3.0f);
        g.setColour (colour (micOpen));
    }
    else
    {
        g.setColour (colour (lampOff));
    }
    g.fillRoundedRectangle (bar, 1.5f);
}

void ChannelStrip::paintMeter (juce::Graphics& g)
{
    auto area = layout_.meter;
    if (area.isEmpty())
        return;

    // Level takes the wider left column, gain reduction a narrow right one.
    // Adjacent rather than overlaid, so neither has to be read through the
    // other.
    const int grWidth = juce::jlimit (5, 10, area.getWidth() / 4);
    auto grColumn = area.removeFromRight (grWidth);
    area.removeFromRight (3);
    auto levelColumn = area;

    drawWell (g, levelColumn);
    drawWell (g, grColumn);

    auto levelInner = levelColumn.reduced (2, 2);
    auto grInner = grColumn.reduced (2, 2);

    drawSegments (g,
                  levelInner,
                  dbToNorm (displayInputDb_),
                  false,
                  colour (segmentOff),
                  [] (float norm)
                  {
                      return levelColour (meterFloorDb
                                          + norm * (meterCeilingDb - meterFloorDb));
                  });

    // Gain reduction hangs from the top the way it does on a compressor, so
    // "more bar" reads as "more attenuation".
    //
    // A silent channel is held at full attenuation, which is true but useless to
    // display: it would peg every idle strip and the wall of bars drowns out the
    // channels that are actually doing something. Show reduction only where
    // there is signal to reduce, and dim it until the mic is open.
    const bool hasSignal = displayInputDb_ > meterFloorDb + 1.0f;
    const float reductionDb =
        hasSignal ? juce::jlimit (0.0f, maxGainReductionDb, -displayGainDb_) : 0.0f;
    const auto grColour = isOpen_ ? colour (gainReduce) : colour (gainReduceDim);
    drawSegments (g,
                  grInner,
                  reductionDb / maxGainReductionDb,
                  true,
                  colour (segmentOff),
                  [grColour] (float) { return grColour; });

    // Noise-floor tick: the threshold this channel is being judged against.
    const int floorY =
        levelInner.getBottom()
        - juce::roundToInt (dbToNorm (displayFloorDb_) * (float) levelInner.getHeight());
    g.setColour (colour (noiseFloorTick));
    g.fillRect (levelInner.getX(), floorY, levelInner.getWidth(), 1);

    // Peak marker.
    if (peakHoldDb_ > meterFloorDb)
    {
        const int peakY =
            levelInner.getBottom()
            - juce::roundToInt (dbToNorm (peakHoldDb_) * (float) levelInner.getHeight());
        g.setColour (levelColour (peakHoldDb_).withAlpha (0.9f));
        g.fillRect (levelInner.getX(), peakY, levelInner.getWidth(), 2);
    }
}

void ChannelStrip::paintReadout (juce::Graphics& g)
{
    const bool silent = displayInputDb_ <= meterFloorDb;
    g.setColour (silent ? colour (textFaint) : colour (textPrimary));
    g.setFont (monoFont (10.0f));
    g.drawText (silent ? juce::String ("--")
                       : juce::String (juce::roundToInt (displayInputDb_)),
                layout_.readout,
                juce::Justification::centred,
                false);
}
