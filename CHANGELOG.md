# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

## [0.1.1] - 2025-06-15

### Fixed
- Per-sample gain smoothing (was per-block, causing audible stepping)
- Noise floor smoother attack/release swap for correct tracking direction
- Rust constant naming to avoid `#define` collisions with JUCE (`AUTOMIX_` prefix)

### Changed
- `AutomixEngine::new()` returns `Box<Self>` to avoid stack overflow from 4.8MB channel array
- Internal constants use `pub(crate)` to prevent cbindgen export

## [0.1.0] - 2025-06-01

### Added
- 9-phase gain-sharing DSP engine in Rust
  - RMS level detection with sliding window
  - Adaptive noise floor tracking
  - Active channel detection with configurable threshold
  - Last-mic-hold to prevent ambient noise pumping
  - Gain-sharing with per-channel weights
  - NOM attenuation
  - One-pole gain smoothing (attack/release)
  - Per-sample gain ramp application
- C FFI boundary with opaque pointer pattern
- cbindgen header generation via `build.rs`
- JUCE 8 AU plugin wrapper (up to 32 discrete channels)
- Standalone macOS app
- CMake + Corrosion build system (universal binary: arm64 + x86_64)
- CI via GitHub Actions (Rust tests + CMake build + Catch2 tests)
- Phase 0 placeholder GUI
