#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE ("Set channel weight", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_channel_weight (engine.get(), 0, 0.5f);
    automix_set_channel_weight (engine.get(), 1, 0.0f);
    automix_set_channel_weight (engine.get(), 2, 1.0f);
    SUCCEED();
}

TEST_CASE ("Set channel mute", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_channel_mute (engine.get(), 0, true);
    automix_set_channel_mute (engine.get(), 0, false);
    SUCCEED();
}

TEST_CASE ("Set channel solo", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_channel_solo (engine.get(), 0, true);
    automix_set_channel_solo (engine.get(), 1, true);
    automix_set_channel_solo (engine.get(), 0, false);
    SUCCEED();
}

TEST_CASE ("Set channel bypass", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_channel_bypass (engine.get(), 0, true);
    automix_set_channel_bypass (engine.get(), 0, false);
    SUCCEED();
}

TEST_CASE ("Set global bypass", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_global_bypass (engine.get(), true);
    automix_set_global_bypass (engine.get(), false);
    SUCCEED();
}

TEST_CASE ("Set attack and release", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_attack_ms (engine.get(), 10.0f);
    automix_set_release_ms (engine.get(), 200.0f);
    SUCCEED();
}

TEST_CASE ("Set hold time", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_hold_time_ms (engine.get(), 1000.0f);
    automix_set_hold_time_ms (engine.get(), 0.0f);
    SUCCEED();
}

TEST_CASE ("Set NOM attenuation enabled", "[parameters]")
{
    TestEngine engine (4, 48000.0f);
    automix_set_nom_atten_enabled (engine.get(), false);
    automix_set_nom_atten_enabled (engine.get(), true);
    SUCCEED();
}

TEST_CASE ("Parameter setters with null engine are safe", "[parameters]")
{
    automix_set_channel_weight (nullptr, 0, 0.5f);
    automix_set_channel_mute (nullptr, 0, true);
    automix_set_channel_solo (nullptr, 0, true);
    automix_set_channel_bypass (nullptr, 0, true);
    automix_set_global_bypass (nullptr, true);
    automix_set_attack_ms (nullptr, 10.0f);
    automix_set_release_ms (nullptr, 200.0f);
    automix_set_hold_time_ms (nullptr, 500.0f);
    automix_set_nom_atten_enabled (nullptr, true);
    SUCCEED();
}

// ---- Behavioral parameter tests ----

static void converge (TestEngine& engine, TestBuffer& buf, int blocks)
{
    for (int i = 0; i < blocks; ++i)
    {
        // Re-fill with original values each iteration
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }
}

TEST_CASE ("Mute silences channel output", "[parameters][behavioral]")
{
    TestEngine engine (2, 48000.0f);
    automix_set_channel_mute (engine.get(), 1, true);

    // Converge
    for (int i = 0; i < 200; ++i)
    {
        TestBuffer buf (2, 256, 0.5f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    TestBuffer finalBuf (2, 256, 0.5f);
    automix_process (engine.get(), finalBuf.data(), finalBuf.numChannels(), finalBuf.numSamples());

    float mutedOut = std::abs (finalBuf.channels[1].back());
    REQUIRE (mutedOut < 0.01f);
}

TEST_CASE ("Global bypass passes audio through", "[parameters][behavioral]")
{
    TestEngine engine (2, 48000.0f);
    automix_set_global_bypass (engine.get(), true);

    TestBuffer buf (2, 256, 0.5f);
    buf.fill (1, 0.3f);
    automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());

    // Audio should be completely unmodified
    for (auto s : buf.channels[0])
        REQUIRE (s == 0.5f);
    for (auto s : buf.channels[1])
        REQUIRE (s == 0.3f);
}

TEST_CASE ("Channel bypass preserves unity gain", "[parameters][behavioral]")
{
    TestEngine engine (2, 48000.0f);
    automix_set_channel_bypass (engine.get(), 0, true);

    // Converge
    for (int i = 0; i < 50; ++i)
    {
        TestBuffer buf (2, 256, 0.5f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    TestBuffer finalBuf (2, 256, 0.5f);
    automix_process (engine.get(), finalBuf.data(), finalBuf.numChannels(), finalBuf.numSamples());

    float bypassedOut = finalBuf.channels[0].back();
    REQUIRE (std::abs (bypassedOut - 0.5f) < 0.01f);
}

TEST_CASE ("Weight affects gain distribution", "[parameters][behavioral]")
{
    TestEngine engine (2, 48000.0f);
    automix_set_channel_weight (engine.get(), 0, 1.0f);
    automix_set_channel_weight (engine.get(), 1, 0.1f);

    // Converge with equal-level input
    for (int i = 0; i < 200; ++i)
    {
        TestBuffer buf (2, 256, 0.5f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    TestBuffer finalBuf (2, 256, 0.5f);
    automix_process (engine.get(), finalBuf.data(), finalBuf.numChannels(), finalBuf.numSamples());

    float ch0Out = std::abs (finalBuf.channels[0].back());
    float ch1Out = std::abs (finalBuf.channels[1].back());
    REQUIRE (ch0Out > ch1Out);
}

TEST_CASE ("Solo isolates channel", "[parameters][behavioral]")
{
    TestEngine engine (2, 48000.0f);
    automix_set_channel_solo (engine.get(), 0, true);

    // Converge
    for (int i = 0; i < 200; ++i)
    {
        TestBuffer buf (2, 256, 0.5f);
        automix_process (engine.get(), buf.data(), buf.numChannels(), buf.numSamples());
    }

    TestBuffer finalBuf (2, 256, 0.5f);
    automix_process (engine.get(), finalBuf.data(), finalBuf.numChannels(), finalBuf.numSamples());

    float soloedOut = std::abs (finalBuf.channels[0].back());
    float nonSoloedOut = std::abs (finalBuf.channels[1].back());
    REQUIRE (soloedOut > 0.1f);
    REQUIRE (nonSoloedOut < 0.01f);
}
