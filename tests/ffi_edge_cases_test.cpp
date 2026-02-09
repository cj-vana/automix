#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE ("Out-of-range channel index for weight", "[edge]")
{
    TestEngine engine (4, 48000.0f);
    // Channel 100 is out of range — should not crash
    automix_set_channel_weight (engine.get(), 100, 0.5f);
    automix_set_channel_mute (engine.get(), 100, true);
    automix_set_channel_solo (engine.get(), 100, true);
    automix_set_channel_bypass (engine.get(), 100, true);
    SUCCEED();
}

TEST_CASE ("Out-of-range channel for metering", "[edge]")
{
    TestEngine engine (2, 48000.0f);
    AutomixChannelMetering cm {};
    bool ok = automix_get_channel_metering (engine.get(), 99, &cm);
    REQUIRE_FALSE (ok);
}

TEST_CASE ("Process with more channels than engine", "[edge]")
{
    TestEngine engine (2, 48000.0f);
    // Provide 4 channels but engine only has 2
    TestBuffer buf (4, 256, 0.5f);
    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    SUCCEED();
}

TEST_CASE ("Rapid create-destroy cycles", "[edge]")
{
    for (int i = 0; i < 100; ++i)
    {
        TestEngine engine (8, 48000.0f);
        TestBuffer buf (8, 256, 0.1f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }
    SUCCEED();
}

TEST_CASE ("Multiple engines simultaneously", "[edge]")
{
    TestEngine engine1 (2, 48000.0f);
    TestEngine engine2 (4, 96000.0f);
    TestEngine engine3 (8, 44100.0f);

    TestBuffer buf1 (2, 256, 0.3f);
    TestBuffer buf2 (4, 256, 0.5f);
    TestBuffer buf3 (8, 256, 0.1f);

    automix_process (engine1.get(), buf1.data(), buf1.numChannels(), buf1.numSamples());
    automix_process (engine2.get(), buf2.data(), buf2.numChannels(), buf2.numSamples());
    automix_process (engine3.get(), buf3.data(), buf3.numChannels(), buf3.numSamples());
    SUCCEED();
}
