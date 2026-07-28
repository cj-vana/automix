# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

## [0.2.0] - 2026-07-27

### Fixed
- Noise floor tracking ran per-sample filter coefficients against a once-per-block update, so
  adaptation was 64x to 2048x slower than intended and varied with the host's buffer size. The
  floor stayed near its initial value, which turned activity detection into a fixed gate at
  about -54 dBFS and stopped quiet talkers opening. Coefficients are now compounded over the
  block length.
- Noise floor is tracked in dB rather than linear amplitude. A few percent of linear travel is
  tens of dB when the gap is wide, so the "slow" tracker still lurched.
- Noise floor could only ever fall. If a room's ambient level rose past the activity margin,
  every channel read as open permanently with no way to recover.
- NOM attenuation was applied on top of gains that already sum to unity, attenuating the mix a
  second time. Four open mics landed 6 dB low, and the error grew with each talker. It is now
  off by default and available as an explicit extra.
- Global and per-channel bypass switched instantly, which clicked. Both now crossfade over
  15 ms. The pipeline keeps running while bypassed so meters stay live and un-bypassing
  resumes from the correct gain.
- A single large-but-finite sample permanently corrupted the RMS running sum, parking a
  channel's measured level wrong for good. Values are clamped and the sum is rebuilt once per
  window.
- `getStateInformation` dereferenced a possibly-null XML pointer.
- Metering slots above the current channel count kept stale values, drawing ghost channels
  after the host narrowed the layout.
- `BUILD_TESTING` was read before anything defined it, so a default configure built no tests
  and `ctest` reported success having run none.
- The release job could never trigger: the workflow had no tag filter.
- Release archives lost the executable bit, so the published app could not launch.

### Added
- Broadcast-console interface, replacing a placeholder that drew one line of text.
  - Gain-share distribution bar: one segment per open mic, sized to its share. The shares
    are normalised to sum to 1.0, so the bar is always full and only its division moves.
  - Per-channel strips: input level and gain reduction as adjacent vertical bars, gain
    readout, weight, and solo/mute/bypass.
  - Aggregate readouts for open-mic count, NOM attenuation, system gain, and the adaptive
    noise floor.
  - Thirty-second open-mic history, so it is possible to see after the fact whether a mic
    ever opened or was chattering.
  - Archivo and IBM Plex Mono are compiled in, subset to the glyphs the panel draws
    (46 KB for five faces). Both SIL OFL 1.1; licences in `assets/fonts`.
  - Resizes 800×400 to 2400×1400, dropping secondary rows as height runs short.
- `getBypassParameter()` override, so host bypass drives the crossfade instead of hard-
  switching the plugin out of the signal path.
- Meters fall to the floor when the host stops processing, rather than freezing at the last
  reading.
- Offscreen snapshot tool (`-DAUTOMIX_BUILD_SNAPSHOT=ON`) that renders the editor to a PNG
  at any size and channel count, with meters driven by synthetic audio. Deterministic
  output, so it works as a visual regression check.

### Changed
- Build drives `cargo` and `lipo` directly for universal binaries. Corrosion was removed
  because it does not support macOS universal builds.
- The algorithm is described generically as gain sharing. The commercial automixer this was
  previously named after is a trademark the project makes no claim to.

### Notes
- Release binaries are ad-hoc signed and **not** notarized. macOS Gatekeeper will warn on
  first open; see the release notes for how to get past it.

## [0.1.1] - 2026-02-09

### Fixed
- Per-sample gain smoothing (was per-block, causing audible stepping)
- Noise floor smoother attack/release swap for correct tracking direction
- Rust constant naming to avoid `#define` collisions with JUCE (`AUTOMIX_` prefix)

### Changed
- `AutomixEngine::new()` returns `Box<Self>` to avoid stack overflow from 4.8MB channel array
- Internal constants use `pub(crate)` to prevent cbindgen export

## [0.1.0] - 2026-02-08

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
- CMake build system (universal binary: arm64 + x86_64)
- CI via GitHub Actions (Rust tests + CMake build + Catch2 tests)
- Placeholder GUI
