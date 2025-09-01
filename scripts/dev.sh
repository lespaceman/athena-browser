#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Starting Vite dev server..."
(
  cd "$ROOT_DIR/frontend"
  npm run dev
) &
VITE_PID=$!

cleanup() {
  echo "Stopping Vite (pid=$VITE_PID)"
  kill "$VITE_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Build the app if needed
BUILD_DIR="$ROOT_DIR/build/debug"
if [ ! -f "$BUILD_DIR/app/athena-browser" ]; then
  echo "Building native app in debug mode..."
  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"
  cmake ../.. -DCMAKE_BUILD_TYPE=Debug
  cmake --build . --parallel
  cd "$ROOT_DIR"
fi

# Wait for Vite to start
echo "Waiting for Vite dev server..."
sleep 3

echo "Launching CEF app pointing at DEV_URL=http://localhost:5173"
DEV_URL="http://localhost:5173" "$BUILD_DIR/app/athena-browser" || true

