#pragma once

#include <cmath>
#include <cstring>
#include <vector>

extern "C"
{
    #include "automix_dsp.h"
}

/// RAII wrapper for AutomixEngine — automatically destroys on scope exit.
struct TestEngine
{
    AutomixEngine* engine = nullptr;

    TestEngine (uint32_t numChannels, float sampleRate, uint32_t maxBlockSize = 256)
        : engine (automix_create (numChannels, sampleRate, maxBlockSize))
    {
    }

    ~TestEngine()
    {
        if (engine != nullptr)
            automix_destroy (engine);
    }

    TestEngine (const TestEngine&) = delete;
    TestEngine& operator= (const TestEngine&) = delete;

    AutomixEngine* get() { return engine; }
    const AutomixEngine* get() const { return engine; }
};

/// Helper to create and manage audio test buffers.
struct TestBuffer
{
    std::vector<std::vector<float>> channels;
    std::vector<float*> ptrs;

    TestBuffer (uint32_t numChannels, uint32_t numSamples, float fillValue = 0.0f)
    {
        channels.resize (numChannels);
        ptrs.resize (numChannels);
        for (uint32_t i = 0; i < numChannels; ++i)
        {
            channels[i].assign (numSamples, fillValue);
            ptrs[i] = channels[i].data();
        }
    }

    /// Fill a specific channel with a value.
    void fill (uint32_t channel, float value)
    {
        std::fill (channels[channel].begin(), channels[channel].end(), value);
    }

    float* const* data() { return ptrs.data(); }
    uint32_t numChannels() const { return static_cast<uint32_t> (channels.size()); }
    uint32_t numSamples() const { return channels.empty() ? 0 : static_cast<uint32_t> (channels[0].size()); }
};

/// Run `blocks` blocks of steady input through the engine so the gain smoothers
/// and the noise floor settle.
///
/// Processing is in place, so the buffer has to be refilled every block.
/// Re-processing the same buffer feeds the previous block's already-attenuated
/// output back in, which converges somewhere else entirely.
inline void converge (TestEngine& engine,
                      uint32_t numChannels,
                      uint32_t numSamples,
                      float level,
                      int blocks)
{
    for (int i = 0; i < blocks; ++i)
    {
        TestBuffer buf (numChannels, numSamples, level);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }
}
