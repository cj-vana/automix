#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

static void converge (TestEngine& engine, uint32_t numChannels, uint32_t numSamples, float value, int blocks)
{
    for (int i = 0; i < blocks; ++i)
    {
        TestBuffer buf (numChannels, numSamples, value);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }
}

TEST_CASE ("NaN input produces finite output", "[nan]")
{
    TestEngine engine (2, 48000.0f);
    converge (engine, 2, 256, 0.5f, 100);

    // Inject NaN
    TestBuffer buf (2, 256);
    for (auto& s : buf.channels[0])
        s = std::nanf ("");
    buf.fill (1, 0.3f);
    // Refresh pointers after fill
    for (uint32_t i = 0; i < buf.numChannels(); ++i)
        buf.ptrs[i] = buf.channels[i].data();

    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

    for (uint32_t ch = 0; ch < buf.numChannels(); ++ch)
        for (auto s : buf.channels[ch])
            REQUIRE (std::isfinite (s));
}

TEST_CASE ("Inf input produces finite output", "[nan]")
{
    TestEngine engine (2, 48000.0f);
    converge (engine, 2, 256, 0.5f, 100);

    TestBuffer buf (2, 256);
    for (auto& s : buf.channels[0])
        s = std::numeric_limits<float>::infinity();
    buf.fill (1, 0.3f);
    for (uint32_t i = 0; i < buf.numChannels(); ++i)
        buf.ptrs[i] = buf.channels[i].data();

    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

    for (uint32_t ch = 0; ch < buf.numChannels(); ++ch)
        for (auto s : buf.channels[ch])
            REQUIRE (std::isfinite (s));
}

TEST_CASE ("Negative Inf input produces finite output", "[nan]")
{
    TestEngine engine (2, 48000.0f);
    converge (engine, 2, 256, 0.5f, 100);

    TestBuffer buf (2, 256);
    for (auto& s : buf.channels[0])
        s = -std::numeric_limits<float>::infinity();
    buf.fill (1, 0.3f);
    for (uint32_t i = 0; i < buf.numChannels(); ++i)
        buf.ptrs[i] = buf.channels[i].data();

    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

    for (uint32_t ch = 0; ch < buf.numChannels(); ++ch)
        for (auto s : buf.channels[ch])
            REQUIRE (std::isfinite (s));
}

TEST_CASE ("Metering is valid after NaN injection", "[nan]")
{
    TestEngine engine (2, 48000.0f);
    converge (engine, 2, 256, 0.5f, 100);

    // Inject NaN
    TestBuffer nanBuf (2, 256);
    for (auto& s : nanBuf.channels[0])
        s = std::nanf ("");
    nanBuf.fill (1, 0.3f);
    for (uint32_t i = 0; i < nanBuf.numChannels(); ++i)
        nanBuf.ptrs[i] = nanBuf.channels[i].data();
    automix_process (engine.get(), nanBuf.data(), nanBuf.numChannels(), nanBuf.numSamples());

    // Metering should still return finite values
    AutomixChannelMetering meters[2] {};
    auto written = automix_get_all_channel_metering (engine.get(), meters, 2);
    REQUIRE (written == 2);
    for (uint32_t i = 0; i < written; ++i)
    {
        REQUIRE (std::isfinite (meters[i].input_rms_db));
        REQUIRE (std::isfinite (meters[i].gain_db));
        REQUIRE (std::isfinite (meters[i].output_rms_db));
    }

    AutomixGlobalMetering gm {};
    REQUIRE (automix_get_global_metering (engine.get(), &gm));
    REQUIRE (std::isfinite (gm.nom_count));
    REQUIRE (std::isfinite (gm.nom_attenuation_db));
}
