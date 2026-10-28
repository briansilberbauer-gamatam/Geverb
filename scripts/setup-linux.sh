#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ ! -e "$ROOT/external/JUCE/CMakeLists.txt" ]; then
    if [ -d /home/brians/bls/sound/plugin_poc/external/JUCE ]; then
        ln -sfn /home/brians/bls/sound/plugin_poc/external "$ROOT/external"
    else
        echo "JUCE checkout not found. Place JUCE 8.0.8 at $ROOT/external/JUCE" >&2
        exit 1
    fi
fi

if [ ! -d "$ROOT/deps/sysroot/usr" ]; then
    if [ -d /home/brians/bls/sound/plugin_poc/deps/sysroot ]; then
        ln -sfn /home/brians/bls/sound/plugin_poc/deps "$ROOT/deps"
    fi
fi

echo "JUCE:   $(readlink -f "$ROOT/external/JUCE")"
echo "Sysroot: $(readlink -f "$ROOT/deps/sysroot" 2>/dev/null || echo 'system libraries')"
