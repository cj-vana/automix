#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>

TEST_CASE ("Process passes audio through single channel", "[process]")
{
    TestEngine engine (1, 48000.0f);
    TestBuffer buf (1, 256, 0.5f);

    // Process many blocks to let gain converge
    for (int i = 0; i < 200; ++i)
    {
        buf.fill (0, 0.5f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    // After convergence, output should be close to input (single channel gain ≈ 1.0)
    float lastSample = buf.channels[0].back();
    REQUIRE_THAT (lastSample, Catch::Matchers::WithinAbs (0.5f, 0.05f));
}

TEST_CASE ("Process with two channels", "[process]")
{
    TestEngine engine (2, 48000.0f);
    TestBuffer buf (2, 256);

    for (int i = 0; i < 200; ++i)
    {
        buf.fill (0, 0.8f);
        buf.fill (1, 0.2f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    // Louder channel should have larger output
    float ch0 = std::abs (buf.channels[0].back());
    float ch1 = std::abs (buf.channels[1].back());
    REQUIRE (ch0 > ch1);
}

TEST_CASE ("Global bypass leaves audio unmodified once engaged", "[process]")
{
    TestEngine engine (2, 48000.0f);
    automix_set_global_bypass (engine.get(), true);

    // Bypass crossfades over 15 ms rather than switching, so it needs a few
    // 256-sample blocks at 48 kHz before it is fully engaged.
    for (int i = 0; i < 8; ++i)
    {
        TestBuffer warmup (2, 256, 0.0f);
        warmup.fill (0, 0.5f);
        warmup.fill (1, 0.3f);
        automix_process (engine.get(), warmup.data(), warmup.numChannels(), warmup.numSamples());
    }

    TestBuffer buf (2, 256, 0.0f);
    buf.fill (0, 0.5f);
    buf.fill (1, 0.3f);

    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

    for (uint32_t i = 0; i < buf.numSamples(); ++i)
    {
        REQUIRE (buf.channels[0][i] == 0.5f);
        REQUIRE (buf.channels[1][i] == 0.3f);
    }
}

TEST_CASE ("Global bypass transition is click-free", "[process]")
{
    TestEngine engine (2, 48000.0f);

    // Settle into a processed state where the gain is well below unity.
    for (int i = 0; i < 400; ++i)
    {
        TestBuffer warmup (2, 256, 0.5f);
        automix_process (engine.get(), warmup.data(), warmup.numChannels(), warmup.numSamples());
    }

    automix_set_global_bypass (engine.get(), true);

    // A hard switch would step the output by most of its amplitude in a single
    // sample. Ramped over 720 samples, no adjacent pair may jump that far.
    for (int block = 0; block < 8; ++block)
    {
        TestBuffer buf (2, 256, 0.5f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

        for (uint32_t i = 1; i < buf.numSamples(); ++i)
        {
            const float delta = std::abs (buf.channels[0][i] - buf.channels[0][i - 1]);
            REQUIRE (delta < 0.01f);
        }
    }
}

TEST_CASE ("Process with zero samples is safe", "[process]")
{
    TestEngine engine (2, 48000.0f);
    TestBuffer buf (2, 0);
    automix_process (engine.get(), buf.data(), buf.numChannels(), 0);
    SUCCEED();
}

TEST_CASE ("Process with one sample", "[process]")
{
    TestEngine engine (1, 48000.0f);
    TestBuffer buf (1, 1, 0.5f);
    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    SUCCEED();
}

TEST_CASE ("Process with max block size", "[process]")
{
    TestEngine engine (2, 48000.0f);
    TestBuffer buf (2, AUTOMIX_MAX_BLOCK_SIZE, 0.1f);
    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    SUCCEED();
}

TEST_CASE ("Silence produces silence output", "[process]")
{
    TestEngine engine (2, 48000.0f);

    for (int i = 0; i < 100; ++i)
    {
        TestBuffer buf (2, 256, 0.0f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

        for (uint32_t ch = 0; ch < 2; ++ch)
            for (uint32_t s = 0; s < 256; ++s)
                REQUIRE (buf.channels[ch][s] == 0.0f);
    }
}
