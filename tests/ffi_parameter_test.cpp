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
