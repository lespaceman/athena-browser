#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Default values
BUILD_TYPE="release"
BUILD_AGENT=true

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
        --skip-agent)
            BUILD_AGENT=false
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --debug              Build in debug mode"
            echo "  --release            Build in release mode (default)"
            echo "  --skip-agent         Skip building agent (Claude integration)"
            echo "  --help               Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                   # Build browser + agent in release mode"
            echo "  $0 --debug           # Build browser + agent in debug mode"
            echo "  $0 --skip-agent      # Build only browser (skip agent)"
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

# ============================================================================
# Build Athena Agent (if requested)
# ============================================================================

if [ "$BUILD_AGENT" = true ]; then
    AGENT_DIR="$ROOT_DIR/agent"

    if [ -d "$AGENT_DIR" ]; then
        echo "======================================"
        echo "Building Athena Agent..."
        echo "======================================"

        cd "$AGENT_DIR"

        # Check if node_modules exists, if not install dependencies
        if [ ! -d "node_modules" ]; then
            echo "Installing agent dependencies..."
            npm install
        fi

        # Build the agent
        echo "Compiling TypeScript..."
        npm run build

        echo "✅ Athena Agent built successfully"
        echo ""

        cd "$ROOT_DIR"
    else
        echo "⚠️  Warning: agent directory not found, skipping agent build"
        echo ""
    fi
fi

# ============================================================================
# Build Browser
# ============================================================================

echo "======================================"
echo "Building Athena Browser ($BUILD_TYPE)..."
echo "======================================"

echo "Configuring CMake ($BUILD_TYPE)..."
cmake --preset "$BUILD_TYPE" "$ROOT_DIR"

echo "Building athena-browser ($BUILD_TYPE)..."
cmake --build --preset "$BUILD_TYPE" -j

# ============================================================================
# Build Summary
# ============================================================================

echo ""
echo "======================================"
echo "✅ Build Complete!"
echo "======================================"
echo ""
echo "Browser binary: $BUILD_DIR/app/athena-browser"

if [ "$BUILD_AGENT" = true ] && [ -d "$ROOT_DIR/agent/dist" ]; then
    echo "Agent script:   $ROOT_DIR/agent/dist/server.js"
fi

echo ""
echo "To run:"
echo "  ./scripts/run.sh                              # Run with default homepage"
echo "  DEV_URL=https://example.com ./scripts/run.sh  # Run with custom URL"
echo ""

