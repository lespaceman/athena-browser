#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "🎨 Starting Frontend Development Mode"
echo ""
echo "This script starts:"
echo "  1. Vite dev server (http://localhost:5173)"
echo "  2. Athena Browser pointing to Vite"
echo ""

# Check if browser is built
BUILD_DIR="$ROOT_DIR/build/debug"
BINARY="$BUILD_DIR/app/athena-browser"

if [ ! -f "$BINARY" ]; then
  echo "⚠️  Browser not built yet. Building in debug mode..."
  ./scripts/build.sh --debug
fi

# Start Vite dev server
echo "Starting Vite dev server..."
(
  cd "$ROOT_DIR/homepage"
  npm run dev
) &
VITE_PID=$!

cleanup() {
  echo ""
  echo "Stopping Vite (pid=$VITE_PID)..."
  kill "$VITE_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Wait for Vite to start
echo "Waiting for Vite to start..."
sleep 3

echo ""
echo "🚀 Launching browser with HMR enabled..."
echo "   Vite: http://localhost:5173"
echo ""

# Run browser pointing to Vite dev server
DEV_URL="http://localhost:5173" "$BINARY" || true

