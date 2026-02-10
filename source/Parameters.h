#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

extern "C"
{
    #include "automix_dsp.h"
}

namespace AutomixParams
{

static constexpr int kMaxChannels = AUTOMIX_MAX_CHANNELS;

// ---- Parameter ID helpers ----

// Channel param IDs: "ch01_weight", "ch02_mute", etc. (1-based, zero-padded)
inline juce::String channelParamID (int channel, const juce::String& suffix)
{
    return juce::String::formatted ("ch%02d_%s", channel + 1, suffix.toRawUTF8());
}

// Global param ID strings
inline const juce::String globalBypassID { "global_bypass" };
inline const juce::String attackMsID     { "attack_ms" };
inline const juce::String releaseMsID    { "release_ms" };
inline const juce::String holdMsID       { "hold_ms" };
inline const juce::String nomAttenID     { "nom_atten" };

// ---- Parameter Layout Factory ----

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Per-channel parameters (32 channels x 4 params = 128 params)
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        auto chStr = juce::String::formatted ("Ch %d", ch + 1);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { channelParamID (ch, "weight"), 1 },
            chStr + " Weight",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
            1.0f));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { channelParamID (ch, "mute"), 1 },
            chStr + " Mute",
            false));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { channelParamID (ch, "solo"), 1 },
            chStr + " Solo",
            false));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { channelParamID (ch, "bypass"), 1 },
            chStr + " Bypass",
            false));
    }

    // Global parameters (5 params)
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { globalBypassID, 1 },
        "Global Bypass",
        false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackMsID, 1 },
        "Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.4f),
        5.0f,
        juce::String {},
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " ms"; },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseMsID, 1 },
        "Release",
        juce::NormalisableRange<float> (1.0f, 1000.0f, 0.1f, 0.4f),
        150.0f,
        juce::String {},
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " ms"; },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { holdMsID, 1 },
        "Hold",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 1.0f, 0.35f),
        500.0f,
        juce::String {},
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 0) + " ms"; },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { nomAttenID, 1 },
        "NOM Attenuation",
        true));

    return layout;
}

} // namespace AutomixParams
