#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist/linux/athena-browser"
BUILD_DIR="$ROOT_DIR/build/release"

echo "📦 Creating Linux distribution bundle..."

# Clean previous dist
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"/{bin,lib,resources/homepage}

# 1. Copy athena-browser binary
echo "  → Copying binary..."
cp "$BUILD_DIR/app/athena-browser" "$DIST_DIR/bin/"

# 2. Copy CEF libraries and resources
echo "  → Copying CEF libraries and resources..."
CEF_DIR=$(find "$ROOT_DIR/third_party" -maxdepth 1 -type d -name "cef_binary_*" | head -1)
if [ -z "$CEF_DIR" ] || [ ! -d "$CEF_DIR" ]; then
  echo "❌ Error: CEF directory not found in third_party/"
  exit 1
fi

CEF_RELEASE="$CEF_DIR/Release"
CEF_RESOURCES="$CEF_DIR/Resources"

if [ ! -d "$CEF_RELEASE" ]; then
  echo "❌ Error: CEF Release directory not found at $CEF_RELEASE"
  exit 1
fi

# Copy CEF binaries from Release/
cp "$CEF_RELEASE/libcef.so" "$DIST_DIR/lib/"
cp "$CEF_RELEASE/"*.so* "$DIST_DIR/lib/" 2>/dev/null || true
cp "$CEF_RELEASE/"*.bin "$DIST_DIR/lib/" 2>/dev/null || true
cp "$CEF_RELEASE/"*.json "$DIST_DIR/lib/" 2>/dev/null || true

# Copy CEF resources from Resources/
if [ -d "$CEF_RESOURCES" ]; then
  cp "$CEF_RESOURCES/"*.pak "$DIST_DIR/lib/" 2>/dev/null || true
  cp "$CEF_RESOURCES/"*.dat "$DIST_DIR/lib/" 2>/dev/null || true

  # Copy locales directory (required for CEF)
  if [ -d "$CEF_RESOURCES/locales" ]; then
    cp -r "$CEF_RESOURCES/locales" "$DIST_DIR/lib/" 2>/dev/null || true
  fi
fi

# 3. Copy agent with production dependencies only
echo "  → Installing agent with production dependencies..."
mkdir -p "$DIST_DIR/lib/agent"
cp -r "$ROOT_DIR/agent/dist/"* "$DIST_DIR/lib/agent/"
cp "$ROOT_DIR/agent/package"*.json "$DIST_DIR/lib/agent/"
cd "$DIST_DIR/lib/agent"
npm ci --production --silent
cd "$ROOT_DIR"

# 4. Copy homepage resources
echo "  → Copying homepage resources..."
if [ -d "$ROOT_DIR/resources/homepage" ] && [ "$(ls -A "$ROOT_DIR/resources/homepage")" ]; then
  cp -r "$ROOT_DIR/resources/homepage/"* "$DIST_DIR/resources/homepage/"
else
  echo "⚠️  Warning: resources/homepage is empty. Run ./scripts/build-homepage.sh first."
fi

# 5. Bundle Qt libraries
echo "  → Bundling Qt libraries..."
# Find Qt libraries using ldd and copy them
QT_LIBS=$(ldd "$DIST_DIR/bin/athena-browser" | grep -E "libQt|libicu" | awk '{print $3}' | grep -v "^$" || true)
for lib in $QT_LIBS; do
  if [ -f "$lib" ]; then
    cp "$lib" "$DIST_DIR/lib/"
  fi
done

# Copy Qt plugins (platforms plugin is required)
QT_PLUGIN_PATH=$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu/qt6/plugins")
if [ -d "$QT_PLUGIN_PATH/platforms" ]; then
  mkdir -p "$DIST_DIR/lib/plugins/platforms"
  cp "$QT_PLUGIN_PATH/platforms/libqxcb.so" "$DIST_DIR/lib/plugins/platforms/" 2>/dev/null || true
fi

# 6. Create launcher script
cat > "$DIST_DIR/athena-browser.sh" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/lib/plugins:$QT_PLUGIN_PATH"
exec "$SCRIPT_DIR/bin/athena-browser" "$@"
EOF
chmod +x "$DIST_DIR/athena-browser.sh"

# 7. Add runtime documentation
cat > "$DIST_DIR/README.txt" << 'EOF'
Athena Browser - Distribution Bundle

Requirements:
- Node.js 18+ must be installed on the system
- X11 display server (Linux desktop environment)

Running:
  ./athena-browser.sh [URL]

Environment Variables:
- LOG_LEVEL: Set logging verbosity (debug, info, warn, error)
- ATHENA_SOCKET_PATH: Custom Unix socket path for control API

Default socket: /tmp/athena-<UID>-control.sock
EOF

echo "✅ Linux bundle created: $DIST_DIR"
echo ""
echo "Test with: $DIST_DIR/athena-browser.sh"
