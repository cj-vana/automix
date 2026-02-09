# Contributing to AutoMix

Thanks for your interest in contributing! Here's how to get started.

## Prerequisites

- macOS 12+
- [Homebrew](https://brew.sh)
- Rust toolchain (`curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`)

```bash
brew install cmake ninja
cargo install cbindgen
```

## Building

```bash
git clone --recursive https://github.com/cj-vana/automix.git
cd automix
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use the dev script to build and launch the standalone app:

```bash
./dev.sh
```

## Testing

```bash
# Rust DSP unit tests
cargo test --manifest-path rust/automix-dsp/Cargo.toml

# C++ integration tests (requires full build)
ctest --test-dir build --output-on-failure

# Memory safety checks
cargo miri test --manifest-path rust/automix-dsp/Cargo.toml
```

## Branch Model

- **`main`** — stable, release-ready code
- **`dev`** — active development

All feature work should branch from `dev` and target `dev` via pull request.

## Pull Requests

1. Fork the repo and create your branch from `dev`
2. Make your changes
3. Ensure all tests pass (`cargo test` + `ctest`)
4. Fill out the PR template
5. Submit your pull request against `dev`

## Code Style

- **Rust**: Follow `rustfmt` defaults
- **C++**: C++20, follow existing patterns in `source/`
- Keep the FFI boundary minimal — Rust owns DSP, C++ owns plugin lifecycle

## Reporting Issues

Use [GitHub Issues](https://github.com/cj-vana/automix/issues) with the provided templates.
