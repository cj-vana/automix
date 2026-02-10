#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

extern "C"
{
    #include "automix_dsp.h"
}

class AutomixProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxChannels = AUTOMIX_MAX_CHANNELS;

    AutomixProcessor();
    ~AutomixProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Metering API (thread-safe, reads cached atomics) ----

    struct ChannelMeterData
    {
        float inputRmsDb   = -120.0f;
        float gainDb       = -120.0f;
        float outputRmsDb  = -120.0f;
        float noiseFloorDb = -120.0f;
        bool  isActive     = false;
    };

    struct GlobalMeterData
    {
        float nomCount         = 0.0f;
        float nomAttenuationDb = 0.0f;
    };

    ChannelMeterData getChannelMeterData (int channel) const;
    GlobalMeterData  getGlobalMeterData() const;
    int              getActiveChannelCount() const;

    // ---- APVTS (public for Editor attachment) ----
    juce::AudioProcessorValueTreeState apvts;

private:
    AutomixEngine* engine_ = nullptr;

    // Cached parameter pointers (audio-thread-safe).
    // Populated once in constructor from apvts.getRawParameterValue().
    std::atomic<float>* channelWeightParams_[kMaxChannels]  {};
    std::atomic<float>* channelMuteParams_[kMaxChannels]    {};
    std::atomic<float>* channelSoloParams_[kMaxChannels]    {};
    std::atomic<float>* channelBypassParams_[kMaxChannels]  {};

    std::atomic<float>* globalBypassParam_ = nullptr;
    std::atomic<float>* attackMsParam_     = nullptr;
    std::atomic<float>* releaseMsParam_    = nullptr;
    std::atomic<float>* holdMsParam_       = nullptr;
    std::atomic<float>* nomAttenParam_     = nullptr;

    // Metering cache (written by audio thread, read by GUI via getters).
    std::atomic<float> meterInputRmsDb_[kMaxChannels]   {};
    std::atomic<float> meterGainDb_[kMaxChannels]       {};
    std::atomic<float> meterOutputRmsDb_[kMaxChannels]  {};
    std::atomic<float> meterNoiseFloorDb_[kMaxChannels] {};
    std::atomic<bool>  meterIsActive_[kMaxChannels]     {};

    std::atomic<float> meterNomCount_    { 0.0f };
    std::atomic<float> meterNomAttenDb_  { 0.0f };

    void syncParametersToEngine();
    void cacheMetering();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomixProcessor)
};
