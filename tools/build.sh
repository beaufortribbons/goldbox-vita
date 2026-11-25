#!/usr/bin/env bash
# build.sh <game>   — builds build/<game>.vpk using VitaSDK
set -euo pipefail

GAME=${1:-gateway}
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/$GAME"

if [ -z "${VITASDK:-}" ]; then
  echo "ERROR: VITASDK is not set. Source your shell profile first." >&2
  exit 1
fi

export PATH="$VITASDK/bin:$PATH"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$REPO_ROOT" \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DGAME="$GAME" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) "$GAME.vpk-vpk"

echo ""
echo "Built: $BUILD_DIR/$GAME.vpk"
