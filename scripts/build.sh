#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Building frontend..."
(cd "$ROOT_DIR/frontend" && npm install && npm run build)

echo "Configuring CMake (Release)..."
cmake --preset release "$ROOT_DIR"

echo "Building native app (Release)..."
cmake --build --preset release -j

echo "Copying frontend bundle to resources/web..."
rsync -a --delete "$ROOT_DIR/frontend/dist/" "$ROOT_DIR/resources/web/"

# Also copy to build output directory for the app to find
BUILD_DIR="$ROOT_DIR/build/release"
mkdir -p "$BUILD_DIR/resources/web"
rsync -a --delete "$ROOT_DIR/frontend/dist/" "$BUILD_DIR/resources/web/"

echo "Done. Binary: $BUILD_DIR/app/athena-browser"

