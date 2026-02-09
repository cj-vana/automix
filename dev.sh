#!/usr/bin/env bash
set -euo pipefail
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release 2>/dev/null
cmake --build build --config Release
open build/AutoMix_artefacts/Release/Standalone/AutoMix.app
