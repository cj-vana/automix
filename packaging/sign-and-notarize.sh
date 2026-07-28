#!/usr/bin/env bash
#
# Sign, notarize, and staple a macOS bundle, then zip it for distribution.
#
# Works the same locally and in CI. Locally it uses whatever Developer ID
# identity is in your keychain; in CI the workflow imports one into a temporary
# keychain first.
#
#   packaging/sign-and-notarize.sh <bundle> <output.zip>
#
# Required:
#   SIGN_IDENTITY     Common name of the Developer ID Application certificate.
#
# Notarization (all three, or none to sign only):
#   NOTARY_KEY        Path to the App Store Connect .p8 private key.
#   NOTARY_KEY_ID     Key ID, the XXXXXXXXXX in AuthKey_XXXXXXXXXX.p8.
#   NOTARY_ISSUER_ID  Issuer UUID from App Store Connect.
#
# Notes on why this is a script and not inline workflow steps: it has to run
# identically on a developer's machine, the signing order matters, and every
# step needs verifying rather than assuming. Burying that in YAML makes it
# untestable.

set -euo pipefail

# Never trace: the notary key path and identity would end up in CI logs.
set +x

readonly BUNDLE="${1:?usage: sign-and-notarize.sh <bundle> <output.zip>}"
readonly OUTPUT_ZIP="${2:?usage: sign-and-notarize.sh <bundle> <output.zip>}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ENTITLEMENTS="$SCRIPT_DIR/AutoMix.entitlements"

if [[ ! -d "$BUNDLE" ]]; then
    echo "error: no bundle at $BUNDLE" >&2
    exit 1
fi

if [[ -z "${SIGN_IDENTITY:-}" ]]; then
    echo "error: SIGN_IDENTITY is not set" >&2
    exit 1
fi

echo "==> Signing $(basename "$BUNDLE")"

# JUCE sets HARDENED_RUNTIME_ENABLED on the plugin target, but that maps to an
# Xcode build setting and does nothing under the Ninja generator, which is what
# this project builds with. So the hardened runtime is applied here instead.
# Notarization rejects anything without it.
#
# --timestamp is likewise required: an unsigned timestamp fails notarization.
codesign \
    --force \
    --sign "$SIGN_IDENTITY" \
    --entitlements "$ENTITLEMENTS" \
    --options runtime \
    --timestamp \
    --deep \
    "$BUNDLE"

echo "==> Verifying signature"
codesign --verify --deep --strict --verbose=2 "$BUNDLE"

if [[ -z "${NOTARY_KEY:-}" || -z "${NOTARY_KEY_ID:-}" || -z "${NOTARY_ISSUER_ID:-}" ]]; then
    echo "==> Notary credentials not set, signing only"
    echo "    The bundle is signed but not notarized; Gatekeeper will still"
    echo "    block it on other machines."
    ditto -c -k --sequesterRsrc --keepParent "$BUNDLE" "$OUTPUT_ZIP"
    exit 0
fi

# notarytool takes a zip, but stapling has to happen on the bundle itself, so
# this zip is only a transport for the submission and gets rebuilt afterwards.
SUBMIT_DIR="$(mktemp -d)"
readonly SUBMIT_ZIP="$SUBMIT_DIR/submit.zip"
trap 'rm -rf "$SUBMIT_DIR"' EXIT

echo "==> Submitting for notarization (this waits on Apple, usually a minute or two)"
ditto -c -k --sequesterRsrc --keepParent "$BUNDLE" "$SUBMIT_ZIP"

# --wait blocks until Apple returns Accepted or Invalid. Without it the script
# would exit before knowing the result and staple a rejected build.
if ! xcrun notarytool submit "$SUBMIT_ZIP" \
        --key "$NOTARY_KEY" \
        --key-id "$NOTARY_KEY_ID" \
        --issuer "$NOTARY_ISSUER_ID" \
        --wait \
        --timeout 30m; then
    echo "error: notarization failed" >&2
    echo "For the reason, run:" >&2
    echo "  xcrun notarytool log <submission-id> --key ... --key-id ... --issuer ..." >&2
    exit 1
fi

echo "==> Stapling the ticket"
# Stapling attaches the ticket to the bundle so Gatekeeper clears it without a
# network round trip. Skipping this means a machine that is offline, or behind a
# firewall that blocks Apple, still refuses to open it.
xcrun stapler staple "$BUNDLE"
xcrun stapler validate "$BUNDLE"

echo "==> Packaging $(basename "$OUTPUT_ZIP")"
rm -f "$OUTPUT_ZIP"
ditto -c -k --sequesterRsrc --keepParent "$BUNDLE" "$OUTPUT_ZIP"

echo "==> Done: signed, notarized, stapled"
