# AutoMix

**Automatic microphone mixer for macOS**

![Build](https://github.com/cj-vana/automix/actions/workflows/build_and_test.yml/badge.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Version](https://img.shields.io/badge/version-0.2.0-green.svg)
![macOS](https://img.shields.io/badge/platform-macOS_12%2B-lightgrey.svg)

---

AutoMix automatically manages the gain of multiple microphone inputs using a gain-sharing algorithm. When someone talks, their mic opens up while inactive channels are attenuated — all while maintaining constant system gain. No more manual fader riding.

**Built for:** live broadcast panels, conference rooms, theater, podcasts, house of worship

## Features

- **Gain-sharing algorithm** with configurable per-channel weights
- **Up to 32 input channels** with independent controls
- **NOM (Number of Open Mics) attenuation** for feedback control
- **Adaptive noise floor tracking** that adjusts to room conditions
- **Last-mic-hold** prevents ambient noise pumping
- **Per-channel solo/mute/bypass** controls
- **Modern dark UI** with real-time metering (input, gain reduction, output)
- **AES67 network audio** receive capability (standalone mode)
- **AU plugin** for Logic Pro, GarageBand, and other AU hosts
- **Standalone app** with direct audio device I/O

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
cargo test --manifest-path rust/automix-dsp/Cargo.toml   # Rust DSP tests
ctest --test-dir build --output-on-failure                # C++ integration tests
cargo miri test --manifest-path rust/automix-dsp/Cargo.toml  # Memory safety
```

## DAW Setup

- **Standalone app** — Works out of the box with any multi-channel audio interface
- **Logic Pro** — Requires surround bus routing. See [Logic Pro Setup Guide](docs/logic-pro-setup.md)
- **REAPER** — Route tracks to a single multi-channel track with AutoMix inserted

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, branch model, and PR guidelines.

## License

MIT — see [LICENSE](LICENSE).
