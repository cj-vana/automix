#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

AutomixProcessor::AutomixProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::discreteChannels (kMaxChannels), true)
          .withOutput ("Output", juce::AudioChannelSet::discreteChannels (kMaxChannels), true)),
      apvts (*this, nullptr, "AutoMixState", AutomixParams::createParameterLayout())
{
    // Cache raw parameter value pointers for audio-thread access.
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        channelWeightParams_[ch]  = apvts.getRawParameterValue (AutomixParams::channelParamID (ch, "weight"));
        channelMuteParams_[ch]    = apvts.getRawParameterValue (AutomixParams::channelParamID (ch, "mute"));
        channelSoloParams_[ch]    = apvts.getRawParameterValue (AutomixParams::channelParamID (ch, "solo"));
        channelBypassParams_[ch]  = apvts.getRawParameterValue (AutomixParams::channelParamID (ch, "bypass"));
    }

    globalBypassParam_ = apvts.getRawParameterValue (AutomixParams::globalBypassID);
    attackMsParam_     = apvts.getRawParameterValue (AutomixParams::attackMsID);
    releaseMsParam_    = apvts.getRawParameterValue (AutomixParams::releaseMsID);
    holdMsParam_       = apvts.getRawParameterValue (AutomixParams::holdMsID);
    nomAttenParam_     = apvts.getRawParameterValue (AutomixParams::nomAttenID);

    // Initialize metering to silence
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        meterInputRmsDb_[ch].store (-120.0f, std::memory_order_relaxed);
        meterGainDb_[ch].store (-120.0f, std::memory_order_relaxed);
        meterOutputRmsDb_[ch].store (-120.0f, std::memory_order_relaxed);
        meterNoiseFloorDb_[ch].store (-120.0f, std::memory_order_relaxed);
        meterIsActive_[ch].store (false, std::memory_order_relaxed);
    }
}

AutomixProcessor::~AutomixProcessor()
{
    if (engine_ != nullptr)
    {
        automix_destroy (engine_);
        engine_ = nullptr;
    }
}

void AutomixProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (engine_ != nullptr)
    {
        automix_destroy (engine_);
    }

    engine_ = automix_create (
        static_cast<uint32_t> (getTotalNumInputChannels()),
        static_cast<float> (sampleRate),
        static_cast<uint32_t> (samplesPerBlock));

    // Force full sync on next processBlock by invalidating the cache
    invalidateParameterCache();
    syncParametersToEngine();
}

void AutomixProcessor::releaseResources()
{
    if (engine_ != nullptr)
    {
        automix_destroy (engine_);
        engine_ = nullptr;
    }
}

void AutomixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (engine_ == nullptr)
        return;

    syncParametersToEngine();

    automix_process (
        engine_,
        buffer.getArrayOfWritePointers(),
        static_cast<uint32_t> (buffer.getNumChannels()),
        static_cast<uint32_t> (buffer.getNumSamples()));

    cacheMetering();
}

static bool floatChanged (float a, float b)
{
    return std::memcmp (&a, &b, sizeof (float)) != 0;
}

void AutomixProcessor::syncParametersToEngine()
{
    // Global params — only call FFI when values have changed
    const bool globalBypass = globalBypassParam_->load (std::memory_order_relaxed) >= 0.5f;
    if (globalBypass != cachedGlobalBypass_ || !cacheInitialized_)
    {
        automix_set_global_bypass (engine_, globalBypass);
        cachedGlobalBypass_ = globalBypass;
    }

    const float attackMs = attackMsParam_->load (std::memory_order_relaxed);
    if (floatChanged (attackMs, cachedAttackMs_))
    {
        automix_set_attack_ms (engine_, attackMs);
        cachedAttackMs_ = attackMs;
    }

    const float releaseMs = releaseMsParam_->load (std::memory_order_relaxed);
    if (floatChanged (releaseMs, cachedReleaseMs_))
    {
        automix_set_release_ms (engine_, releaseMs);
        cachedReleaseMs_ = releaseMs;
    }

    const float holdMs = holdMsParam_->load (std::memory_order_relaxed);
    if (floatChanged (holdMs, cachedHoldMs_))
    {
        automix_set_hold_time_ms (engine_, holdMs);
        cachedHoldMs_ = holdMs;
    }

    const bool nomAtten = nomAttenParam_->load (std::memory_order_relaxed) >= 0.5f;
    if (nomAtten != cachedNomAtten_ || !cacheInitialized_)
    {
        automix_set_nom_atten_enabled (engine_, nomAtten);
        cachedNomAtten_ = nomAtten;
    }

    // Per-channel params (only for active channels, only when changed)
    const auto numCh = static_cast<uint32_t> (getTotalNumInputChannels());
    for (uint32_t ch = 0; ch < numCh && ch < static_cast<uint32_t> (kMaxChannels); ++ch)
    {
        const float weight = channelWeightParams_[ch]->load (std::memory_order_relaxed);
        if (floatChanged (weight, cachedChannelWeight_[ch]))
        {
            automix_set_channel_weight (engine_, ch, weight);
            cachedChannelWeight_[ch] = weight;
        }

        const bool muted = channelMuteParams_[ch]->load (std::memory_order_relaxed) >= 0.5f;
        if (muted != cachedChannelMute_[ch] || !cacheInitialized_)
        {
            automix_set_channel_mute (engine_, ch, muted);
            cachedChannelMute_[ch] = muted;
        }

        const bool soloed = channelSoloParams_[ch]->load (std::memory_order_relaxed) >= 0.5f;
        if (soloed != cachedChannelSolo_[ch] || !cacheInitialized_)
        {
            automix_set_channel_solo (engine_, ch, soloed);
            cachedChannelSolo_[ch] = soloed;
        }

        const bool bypassed = channelBypassParams_[ch]->load (std::memory_order_relaxed) >= 0.5f;
        if (bypassed != cachedChannelBypass_[ch] || !cacheInitialized_)
        {
            automix_set_channel_bypass (engine_, ch, bypassed);
            cachedChannelBypass_[ch] = bypassed;
        }
    }

    cacheInitialized_ = true;
}

void AutomixProcessor::invalidateParameterCache()
{
    cacheInitialized_ = false;
    cachedAttackMs_  = -1.0f;
    cachedReleaseMs_ = -1.0f;
    cachedHoldMs_    = -1.0f;
    for (int ch = 0; ch < kMaxChannels; ++ch)
        cachedChannelWeight_[ch] = -1.0f;
}

void AutomixProcessor::cacheMetering()
{
    AutomixChannelMetering meters[kMaxChannels] {};
    const auto numWritten = automix_get_all_channel_metering (
        engine_, meters, static_cast<uint32_t> (kMaxChannels));

    for (uint32_t i = 0; i < numWritten; ++i)
    {
        meterInputRmsDb_[i].store (meters[i].input_rms_db, std::memory_order_relaxed);
        meterGainDb_[i].store (meters[i].gain_db, std::memory_order_relaxed);
        meterOutputRmsDb_[i].store (meters[i].output_rms_db, std::memory_order_relaxed);
        meterNoiseFloorDb_[i].store (meters[i].noise_floor_db, std::memory_order_relaxed);
        meterIsActive_[i].store (meters[i].is_active, std::memory_order_relaxed);
    }

    AutomixGlobalMetering gm {};
    if (automix_get_global_metering (engine_, &gm))
    {
        meterNomCount_.store (gm.nom_count, std::memory_order_relaxed);
        meterNomAttenDb_.store (gm.nom_attenuation_db, std::memory_order_relaxed);
    }
}

// ---- Metering getters (called from GUI thread) ----

AutomixProcessor::ChannelMeterData AutomixProcessor::getChannelMeterData (int channel) const
{
    if (channel < 0 || channel >= kMaxChannels)
        return {};

    return {
        meterInputRmsDb_[channel].load (std::memory_order_relaxed),
        meterGainDb_[channel].load (std::memory_order_relaxed),
        meterOutputRmsDb_[channel].load (std::memory_order_relaxed),
        meterNoiseFloorDb_[channel].load (std::memory_order_relaxed),
        meterIsActive_[channel].load (std::memory_order_relaxed)
    };
}

AutomixProcessor::GlobalMeterData AutomixProcessor::getGlobalMeterData() const
{
    return {
        meterNomCount_.load (std::memory_order_relaxed),
        meterNomAttenDb_.load (std::memory_order_relaxed)
    };
}

int AutomixProcessor::getActiveChannelCount() const
{
    int count = 0;
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        if (meterIsActive_[ch].load (std::memory_order_relaxed))
            ++count;
    }
    return count;
}

bool AutomixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    int numChannels = mainInput.size();
    return numChannels >= 1 && numChannels <= kMaxChannels;
}

juce::AudioProcessorEditor* AutomixProcessor::createEditor()
{
    return new AutomixEditor (*this);
}

// ---- State Persistence ----

void AutomixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary (*xml, destData);
}

void AutomixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutomixProcessor();
}
