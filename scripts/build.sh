#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Default values
BUILD_TYPE="release"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --debug              Build in debug mode"
            echo "  --release            Build in release mode (default)"
            echo "  --help               Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                   # Build in release mode"
            echo "  $0 --debug           # Build in debug mode"
            echo ""
            echo "To run after building:"
            echo "  ./scripts/run.sh     # Run with Google as homepage"
            echo "  DEV_URL=https://example.com ./scripts/run.sh"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

BUILD_DIR="$ROOT_DIR/build/$BUILD_TYPE"

echo "Configuring CMake ($BUILD_TYPE)..."
cmake --preset "$BUILD_TYPE" "$ROOT_DIR"

echo "Building athena-browser ($BUILD_TYPE)..."
cmake --build --preset "$BUILD_TYPE" -j

echo ""
echo "✅ Build complete!"
echo ""
echo "Built binary: $BUILD_DIR/app/athena-browser"
echo ""
echo "To run:"
echo "  ./scripts/run.sh                              # Run with default homepage"
echo "  DEV_URL=https://example.com ./scripts/run.sh  # Run with custom URL"
echo ""

