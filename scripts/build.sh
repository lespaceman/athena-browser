#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Default values
BUILD_TYPE="release"
SKIP_FRONTEND=false

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
        --skip-frontend)
            SKIP_FRONTEND=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --debug              Build in debug mode (default: release)"
            echo "  --release            Build in release mode (default)"
            echo "  --skip-frontend      Skip frontend build"
            echo "  --help               Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                   # Build in release mode"
            echo "  $0 --debug           # Build in debug mode"
            echo "  $0 --skip-frontend   # Build without rebuilding frontend"
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

# Build frontend if not skipped
if [ "$SKIP_FRONTEND" = false ]; then
    echo "Building frontend..."
    (cd "$ROOT_DIR/frontend" && npm install && npm run build)
    
    echo "Copying frontend bundle to resources/web..."
    rsync -a --delete "$ROOT_DIR/frontend/dist/" "$ROOT_DIR/resources/web/"
    
    # Also copy to build output directory for the app to find
    mkdir -p "$BUILD_DIR/resources/web"
    rsync -a --delete "$ROOT_DIR/frontend/dist/" "$BUILD_DIR/resources/web/"
else
    echo "Skipping frontend build..."
fi

echo "Configuring CMake ($BUILD_TYPE)..."
cmake --preset "$BUILD_TYPE" "$ROOT_DIR"

echo "Building athena-browser ($BUILD_TYPE)..."
cmake --build --preset "$BUILD_TYPE" -j

echo ""
echo "Built binary: $BUILD_DIR/app/athena-browser"
echo ""
echo "To run:"
echo "  GDK_BACKEND=x11 $BUILD_DIR/app/athena-browser"
echo ""
echo "Or use the dev script with HMR:"
echo "  ./scripts/dev.sh"
echo ""
echo "Build complete!"

