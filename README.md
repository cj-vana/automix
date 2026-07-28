# AutoMix

**Automatic microphone mixer for macOS**

![Build](https://github.com/cj-vana/automix/actions/workflows/build_and_test.yml/badge.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Version](https://img.shields.io/badge/version-0.3.1-green.svg)
![macOS](https://img.shields.io/badge/platform-macOS_12%2B-lightgrey.svg)

---

AutoMix automatically manages the gain of multiple microphone inputs using a gain-sharing algorithm. When someone talks, their mic opens up while inactive channels are attenuated — all while maintaining constant system gain. No more manual fader riding.

**Built for:** live broadcast panels, conference rooms, theater, podcasts, house of worship

![AutoMix mixer view](docs/images/automix-mixer.png)

The gain-share bar across the top is the algorithm made visible: one segment per open
mic, sized to its share of the mix. The shares always sum to 1.000, which is what holds
the system gain constant however many mics are open. Below it, each channel shows input
level and gain reduction side by side, and the history lane keeps the last 30 seconds of
who was open.

The window runs from 800×400 to 2400×1400. The secondary rows drop out as height runs
short, leaving the mixer bay.

<details>
<summary>Compact (800×400) and 32-channel views</summary>

![Compact view](docs/images/automix-compact.png)

![32 channels](docs/images/automix-32ch.png)

</details>

## Features

- **Gain-sharing algorithm** with configurable per-channel weights
- **Up to 32 input channels** with independent controls
- **Adaptive noise floor tracking** that adjusts to room conditions
- **Last-mic-hold** prevents ambient noise pumping
- **Per-channel weight, solo, mute, and bypass**
- **Console-style metering** with per-channel input level, gain reduction, and an open-mic
  indicator, plus a running count of how many mics are open
- **Click-free bypass**, global and per channel, crossfaded rather than switched, so it is
  safe to hit during a show
- **AU plugin** for AU hosts, and a **standalone app** with direct audio device I/O

Optional NOM (number of open mics) attenuation is available but off by default. Gain sharing
already normalises the channel gains to sum to unity, which is what holds the loop gain
constant as more mics open. Layering the classic `-10*log10(NOM)` on top attenuates a second
time and drops the mix further with every talker. Turn it on only if you want to trade level
for extra feedback margin.

## Architecture

```
┌─────────────────────────────────────────────────┐
│  JUCE 8 (C++)                                   │
│  Plugin wrapper · GUI · Audio I/O               │
│                                                 │
│    float* const* ──► FFI ──► float* const*      │
│                                                 │
│  ┌─────────────────────────────────────────┐    │
│  │  Rust DSP Core                          │    │
│  │  9-phase gain-sharing pipeline          │    │
│  │  Zero allocation · Memory safe          │    │
│  └─────────────────────────────────────────┘    │
└─────────────────────────────────────────────────┘
```

## Quick Start

```bash
# Prerequisites
brew install cmake ninja
cargo install cbindgen

# Clone, build, and run
git clone --recursive https://github.com/cj-vana/automix.git
cd automix
./dev.sh
```

## Building

```bash
# Full build (universal binary: arm64 + x86_64)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Output

| Artifact | Path |
|----------|------|
| AU Plugin | `build/AutoMix_artefacts/Release/AU/AutoMix.component` |
| Standalone App | `build/AutoMix_artefacts/Release/Standalone/AutoMix.app` |

### Testing

```bash
# Rust DSP tests
cargo test --manifest-path rust/automix-dsp/Cargo.toml

# C++ FFI tests. These only exist if the build was configured with testing on,
# which is the default; ctest reports "No tests were found" otherwise.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure

# Memory safety. miri is nightly-only and the repo pins stable, so ask for the
# toolchain explicitly:
#   rustup toolchain install nightly && rustup +nightly component add miri
cargo +nightly miri test --manifest-path rust/automix-dsp/Cargo.toml
```

### Editor screenshots

The interface renders offscreen, so a design change can be checked without launching the
standalone app and fighting the window manager for a screenshot. Output is deterministic:
the same arguments always produce a byte-identical PNG.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAUTOMIX_BUILD_SNAPSHOT=ON
cmake --build build --target AutoMixSnapshot

# <out.png> [width] [height] [channels]
./build/AutoMixSnapshot_artefacts/Release/AutoMixSnapshot docs/images/automix-mixer.png 1200 700 16
```

## DAW Setup

- **Standalone app (recommended)** — Works out of the box with any multi-channel audio interface
- **REAPER** — Native multi-channel routing. Route tracks to a single multi-channel track with AutoMix inserted
- **Logic Pro** — Use the standalone app; Logic's routing model doesn't support multi-channel effect plugins well. See [DAW Setup Guide](docs/logic-pro-setup.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, branch model, and PR guidelines.

## License

MIT — see [LICENSE](LICENSE).
