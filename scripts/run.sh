#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Default to release build
BUILD_TYPE="${BUILD_TYPE:-release}"
BUILD_DIR="$ROOT_DIR/build/$BUILD_TYPE"
BINARY="$BUILD_DIR/app/athena-browser"

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo "❌ Binary not found: $BINARY"
    echo ""
    echo "Please build first:"
    echo "  ./scripts/build.sh                # Release build"
    echo "  ./scripts/build.sh --debug        # Debug build"
    exit 1
fi

# Set default URL if not provided
DEFAULT_URL="${DEV_URL:-https://www.google.com}"

echo "🚀 Starting Athena Browser ($BUILD_TYPE)..."
echo "   URL: $DEFAULT_URL"
echo "   Binary: $BINARY"
echo ""

# Run the browser
exec "$BINARY"
