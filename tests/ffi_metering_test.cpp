#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "test_helpers.h"

TEST_CASE ("Channel metering returns valid data", "[metering]")
{
    TestEngine engine (2, 48000.0f);

    // Process some audio to generate metering data
    for (int i = 0; i < 100; ++i)
    {
        TestBuffer buf (2, 256);
        buf.fill (0, 0.5f);
        buf.fill (1, 0.1f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    AutomixChannelMetering metering {};
    bool ok = automix_get_channel_metering (engine.get(), 0, &metering);
    REQUIRE (ok);

    // Values should be finite (no NaN or inf)
    REQUIRE (std::isfinite (metering.input_rms_db));
    REQUIRE (std::isfinite (metering.gain_db));
    REQUIRE (std::isfinite (metering.output_rms_db));
    REQUIRE (std::isfinite (metering.noise_floor_db));
}

TEST_CASE ("Global metering returns valid data", "[metering]")
{
    TestEngine engine (2, 48000.0f);

    for (int i = 0; i < 50; ++i)
    {
        TestBuffer buf (2, 256, 0.3f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    AutomixGlobalMetering gm {};
    bool ok = automix_get_global_metering (engine.get(), &gm);
    REQUIRE (ok);
    REQUIRE (std::isfinite (gm.nom_count));
    REQUIRE (std::isfinite (gm.nom_attenuation_db));
    REQUIRE (gm.nom_count >= 0.0f);
}

TEST_CASE ("Get all channel metering", "[metering]")
{
    TestEngine engine (4, 48000.0f);

    for (int i = 0; i < 50; ++i)
    {
        TestBuffer buf (4, 256, 0.2f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    AutomixChannelMetering meters[AUTOMIX_MAX_CHANNELS] {};
    uint32_t count = automix_get_all_channel_metering (engine.get(), meters, 4);
    REQUIRE (count == 4);

    for (uint32_t i = 0; i < count; ++i)
    {
        REQUIRE (std::isfinite (meters[i].input_rms_db));
        REQUIRE (std::isfinite (meters[i].gain_db));
    }
}

TEST_CASE ("Metering with null engine returns false", "[metering]")
{
    AutomixChannelMetering cm {};
    REQUIRE_FALSE (automix_get_channel_metering (nullptr, 0, &cm));

    AutomixGlobalMetering gm {};
    REQUIRE_FALSE (automix_get_global_metering (nullptr, &gm));

    REQUIRE (automix_get_all_channel_metering (nullptr, &cm, 1) == 0);
}

TEST_CASE ("Metering with null output pointer returns false", "[metering]")
{
    TestEngine engine (2, 48000.0f);
    REQUIRE_FALSE (automix_get_channel_metering (engine.get(), 0, nullptr));
    REQUIRE_FALSE (automix_get_global_metering (engine.get(), nullptr));
    REQUIRE (automix_get_all_channel_metering (engine.get(), nullptr, 2) == 0);
}
