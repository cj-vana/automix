# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Dugan-style automixer: AU plugin + standalone macOS app.
Hybrid architecture: JUCE 8 (C++) for plugin/GUI + Rust for DSP via C FFI.

## Build Commands

Prerequisites: `brew install cmake ninja` and `cargo install cbindgen`

```bash
# Full build (universal binary: arm64 + x86_64)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Rust DSP tests only
cargo test --manifest-path rust/automix-dsp/Cargo.toml

# C++ integration tests (requires full build first)
ctest --test-dir build --output-on-failure

# UB detection
cargo miri test --manifest-path rust/automix-dsp/Cargo.toml
```

Build artifacts:
- AU: `build/AutoMix_artefacts/Release/AU/AutoMix.component` (auto-copies to `~/Library/Audio/Plug-Ins/Components/`)
- Standalone: `build/AutoMix_artefacts/Release/Standalone/AutoMix.app`

## Architecture

### FFI Boundary (C++ ↔ Rust)
The central design constraint. C++ owns the plugin lifecycle; Rust owns all DSP math.

- **Opaque pointer pattern**: C++ holds `AutomixEngine*`, created/destroyed via `automix_create`/`automix_destroy`
- **Zero-copy audio**: Buffers cross FFI as `float* const*` (matches JUCE `AudioBuffer::getArrayOfWritePointers()`)
- **Header generation**: cbindgen runs via `build.rs` during `cargo build`, outputs to `rust/automix-dsp/include/automix_dsp.h`
- **cbindgen config**: `rust/automix-dsp/cbindgen.toml` (C language mode, `AUTOMIX_DSP_H` guard)
- The generated header is included in C++ via `extern "C" { #include "automix_dsp.h" }`

### Naming Convention for Rust→C Exports
Rust `pub const` values become `#define` macros in the generated header. **Always prefix with `AUTOMIX_`** to avoid collisions with JUCE/C++ identifiers (e.g., `AUTOMIX_MAX_CHANNELS`, not `MAX_CHANNELS`). In C++, reference via `static constexpr int kMaxChannels = AUTOMIX_MAX_CHANNELS;`

### Audio Thread Safety
- Fixed-size arrays `[T; 32]` in Rust — zero heap allocation in the process path
- `std::atomic<float>` for metering data (audio thread → GUI thread)
- APVTS parameters synced to Rust engine via FFI setters on the audio thread

### Plugin Configuration
- Formats: AU + Standalone (no VST3)
- Up to 32 discrete channels via `juce::AudioChannelSet::discreteChannels(kMaxChannels)`
- Input/output bus layout must match (`isBusesLayoutSupported` enforces symmetry)
- Version read from root `VERSION` file by CMake

### DSP Algorithm (Dugan Gain-Sharing)
9-phase per-block pipeline:
1. RMS level detection (sliding window)
2. Adaptive noise floor tracking
3. Active channel detection
4. Last-mic-hold evaluation
5. Gain-sharing: `gain[i] = (rms[i] * weight[i]) / sum(all)`
6. NOM attenuation: `-10*log10(NOM)` dB
7. One-pole gain smoothing (attack/release)
8. Per-sample gain ramp application
9. Counter update

### Key Files
- `source/PluginProcessor.cpp` — JUCE processor: creates/destroys Rust engine in `prepareToPlay`/`releaseResources`, calls `automix_process` in `processBlock`
- `source/PluginEditor.cpp` — GUI (currently Phase 0 placeholder)
- `rust/automix-dsp/src/lib.rs` — `AutomixEngine` struct and core algorithm
- `rust/automix-dsp/src/ffi.rs` — `#[no_mangle] extern "C"` wrapper functions
- `rust/automix-dsp/build.rs` — cbindgen header generation
- `cmake/CompilerWarnings.cmake` — shared warning flags (`set_automix_warnings(target)`)

## CI
GitHub Actions (`.github/workflows/build_and_test.yml`): runs on `main` and `dev` branches. Rust tests first, then full CMake build + Catch2 tests. Tag `v*` triggers release with AU/Standalone zips.

## Testing
- **Rust**: `cargo test` for unit tests, `proptest` for invariants (gain sum ≈ 1.0)
- **C++**: Catch2 for plugin instantiation, parameter layout, bus validation
- **Plugin validation**: pluginval at strictness 10
- **Memory safety**: `cargo miri test`

## macOS Build
- Universal binary (arm64 + x86_64) built by driving `cargo build` for each Rust target and combining with `lipo`
- Corrosion was removed — it doesn't support macOS universal binaries. CMake drives cargo directly.
- `CMAKE_OSX_ARCHITECTURES` must be set **before** `project()` so CMake picks it up
- CMake adds `-framework Security -framework CoreFoundation` (required by Rust std)
- Both Rust targets must be installed: `rustup target add x86_64-apple-darwin aarch64-apple-darwin`
