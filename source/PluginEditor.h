#pragma once

#include "PluginProcessor.h"
#include "gui/AutomixFonts.h"
#include "gui/AutomixTheme.h"
#include "gui/ChannelStrip.h"
#include "gui/GainShareBar.h"
#include "gui/OpenMicHistory.h"

/// The plugin window, following design option 1a: a status bar, a row of
/// aggregate readouts, the mixer bay, and a thirty-second activity history.
///
/// The window resizes from 800×400 to 2400×1400, so the secondary rows drop out
/// as height runs short. The mixer bay is the last thing to go.
class AutomixEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit AutomixEditor (AutomixProcessor&);
    ~AutomixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /// Pull one frame of meter data from the processor and update the display.
    ///
    /// Normally driven by the editor's own timer. Exposed so the offscreen
    /// snapshot tool can step the interface deterministically instead of
    /// running a message loop and hoping the timer fired enough times.
    void refresh (double frameSeconds = 1.0 / 30.0);

private:
    void timerCallback() override;
    void rebuildStrips (int numChannels);
    void layoutStrips();

    void paintStatusBar (juce::Graphics&);
    void paintSharePanel (juce::Graphics&);
    void paintStatTiles (juce::Graphics&);
    void paintResponsePanel (juce::Graphics&);
    void paintBayHeader (juce::Graphics&);

    AutomixProcessor& processor_;

    GainShareBar shareBar_;
    OpenMicHistory history_;
    juce::Viewport stripViewport_;
    juce::Component stripContainer_;
    juce::OwnedArray<ChannelStrip> strips_;

    juce::TextButton bypassButton_ { "BYPASS" };
    juce::TextButton nomAttenButton_ { "NOM ATTEN" };
    juce::Slider attackSlider_, releaseSlider_, holdSlider_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> nomAttenAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttach_;

    juce::TooltipWindow tooltips_ { this, 700 };

    AutomixProcessor::GlobalMeterData globalMeter_;
    int      openChannelCount_ = 0;
    int      stripCount_       = 0;
    float    noiseFloorDb_     = -60.0f;
    float    systemGainDb_     = 0.0f;
    uint32_t lastBlockCounter_ = 0;
    int      idleFrames_       = 0;
    bool     audioRunning_     = false;

    // Layout regions, recomputed in resized() and read by the paint helpers.
    juce::Rectangle<int> statusBounds_, sharePanel_, statsPanel_, responsePanel_, bayHeader_;
    bool showTopRow_  = true;
    bool showHistory_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomixEditor)
};
