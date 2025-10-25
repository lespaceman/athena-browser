#!/bin/bash
# Test platform flag presets
# Usage: ./scripts/test-platform-flags.sh [preset]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BROWSER="${PROJECT_ROOT}/build/release/app/athena-browser"

if [ ! -f "$BROWSER" ]; then
  echo "❌ Browser not built. Run ./scripts/build.sh first"
  exit 1
fi

PRESET="${1:-}"

echo "========================================"
echo "Platform Flag Preset Testing"
echo "========================================"
echo ""

# Function to show preset info
show_preset() {
  local preset_name="$1"
  echo "Testing preset: $preset_name"
  echo ""
  echo "Command:"
  echo "  ATHENA_FLAG_PRESET=$preset_name $BROWSER"
  echo ""
  echo "Description:"

  case "$preset_name" in
    debug|DEBUG)
      echo "  - Verbose logging (--v=1)"
      echo "  - GPU validation layers"
      echo "  - In-process GPU (easier debugging)"
      echo "  - Synchronous rendering"
      echo "  Use for: Development, debugging, issue investigation"
      ;;
    release|RELEASE)
      echo "  - Minimal logging (warnings only)"
      echo "  - Optimized GPU backend (ANGLE D3D11/Metal/GL-EGL)"
      echo "  - Separate GPU process (stability)"
      echo "  - Hardware acceleration enabled"
      echo "  Use for: Production builds, end users"
      ;;
    performance|PERFORMANCE)
      echo "  - No logging (--disable-logging)"
      echo "  - Zero-copy rasterizer (--enable-zero-copy)"
      echo "  - In-process GPU (reduced IPC overhead)"
      echo "  - Maximum hardware acceleration"
      echo "  Use for: Benchmarking, resource-constrained systems"
      ;;
    compatibility|COMPATIBILITY)
      echo "  - Verbose logging for diagnostics"
      echo "  - Software rendering fallback"
      echo "  - Older/safer GPU backends"
      echo "  - Conservative optimizations"
      echo "  Use for: Troubleshooting GPU/rendering issues"
      ;;
    *)
      echo "  Unknown preset!"
      ;;
  esac
  echo ""
  echo "Platform-specific flags (Linux):"
  echo "  - --use-angle=gl-egl (required for OSR)"
  echo "  - --ozone-platform=x11 (Wayland not fully supported)"
  echo "  - --enable-features=VaapiVideoDecoder (hardware video decode)"
  echo ""
}

if [ -n "$PRESET" ]; then
  # Test specific preset
  show_preset "$PRESET"
  echo "Press Ctrl+C to exit..."
  echo ""
  ATHENA_FLAG_PRESET="$PRESET" "$BROWSER"
else
  # Show all presets
  echo "Available presets:"
  echo ""
  echo "1. DEBUG       - Verbose logging, GPU validation"
  echo "2. RELEASE     - Production optimized (default)"
  echo "3. PERFORMANCE - Maximum speed, minimal logging"
  echo "4. COMPATIBILITY - Software rendering, maximum compatibility"
  echo ""
  echo "Usage:"
  echo "  $0 [preset]"
  echo ""
  echo "Examples:"
  echo "  $0 debug         # Test DEBUG preset"
  echo "  $0 performance   # Test PERFORMANCE preset"
  echo ""
  echo "Or use environment variable directly:"
  echo "  ATHENA_FLAG_PRESET=debug ./build/release/app/athena-browser"
  echo ""
fi
