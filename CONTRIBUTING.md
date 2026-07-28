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
suites, signs and notarizes the bundles, and publishes a GitHub Release with the archives
attached.

### Signing and notarization

Tag builds are signed with a Developer ID certificate and notarized by Apple. Branch and PR
builds are not: the secrets are unavailable to forks, so those produce unsigned archives and
the signing steps are skipped rather than failing.

Five repository secrets drive it:

| Secret | What |
| --- | --- |
| `APPLE_CERT_P12` | Developer ID Application certificate and key, base64 of a `.p12` |
| `APPLE_CERT_PASSWORD` | Password the `.p12` was exported with |
| `APPLE_API_KEY_P8` | Contents of the App Store Connect `AuthKey_XXXX.p8` |
| `APPLE_API_KEY_ID` | The `XXXX` from that filename |
| `APPLE_API_ISSUER_ID` | Issuer UUID, from App Store Connect → Users and Access → Integrations |
| `APPLE_TEAM_ID` | Ten-character team identifier |

The release job refuses to publish anything that is not stapled, so a missing or expired
credential fails the build instead of quietly shipping something Gatekeeper blocks.

To sign locally, which is worth doing before trusting a release:

```bash
export SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export NOTARY_KEY=~/.appstoreconnect/private_keys/AuthKey_XXXX.p8
export NOTARY_KEY_ID=XXXX
export NOTARY_ISSUER_ID=<uuid>

./packaging/sign-and-notarize.sh \
  build/AutoMix_artefacts/Release/Standalone/AutoMix.app \
  AutoMix-Standalone-macOS.zip
```

Omit the three `NOTARY_*` variables to sign without notarizing, which is fast and enough to
check that the entitlements and hardened runtime are right.

The certificate expires. When it does, export a new one and update the two cert secrets;
nothing else changes.

Artifact upload to GitHub occasionally times out. If the run fails on
`Failed to FinalizeArtifact`, re-run the failed job; nothing about the build is wrong.

## Reporting Issues

Use [GitHub Issues](https://github.com/cj-vana/automix/issues) with the provided templates.
