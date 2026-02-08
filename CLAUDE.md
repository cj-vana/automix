# AutoMix - AI Assistant Context

## Project Overview
Dugan-style automixer: AU plugin + standalone macOS app.
Hybrid architecture: JUCE 8 (C++) for plugin/GUI + Rust for DSP via C FFI.

## Tech Stack
- **C++20** with JUCE 8 framework
- **Rust** (stable) for DSP core, compiled as staticlib
- **CMake 3.25+** with Corrosion for Rust integration
- **cbindgen** generates C headers from Rust FFI
- **Catch2 v3** for C++ tests
- **proptest** for Rust property-based tests

## Build Commands
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cargo test --manifest-path rust/automix-dsp/Cargo.toml
ctest --test-dir build --output-on-failure
```

## Key Architecture Decisions
- Audio buffers cross FFI as `float**` (zero-copy, matches JUCE AudioBuffer)
- Opaque pointer pattern for Rust engine handle in C++
- Fixed-size arrays `[T; 32]` in Rust -- zero heap allocation in process path
- Block-rate gain computation with per-sample linear interpolation
- `std::atomic<float>` for metering data (audio thread -> GUI thread)
- APVTS parameters synced to Rust engine via FFI setters on audio thread

## Directory Layout
- `source/` - C++ JUCE plugin code
- `rust/automix-dsp/` - Rust DSP crate (gain-sharing algorithm)
- `rust/automix-aes67/` - Rust AES67 receive crate
- `tests/` - Catch2 C++ integration tests
- `cmake/` - CMake helper modules

## DSP Algorithm (Dugan Gain-Sharing)
9-phase per-block pipeline:
1. RMS level detection (sliding window)
2. Adaptive noise floor tracking
3. Active channel detection
4. Last-mic-hold evaluation
5. Gain-sharing: gain[i] = (rms[i] * weight[i]) / sum(all)
6. NOM attenuation: -10*log10(NOM) dB
7. One-pole gain smoothing (attack/release)
8. Per-sample gain ramp application
9. Counter update

## Testing Strategy
- Rust: `cargo test` for unit tests, `proptest` for invariants (gain sum = 1.0)
- C++: Catch2 for plugin instantiation, parameter layout, bus validation
- Plugin: pluginval at strictness 10
- Memory: `cargo miri test` for UB detection
