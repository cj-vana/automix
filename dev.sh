#!/usr/bin/env bash
# Configure, build, and launch the standalone app.
set -euo pipefail

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
open build/AutoMix_artefacts/Release/Standalone/AutoMix.app
