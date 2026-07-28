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
cargo +nightly miri test --manifest-path rust/automix-dsp/Cargo.toml
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

## Releases

Tags are cut by hand. An earlier workflow created them automatically when `VERSION`
changed, but it pushed with the default `GITHUB_TOKEN`, and GitHub deliberately does not
fire workflow triggers for pushes made with that token. Every tag it created was therefore
one no release build could ever run from, and it raced anyone tagging manually. It was
removed rather than repaired, since fixing it needs a personal access token stored as a
secret.

To cut a release:

```bash
# 1. Bump the version in all three places. CI fails the build if they disagree.
#    VERSION, rust/automix-dsp/Cargo.toml, and the README badge.
#    Then refresh Cargo.lock:
cargo build --manifest-path rust/automix-dsp/Cargo.toml

# 2. Add a CHANGELOG entry for the new version.

# 3. Land it on main, then tag from your own machine so the push triggers CI:
git tag -a v0.3.0 -m "AutoMix v0.3.0"
git push origin v0.3.0
```

The tag push runs `build_and_test.yml`, which builds the universal binaries, runs both test
suites, and publishes a GitHub Release with the AU and standalone archives attached.

Artifact upload to GitHub occasionally times out. If the run fails on
`Failed to FinalizeArtifact`, re-run the failed job; nothing about the build is wrong.

## Reporting Issues

Use [GitHub Issues](https://github.com/cj-vana/automix/issues) with the provided templates.
