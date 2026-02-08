# AutoMix

A professional Dugan-style automatic microphone mixer for macOS, available as an AU plugin and standalone application.

## What is AutoMix?

AutoMix automatically manages the gain of multiple microphone inputs using the Dugan gain-sharing algorithm. When a speaker talks into their microphone, that channel's gain increases while inactive channels are attenuated -- all while maintaining constant system gain. This eliminates manual fader riding in:

- Live broadcast panels
- Conference rooms
- Theater productions
- Podcast recording
- House of worship

## Features

- **Dugan gain-sharing algorithm** with configurable per-channel weights
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

AutoMix uses a hybrid architecture:
- **JUCE 8** (C++) for the plugin wrapper, GUI, and audio I/O
- **Rust** for the DSP core via C FFI -- memory-safe, zero-allocation audio processing

## Building

### Prerequisites

```bash
brew install cmake ninja
cargo install cbindgen
```

### Build

```bash
git clone --recursive https://github.com/yourusername/automix.git
cd automix
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Test

```bash
# Rust DSP unit tests
cargo test --manifest-path rust/automix-dsp/Cargo.toml

# C++ integration tests
ctest --test-dir build --output-on-failure
```

### Output

After building, you'll find:
- **AU Plugin**: `build/AutoMix_artefacts/Release/AU/AutoMix.component`
- **Standalone App**: `build/AutoMix_artefacts/Release/Standalone/AutoMix.app`

## License

MIT License. See [LICENSE](LICENSE) for details.
