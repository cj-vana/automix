#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE ("Create and destroy engine", "[lifecycle]")
{
    TestEngine engine (8, 48000.0f);
    REQUIRE (engine.get() != nullptr);
}

TEST_CASE ("Create with 1 channel", "[lifecycle]")
{
    TestEngine engine (1, 44100.0f);
    REQUIRE (engine.get() != nullptr);
}

TEST_CASE ("Create with max channels", "[lifecycle]")
{
    TestEngine engine (AUTOMIX_MAX_CHANNELS, 96000.0f);
    REQUIRE (engine.get() != nullptr);
}

TEST_CASE ("Destroy null engine is safe", "[lifecycle]")
{
    automix_destroy (nullptr);
    // Should not crash
    SUCCEED();
}

TEST_CASE ("Process null engine is safe", "[lifecycle]")
{
    float sample = 0.0f;
    float* ptr = &sample;
    automix_process (nullptr, &ptr, 1, 1);
    SUCCEED();
}

TEST_CASE ("Process null channel_ptrs is safe", "[lifecycle]")
{
    TestEngine engine (2, 48000.0f);
    automix_process (engine.get(), nullptr, 2, 256);
    SUCCEED();
}

TEST_CASE ("Version string is valid", "[lifecycle]")
{
    const uint8_t* version = automix_version();
    REQUIRE (version != nullptr);

    // Version should be a non-empty null-terminated string
    const char* vstr = reinterpret_cast<const char*> (version);
    REQUIRE (std::strlen (vstr) > 0);
}
