# Athena Browser - Phased Reorganization Plan

## Executive Summary

This plan addresses packaging and distribution issues while minimizing disruption to the codebase. The approach is incremental, with each phase independently testable and shippable.

**Key Decisions:**
- ✅ Rename directories for clarity: `frontend/` → `homepage/`, `athena-agent/` → `agent/`, `resources/web/` → `resources/homepage/`
- ✅ Add `dist/` and `releases/` directories for proper packaging
- ✅ Eliminate `resources/homepage/` duplication via automated build
- ✅ Create platform-specific packaging scripts with complete Qt/CEF bundling
- ⚠️ **DEFER** renaming `app/` → `src/app/` (low value, high disruption)
- ⚠️ **DEFER** configurable resource paths until multi-platform support needed (Phase 4)

**Final Structure:**
```
athena-browser/
├── app/                    # C++ browser engine
├── homepage/               # Default homepage webapp (was: frontend)
├── agent/                  # Node.js Claude integration (was: athena-agent)
├── resources/
│   └── homepage/           # Built homepage assets (was: resources/web)
├── build/                  # Build intermediates (gitignored)
├── dist/                   # Distribution bundles (gitignored)
└── releases/               # Final packages (gitignored)
```

**Execution Strategy:**
- ✅ **Do Now (8-12 hours):** Phases 0-3 → Working Linux bundle
- ⏭️ **Defer:** Phase 4 (over-engineered)
- ⚠️ **Do Later:** Phases 5-7 (when multi-platform users exist)
- ❌ **Skip:** Phase 8 (low value, high disruption)

---

## ⚡ Current Status

**Last Updated:** 2025-01-24

### ✅ Completed Phases

**Phase 0: Pre-Flight Checks** (COMPLETED)
- ✅ Audited all hardcoded paths in C++, CMake, Node.js, and scripts
- ✅ Documented current runtime paths
- ✅ Baseline tests passing (210/210 tests)
- ✅ Time: ~1 hour

**Phase 1: Rename Directories for Clarity** (COMPLETED)
- ✅ Renamed `frontend/` → `homepage/`
- ✅ Renamed `athena-agent/` → `agent/`
- ✅ Renamed `resources/web/` → `resources/homepage/`
- ✅ Updated .gitignore
- ✅ Updated all C++ code references
- ✅ Updated all script references (build.sh, dev.sh, build.ps1)
- ✅ Updated CMake files
- ✅ Updated documentation (CLAUDE.md)
- ✅ All tests passing (210/210)
- ✅ Build succeeds
- ✅ Git commit: a2f6e42 "refactor: rename directories for clarity"
- ✅ Time: ~2 hours

**Total Time Spent:** 3 hours

### 🔄 Remaining Phases

- **Phase 2:** Fix Homepage Build Duplication (1 hour estimated)
- **Phase 3:** Add dist/ Structure (4-6 hours estimated)
- **Phases 4-8:** Deferred or skipped

---

## Phase 0: Pre-Flight Checks ⚙️ ✅ COMPLETED

**Goal:** Establish baseline and identify all hardcoded paths

**Tasks:**

1. **Audit Hardcoded Paths**
   ```bash
   # Find all hardcoded paths in C++ code (using CURRENT names)
   find app/src -name "*.cpp" -o -name "*.h" | \
     xargs grep -n "resources/web\|frontend\|athena-agent\|GetExecutableDir"

   # Find all path references in CMake
   grep -rn "app/\|resources/\|frontend\|athena-agent" CMakeLists.txt app/CMakeLists.txt

   # Find all path references in Node.js agent
   grep -rn "athena-agent\|agent/dist\|frontend" athena-agent/src athena-agent/package.json

   # Find all path references in scripts
   grep -rn "frontend\|athena-agent\|resources/web" scripts/
   ```

2. **Document Current Runtime Paths**
   - Development mode: `DEV_URL=http://localhost:5173` (bypasses resources/web)
   - Production mode: `resources/web/` relative to CWD or binary
   - athena-agent: `athena-agent/dist/server.js` relative to project root

3. **Baseline Test**
   ```bash
   # Ensure all tests pass before any changes
   ./scripts/build.sh && ctest --test-dir build/release --output-on-failure

   # Verify dev mode works
   ./scripts/dev.sh

   # Verify production mode works
   cd homepage && npm run build
   cp -r dist/* ../resources/homepage/
   cd ..
   ./build/release/app/athena-browser
   ```

**Exit Criteria:**
- ✅ All tests passing
- ✅ Dev mode working (Vite HMR)
- ✅ Production mode working (built homepage)
- ✅ All hardcoded paths documented

**Time Estimate:** 1-2 hours

---

## Phase 1: Rename Directories for Clarity 📝 ✅ COMPLETED

**Goal:** Rename directories to better reflect their purpose

**Problem:**
- `frontend/` is misleading - it's the browser's default homepage webapp, not frontend UI
- `athena-agent/` is verbose - shorter `agent/` is clearer
- `resources/web/` doesn't indicate it's the homepage

**Solution:** Rename directories and update all references

### Changes

**1.1: Rename Directories**

```bash
# Rename directories
git mv frontend homepage
git mv athena-agent agent
git mv resources/web resources/homepage
```

**1.2: Update .gitignore**

```diff
-/frontend/dist/
-/athena-agent/dist/
-/resources/web/
+/homepage/dist/
+/agent/dist/
+/resources/homepage/
```

**1.3: Update scripts/dev.sh**

```diff
-  cd "$ROOT_DIR/frontend"
+  cd "$ROOT_DIR/homepage"
   npm run dev
```

**1.4: Update CMakeLists.txt Comments**

```diff
-# Optionally: copy homepage production bundle to resources/homepage (handled by scripts/build)
+# Optionally: copy homepage production bundle to resources/homepage (handled by scripts/build)
```

**1.5: Update main.cpp Comments**

```diff
-    // The script is at the project root: /path/to/project/agent/dist/server.js
+    // The script is at the project root: /path/to/project/agent/dist/server.js
```

```diff
-    std::filesystem::path runtime_script = project_root / "agent" / "dist" / "server.js";
+    std::filesystem::path runtime_script = project_root / "agent" / "dist" / "server.js";
```

**1.6: Update scheme_handler.cpp Comments**

```diff
-  // Look for resources in resources/homepage directory (production build output)
-  fs::path resource_path = fs::path("resources/homepage") / path;
+  // Look for resources in resources/homepage directory (production build output)
+  fs::path resource_path = fs::path("resources/homepage") / path;
```

```diff
-      resource_path = exe_dir / "resources/homepage" / path;
+      resource_path = exe_dir / "resources/homepage" / path;
```

**1.7: Update CLAUDE.md**

Replace all references:
- `homepage/` → `homepage/`
- `agent/` → `agent/`
- `resources/homepage/` → `resources/homepage/`

**1.8: Update README.md** (if it exists)

Same replacements as CLAUDE.md

**1.9: Update package.json and tsconfig.json in agent/**

Check if there are any paths that reference the old directory name (unlikely, but verify).

### Testing

```bash
# Ensure git tracked the renames
git status

# Build homepage
cd homepage && npm run build
cd ..

# Build agent
cd agent && npm install && npm run build
cd ..

# Build browser
./scripts/build.sh

# Test dev mode
./scripts/dev.sh

# Test production mode
./build/release/app/athena-browser
```

**Exit Criteria:**
- ✅ All directories renamed via git mv (preserves history)
- ✅ All code references updated
- ✅ All documentation updated
- ✅ Build succeeds
- ✅ Tests pass
- ✅ Dev mode works
- ✅ Production mode works

**Time Estimate:** 2-3 hours (Actual: 2 hours)

**Git Commit:** a2f6e42 "refactor: rename directories for clarity"

---

## Phase 2: Fix Homepage Build Duplication 🎯

**Goal:** Eliminate manual copying between `homepage/dist/` and `resources/homepage/`

**Problem:** Homepage is built to `homepage/dist/`, but scheme handler loads from `resources/homepage/`. These get out of sync.

**Solution:** Automate the copy step in build scripts

### Changes

**2.1: Create Homepage Build Script**

```bash
# scripts/build-homepage.sh
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "🎨 Building Homepage..."

cd "$ROOT_DIR/homepage"

# Ensure dependencies are installed
if [ ! -d "node_modules" ]; then
  echo "📥 Installing homepage dependencies..."
  npm install
fi

npm run build

echo "📦 Copying homepage to resources/homepage/..."
rm -rf "$ROOT_DIR/resources/homepage/"*
cp -r dist/* "$ROOT_DIR/resources/homepage/"

echo "✅ Homepage build complete: resources/homepage/"
```

**2.2: Update Main Build Script**

```bash
# scripts/build.sh (add before CMake build)

# Build homepage if requested
if [ "${BUILD_HOMEPAGE:-1}" = "1" ]; then
  ./scripts/build-homepage.sh
fi
```

**2.3: Update .gitignore**

Already updated in Phase 1.

**2.4: Remove .gitkeep** (if exists)

```bash
rm resources/homepage/.gitkeep 2>/dev/null || true
```

**2.5: Update Documentation**

Update CLAUDE.md to reflect the new workflow:

```markdown
### Homepage Development

Development mode (HMR):
  ./scripts/dev.sh

Production build:
  ./scripts/build-homepage.sh  # Builds and copies to resources/homepage/
  ./scripts/build.sh            # Automatically builds homepage first
```

### Testing

```bash
# Test automated build
./scripts/build-homepage.sh

# Verify files copied
ls -la resources/homepage/

# Test browser loads resources
./build/release/app/athena-browser

# Test that main build script includes homepage
rm -rf resources/homepage/* homepage/dist/*
./scripts/build.sh
ls -la resources/homepage/  # Should have files
```

**Exit Criteria:**
- ✅ Homepage builds automatically during `./scripts/build.sh`
- ✅ `resources/homepage/` populated from `homepage/dist/`
- ✅ Browser loads resources correctly
- ✅ Dev mode still works (unchanged)

**Time Estimate:** 1 hour

**Git Commit:** "build: automate homepage build and copy to resources/homepage"

---

## Phase 3: Add dist/ Structure 📦

**Goal:** Create unified distribution directory for platform bundles

**Problem:** Build artifacts scattered across `build/`, `homepage/dist/`, `agent/dist/`, `resources/homepage/`

**Solution:** Create `dist/` directory with platform-specific bundle structures

### Changes

**3.1: Create dist/ Directory Structure**

```bash
mkdir -p dist/linux/athena-browser/{bin,lib,resources}
mkdir -p dist/macos
mkdir -p dist/windows
```

**3.2: Update .gitignore**

```diff
 /build/
+/dist/
+/releases/
```

**4.3: Create Linux Packaging Script**

```bash
# scripts/package-linux.sh
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
CEF_RESOURCES="$ROOT_DIR/third_party/cef_binary_"*/Release
cp "$CEF_RESOURCES/libcef.so" "$DIST_DIR/lib/"
cp "$CEF_RESOURCES/"*.pak "$DIST_DIR/lib/"
cp "$CEF_RESOURCES/"*.dat "$DIST_DIR/lib/" 2>/dev/null || true
cp "$CEF_RESOURCES/"*.bin "$DIST_DIR/lib/" 2>/dev/null || true
# Copy locales directory (required for CEF)
cp -r "$CEF_RESOURCES/locales" "$DIST_DIR/lib/" 2>/dev/null || true

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
cp -r "$ROOT_DIR/resources/homepage/"* "$DIST_DIR/resources/homepage/"

# 5. Bundle Qt libraries
echo "  → Bundling Qt libraries..."
# Find Qt libraries using ldd and copy them
QT_LIBS=$(ldd "$DIST_DIR/bin/athena-browser" | grep -E "libQt|libicu" | awk '{print $3}' | grep -v "^$")
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
```

**4.4: Update Build Script to Support Packaging**

```bash
# scripts/build.sh (add at end)

if [ "${PACKAGE:-0}" = "1" ]; then
  echo ""
  echo "📦 Creating distribution bundle..."
  ./scripts/package-linux.sh
fi
```

### Testing

```bash
# Build everything
./scripts/build.sh

# Create Linux bundle
./scripts/package-linux.sh

# Verify structure
tree dist/linux/athena-browser -L 2

# Test bundle runs
cd dist/linux/athena-browser
./athena-browser.sh
```

**Exit Criteria:**
- ✅ `dist/linux/athena-browser/` contains complete runnable bundle
- ✅ Binary, CEF libs (including locales), Qt libs, agent, homepage all present
- ✅ Agent uses production dependencies only (no devDependencies)
- ✅ Qt plugins (platforms) bundled correctly
- ✅ Launcher script sets LD_LIBRARY_PATH and QT_PLUGIN_PATH
- ✅ Bundle runs without dependencies on build/ directory
- ✅ Bundle includes README.txt with requirements and usage
- ⚠️ Requires Node.js 18+ on target system

**Time Estimate:** 4-6 hours

**Note:** This creates a semi-portable bundle. Node.js must be installed on the target system. For fully standalone distribution, consider Phase 6 (AppImage) which can bundle Node.js.

**Git Commit:** "build: add Linux distribution packaging"

---

## Phase 4: Make Resource Paths Configurable 🔧 (OPTIONAL - DEFER)

**⚠️ Recommendation: DEFER until multi-platform support is actually needed**

**Goal:** Eliminate hardcoded `resources/homepage` and `agent/dist` paths

**Problem:** Paths are hardcoded, making it hard to support different bundle structures

**Why Defer:**
- Current runtime path detection in `scheme_handler.cpp` and `main.cpp` already works for dev and production modes
- Adds significant complexity (8 hours) for uncertain future benefit
- Can be added later when macOS/Windows support is actively developed
- Phases 1-3 provide immediate value; this adds technical debt

**Solution (if needed later):** Make paths configurable via CMake defines and runtime detection

### Changes

**4.1: Add CMake Configuration Options**

```cmake
# CMakeLists.txt (at top)

# Deployment paths (can be overridden for packaging)
set(ATHENA_RESOURCES_PATH "resources/homepage" CACHE STRING "Path to homepage resources")
set(ATHENA_AGENT_SCRIPT "agent/dist/server.js" CACHE STRING "Path to agent script")

# Generate config header
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/app/src/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/app/src/config.h"
)
```

**4.2: Create config.h.in Template**

```cpp
// app/src/config.h.in
#ifndef ATHENA_CONFIG_H
#define ATHENA_CONFIG_H

// Build-time configuration
#define ATHENA_VERSION "@PROJECT_VERSION@"

// Default resource paths (relative to executable or CWD)
#define ATHENA_DEFAULT_RESOURCES_PATH "@ATHENA_RESOURCES_PATH@"
#define ATHENA_DEFAULT_AGENT_SCRIPT "@ATHENA_AGENT_SCRIPT@"

#endif  // ATHENA_CONFIG_H
```

**4.3: Add Path Resolution Utility**

```cpp
// app/src/utils/paths.h
#ifndef ATHENA_UTILS_PATHS_H
#define ATHENA_UTILS_PATHS_H

#include <filesystem>
#include <string>
#include "utils/result.h"

namespace athena {
namespace utils {

class PathResolver {
 public:
  // Get path to web resources
  // Checks (in order):
  //   1. DEV_URL env var → use Vite dev server (no local files needed)
  //   2. resources/homepage relative to CWD
  //   3. ../resources/homepage relative to executable (for bundles)
  static Result<std::filesystem::path> GetResourcesPath();

  // Get path to agent script
  // Checks (in order):
  //   1. agent/dist/server.js relative to CWD (dev mode)
  //   2. ../agent/dist/server.js relative to executable
  //   3. lib/agent/server.js relative to executable (bundle)
  static Result<std::filesystem::path> GetAgentScriptPath();

  // Get executable directory
  static std::filesystem::path GetExecutableDir();
};

}  // namespace utils
}  // namespace athena

#endif  // ATHENA_UTILS_PATHS_H
```

**4.4: Implement Path Resolution**

```cpp
// app/src/utils/paths.cpp
#include "utils/paths.h"
#include "config.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace athena {
namespace utils {

std::filesystem::path PathResolver::GetExecutableDir() {
  char exe_path[1024];
#ifdef _WIN32
  GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
#else
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len != -1) {
    exe_path[len] = '\0';
  }
#endif
  return std::filesystem::path(exe_path).parent_path();
}

Result<std::filesystem::path> PathResolver::GetResourcesPath() {
  namespace fs = std::filesystem;

  // 1. Check if using dev server
  const char* dev_url = std::getenv("DEV_URL");
  if (dev_url && *dev_url) {
    // Dev server mode - no local resources needed
    return Err("Using dev server, no local resources");
  }

  // 2. Check CWD (development mode)
  fs::path cwd_path = fs::current_path() / ATHENA_DEFAULT_RESOURCES_PATH;
  if (fs::exists(cwd_path / "index.html")) {
    return Ok(cwd_path);
  }

  fs::path exe_dir = GetExecutableDir();

  // 3. Check Linux/Windows bundle (resources/homepage/)
  fs::path bundle_path = exe_dir.parent_path() / "resources/homepage";
  if (fs::exists(bundle_path / "index.html")) {
    return Ok(bundle_path);
  }

  // 4. Check macOS bundle (Resources/homepage/)
  fs::path bundle_macos_path = exe_dir.parent_path() / "Resources/homepage";
  if (fs::exists(bundle_macos_path / "index.html")) {
    return Ok(bundle_macos_path);
  }

  return Err("Resources not found. Checked: " +
             cwd_path.string() + ", " +
             bundle_path.string() + ", " +
             bundle_macos_path.string());
}

Result<std::filesystem::path> PathResolver::GetAgentScriptPath() {
  namespace fs = std::filesystem;

  // 1. Check CWD (development mode)
  fs::path cwd_path = fs::current_path() / ATHENA_DEFAULT_AGENT_SCRIPT;
  if (fs::exists(cwd_path)) {
    return Ok(cwd_path);
  }

  fs::path exe_dir = GetExecutableDir();

  // 2. Check Linux/Windows bundle (lib/agent/)
  fs::path bundle_lib_path = exe_dir.parent_path() / "lib" / "agent" / "server.js";
  if (fs::exists(bundle_lib_path)) {
    return Ok(bundle_lib_path);
  }

  // 3. Check macOS bundle (Resources/agent/)
  fs::path bundle_macos_path = exe_dir.parent_path() / "Resources" / "agent" / "server.js";
  if (fs::exists(bundle_macos_path)) {
    return Ok(bundle_macos_path);
  }

  return Err("Agent script not found. Checked: " +
             cwd_path.string() + ", " +
             bundle_lib_path.string() + ", " +
             bundle_macos_path.string());
}

}  // namespace utils
}  // namespace athena
```

**4.5: Update scheme_handler.cpp**

```cpp
// app/src/resources/scheme_handler.cpp

#include "utils/paths.h"

bool AppSchemeHandler::LoadResource(const std::string& path) {
  // Get resources path using path resolver
  auto resources_path_result = utils::PathResolver::GetResourcesPath();
  if (!resources_path_result) {
    // Dev mode - resources loaded via DEV_URL
    return false;
  }

  fs::path resource_path = resources_path_result.Value() / path;

  if (!fs::exists(resource_path)) {
    return false;
  }

  // ... rest unchanged
}
```

**4.6: Update main.cpp**

```cpp
// app/src/main.cpp

#include "utils/paths.h"

// Replace hardcoded path with:
auto runtime_script_result = utils::PathResolver::GetAgentScriptPath();
if (!runtime_script_result) {
  logger.Warn("Athena Agent script not found: {}",
              runtime_script_result.GetError().Message());
  // ... rest unchanged
} else {
  runtime_config.runtime_script_path = runtime_script_result.Value().string();
  // ... rest unchanged
}
```

**4.7: Add to CMake**

```cmake
# app/CMakeLists.txt

# Add new files
add_executable(athena-browser
  # ... existing files ...
  src/utils/paths.cpp
)

# Include generated config header
target_include_directories(athena-browser PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)
```

**4.8: Write Tests**

```cpp
// app/tests/utils/paths_test.cpp

#include <gtest/gtest.h>
#include "utils/paths.h"

TEST(PathResolverTest, GetExecutableDirReturnsValidPath) {
  auto exe_dir = utils::PathResolver::GetExecutableDir();
  EXPECT_FALSE(exe_dir.empty());
  EXPECT_TRUE(std::filesystem::exists(exe_dir));
}

TEST(PathResolverTest, GetResourcesPathFindsDevelopmentResources) {
  // This test assumes running from project root
  auto result = utils::PathResolver::GetResourcesPath();
  if (std::getenv("DEV_URL")) {
    EXPECT_FALSE(result.IsOk());  // Should fail in dev mode
  } else {
    EXPECT_TRUE(result.IsOk()) << result.GetError().Message();
  }
}

// Add more tests for different scenarios
```

### Testing

```bash
# Build with new path resolution
./scripts/build.sh

# Run tests
./build/release/app/tests/paths_test

# Test dev mode (should work unchanged)
DEV_URL=http://localhost:5173 ./build/release/app/athena-browser

# Test production mode from CWD
./build/release/app/athena-browser

# Test bundle mode
cd dist/linux/athena-browser
./athena-browser.sh
```

**Exit Criteria (if implemented):**
- ✅ All hardcoded paths replaced with PathResolver
- ✅ Dev mode works (finds agent/dist, uses Vite for homepage)
- ✅ Production from CWD works (finds resources/homepage, agent/dist)
- ✅ Bundle mode works (finds ../resources/homepage, lib/agent)
- ✅ Tests pass
- ✅ Clear error messages when paths not found

**Time Estimate:** 6-8 hours

**Simpler Alternative (if needed):**

Instead of the full PathResolver implementation above, consider this simpler 1-hour approach:

```cpp
// Just add fallback logic to existing code
fs::path GetResourcePath() {
  // Try dev location first
  if (fs::exists("resources/homepage/index.html"))
    return "resources/homepage";

  // Try bundle location (relative to exe)
  fs::path exe_dir = GetExecutableDir();
  if (fs::exists(exe_dir.parent_path() / "resources/homepage/index.html"))
    return exe_dir.parent_path() / "resources/homepage";

  throw std::runtime_error("Resources not found");
}
```

This provides 90% of the benefit with 10% of the complexity.

**Git Commit (if implemented):** "refactor: make resource paths configurable"

---

## Phase 5: Add macOS and Windows Packaging 🍎🪟

**Goal:** Support multi-platform packaging

### Changes

**5.1: macOS Packaging Script**

```bash
# scripts/package-macos.sh
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist/macos"
APP_BUNDLE="$DIST_DIR/Athena Browser.app"

echo "📦 Creating macOS app bundle..."

# Create app bundle structure
mkdir -p "$APP_BUNDLE/Contents/"{MacOS,Frameworks,Resources/homepage,Resources/agent}

# Copy binary
cp "$ROOT_DIR/build/release/app/athena-browser" \
   "$APP_BUNDLE/Contents/MacOS/"

# Copy CEF framework
cp -R "$ROOT_DIR/third_party/cef_binary_"*/Release/\
"Chromium Embedded Framework.framework" \
      "$APP_BUNDLE/Contents/Frameworks/"

# Copy agent (Node.js app goes in Resources/)
cp -r "$ROOT_DIR/agent/dist/"* \
      "$APP_BUNDLE/Contents/Resources/agent/"
cp -r "$ROOT_DIR/agent/node_modules" \
      "$APP_BUNDLE/Contents/Resources/agent/"
cp "$ROOT_DIR/agent/package.json" \
      "$APP_BUNDLE/Contents/Resources/agent/"

# Copy homepage
cp -r "$ROOT_DIR/resources/homepage/"* \
      "$APP_BUNDLE/Contents/Resources/homepage/"

# Create Info.plist
cat > "$APP_BUNDLE/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key>
  <string>athena-browser</string>
  <key>CFBundleName</key>
  <string>Athena Browser</string>
  <key>CFBundleIdentifier</key>
  <string>com.athena.browser</string>
  <key>CFBundleVersion</key>
  <string>0.1.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
EOF

# Run macdeployqt to bundle Qt libraries
macdeployqt "$APP_BUNDLE"

echo "✅ macOS bundle created: $APP_BUNDLE"
```

**5.2: Windows Packaging Script**

```bash
# scripts/package-windows.sh
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist/windows/athena-browser"

echo "📦 Creating Windows distribution bundle..."

mkdir -p "$DIST_DIR"/{lib/agent,resources/homepage}

# Copy binary
cp "$ROOT_DIR/build/release/app/athena-browser.exe" "$DIST_DIR/"

# Copy CEF DLLs
cp "$ROOT_DIR/third_party/cef_binary_"*/Release/*.dll "$DIST_DIR/"
cp "$ROOT_DIR/third_party/cef_binary_"*/Release/*.pak "$DIST_DIR/"
cp "$ROOT_DIR/third_party/cef_binary_"*/Release/*.dat "$DIST_DIR/" 2>/dev/null || true

# Copy agent (Node.js app goes in lib/)
cp -r "$ROOT_DIR/agent/dist/"* "$DIST_DIR/lib/agent/"
cp -r "$ROOT_DIR/agent/node_modules" "$DIST_DIR/lib/agent/"
cp "$ROOT_DIR/agent/package.json" "$DIST_DIR/lib/agent/"

# Copy homepage
cp -r "$ROOT_DIR/resources/homepage/"* "$DIST_DIR/resources/homepage/"

# Run windeployqt to bundle Qt DLLs
windeployqt "$DIST_DIR/athena-browser.exe"

echo "✅ Windows bundle created: $DIST_DIR"
```

**Exit Criteria:**
- ✅ macOS .app bundle structure correct
- ✅ Windows directory structure correct
- ✅ Both platforms tested (if available)

**Time Estimate:** 4-6 hours

**Git Commit:** "build: add macOS and Windows packaging"

---

## Phase 6: Create Release Packages 📦

**Goal:** Create distributable packages (AppImage, DMG, installer)

### Changes

**6.1: AppImage Creation (Linux)**

```bash
# scripts/create-appimage.sh
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION=$(git describe --tags --always)

# Ensure bundle exists
if [ ! -d "$ROOT_DIR/dist/linux/athena-browser" ]; then
  ./scripts/package-linux.sh
fi

# Download linuxdeployqt if needed
if [ ! -f "$ROOT_DIR/tools/linuxdeployqt" ]; then
  wget -O "$ROOT_DIR/tools/linuxdeployqt" \
    https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
  chmod +x "$ROOT_DIR/tools/linuxdeployqt"
fi

# Create AppImage
cd "$ROOT_DIR/dist/linux"
"$ROOT_DIR/tools/linuxdeployqt" \
  athena-browser/bin/athena-browser \
  -bundle-non-qt-libs \
  -appimage

# Move to releases/
mkdir -p "$ROOT_DIR/releases"
mv Athena_Browser*.AppImage "$ROOT_DIR/releases/athena-browser-$VERSION-linux-x86_64.AppImage"

echo "✅ AppImage created: releases/athena-browser-$VERSION-linux-x86_64.AppImage"
```

**6.2: DMG Creation (macOS)**

```bash
# scripts/create-dmg.sh
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION=$(git describe --tags --always)

# Ensure bundle exists
if [ ! -d "$ROOT_DIR/dist/macos/Athena Browser.app" ]; then
  ./scripts/package-macos.sh
fi

# Create DMG using create-dmg tool
mkdir -p "$ROOT_DIR/releases"
create-dmg \
  --volname "Athena Browser" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --app-drop-link 600 185 \
  "$ROOT_DIR/releases/athena-browser-$VERSION-macos-universal.dmg" \
  "$ROOT_DIR/dist/macos/Athena Browser.app"

echo "✅ DMG created: releases/athena-browser-$VERSION-macos-universal.dmg"
```

**6.3: Update .gitignore**

```gitignore
/releases/
```

**Exit Criteria:**
- ✅ AppImage builds and runs
- ✅ DMG builds (on macOS)
- ✅ Windows installer builds (on Windows)
- ✅ Versioned filenames include git tag

**Time Estimate:** 4-6 hours

**Git Commit:** "build: add release packaging (AppImage, DMG)"

---

## Phase 7: CI/CD Integration 🤖

**Goal:** Automate building and packaging in CI

### Changes

**7.1: GitHub Actions Workflow**

```yaml
# .github/workflows/build.yml
name: Build and Package

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]
  release:
    types: [ created ]

jobs:
  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y qt6-base-dev libgl1-mesa-dev

      - name: Build
        run: ./scripts/build.sh

      - name: Run tests
        run: ctest --test-dir build/release --output-on-failure

      - name: Package
        run: ./scripts/package-linux.sh

      - name: Create AppImage
        if: github.event_name == 'release'
        run: ./scripts/create-appimage.sh

      - name: Upload AppImage
        if: github.event_name == 'release'
        uses: actions/upload-artifact@v3
        with:
          name: athena-browser-linux
          path: releases/*.AppImage

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: brew install qt@6

      - name: Build
        run: ./scripts/build.sh

      - name: Run tests
        run: ctest --test-dir build/release --output-on-failure

      - name: Package
        run: ./scripts/package-macos.sh

      - name: Create DMG
        if: github.event_name == 'release'
        run: ./scripts/create-dmg.sh

      - name: Upload DMG
        if: github.event_name == 'release'
        uses: actions/upload-artifact@v3
        with:
          name: athena-browser-macos
          path: releases/*.dmg
```

**Exit Criteria:**
- ✅ CI builds on all platforms
- ✅ Tests run in CI
- ✅ Releases automatically create packages
- ✅ Artifacts uploaded

**Time Estimate:** 3-4 hours

**Git Commit:** "ci: add build and packaging workflow"

---

## Phase 8 (OPTIONAL): Source Reorganization 📁

**Goal:** Rename `app/` → `src/app/` (if still desired)

**⚠️ WARNING:** This phase is HIGH EFFORT, LOW VALUE. Only proceed if there's a strong reason.

### Changes

**8.1: Move Source Directories**

```bash
mkdir src
mv app src/
mv homepage src/
mv agent src/
```

**8.2: Update CMakeLists.txt**

```diff
-add_subdirectory(app)
+add_subdirectory(src/app)
```

**8.3: Update All Path References**

- Update all scripts in `scripts/`
- Update all `#include` paths if needed (check if they're relative or absolute)
- Update IDE configurations in `.vscode/`
- Update CLAUDE.md documentation

**8.4: Update Packaging Scripts**

All packaging scripts reference old paths - need updates.

**Exit Criteria:**
- ✅ All builds work
- ✅ All tests pass
- ✅ All scripts work
- ✅ Documentation updated

**Time Estimate:** 8-12 hours (high risk of breakage)

**Git Commit:** "refactor: reorganize source into src/ directory"

---

## Summary Timeline

### Recommended Execution Path

| Phase | Goal | Priority | Time | Cumulative |
|-------|------|----------|------|------------|
| 0 | Pre-flight checks | 🔥 High | 1-2h | 1-2h |
| 1 | Rename directories | 🔥 High | 2-3h | 3-5h |
| 2 | Fix homepage duplication | 🔥 High | 1h | 4-6h |
| 3 | Add dist/ structure (Linux) | 🔥 High | 4-6h | 8-12h |
| **SHIP v1.0 HERE** | **Working Linux bundle** | | | |
| 4 | Configurable paths | ⏭️ Defer | 6-8h | (defer) |
| 5 | macOS/Windows packaging | ⚠️ Medium | 4-6h | 12-18h |
| 6 | Release packages (AppImage) | ⚠️ Medium | 4-6h | 16-24h |
| 7 | CI/CD | ⚠️ Medium | 3-4h | 19-28h |
| 8 | src/ rename | ❌ Skip | 8-12h | (skip) |

**Immediate Work (Phases 0-3): 8-12 hours → Functional Linux bundle**
**Full Multi-Platform (Phases 0-3, 5-7): 19-28 hours**
**Total with all optional phases: 33-48 hours**

### Priority Levels
- 🔥 **High**: Do now for immediate value
- ⚠️ **Medium**: Do when needed (multi-platform users)
- ⏭️ **Defer**: Only if actually needed
- ❌ **Skip**: Not worth the effort

---

## Testing Strategy

After each phase:

1. **Unit Tests:** `ctest --test-dir build/release --output-on-failure`
2. **Dev Mode:** `./scripts/dev.sh` - verify Vite HMR works
3. **Production Build:** `./scripts/build.sh && ./build/release/app/athena-browser`
4. **Bundle Test:** Run from `dist/` directory to verify isolation

---

## Rollback Plan

Each phase is a separate git commit. If a phase fails:

```bash
# Rollback last commit
git reset --hard HEAD~1

# Or rollback to specific phase
git reset --hard <commit-hash>
```

---

## Key Decisions & Open Questions

### Resolved ✅

1. **Qt Library Bundling:**
   - ✅ Phase 3 uses manual copy via `ldd` for simplicity and reliability
   - Alternative: `linuxdeployqt` can be used in Phase 6 (AppImage)

2. **Node.js Runtime:**
   - ✅ Phase 3 requires Node.js 18+ on target system (documented in README.txt)
   - ✅ Phase 6 (AppImage) can bundle Node.js for fully standalone distribution
   - Alternative: Use `pkg` or `nexe` to create standalone Node.js executables

3. **Version Management:**
   - ✅ Use git tags for versioning: `git describe --tags --always`
   - Used in Phase 6 for release filenames

### Still Open ❓

4. **Code Signing:**
   - macOS: Required for distribution outside App Store (use `codesign`)
   - Windows: Recommended for avoiding SmartScreen warnings (use `signtool`)
   - Decision: Defer until actually distributing to end users

5. **Auto-updates:**
   - Options: Electron's `autoUpdater`, custom implementation, or manual
   - Decision: Not needed for v1.0, defer to future version

6. **Environment Variables in Packaged Builds:**
   - Should packaged builds support `DEV_URL`, `LOG_LEVEL`, etc.?
   - Current approach: Yes, all env vars work in bundles
   - Socket paths use `/tmp/athena-<UID>-control.sock` by default

---

## Success Metrics

### Phase 0-3 (Immediate Goals)
- ✅ No manual steps required to create Linux bundle
- ✅ Single command creates bundle: `./scripts/package-linux.sh`
- ✅ Bundle is semi-portable (requires Node.js 18+ on target system)
- ✅ Dev workflow unchanged (Vite HMR still works)
- ✅ All tests pass
- ✅ Clear documentation of requirements in README.txt

### Phase 5-7 (Multi-Platform Goals)
- ✅ macOS .app bundle and DMG
- ✅ Windows installer
- ✅ AppImage for fully standalone Linux distribution
- ✅ CI produces release artifacts automatically
- ✅ Versioned releases via git tags

---

## Quick Start Recommendation

**TL;DR: Execute Phases 0-3, skip the rest for now.**

### Why This Order?

1. **Phase 0-1-2-3 (8-12 hours):**
   - Fixes immediate pain points (directory naming, build duplication)
   - Creates working Linux distribution bundle
   - Low risk, high value
   - Can ship v1.0 after Phase 3

2. **Phase 4 (DEFER):**
   - Over-engineered for current needs
   - Current path detection already works
   - Add only if you need macOS/Windows support

3. **Phase 5-6-7 (Do when needed):**
   - Only implement when you have users on other platforms
   - Each phase is independent and can be done later

4. **Phase 8 (SKIP):**
   - Low value, high disruption
   - No compelling reason to rename `app/` → `src/app/`

### Next Steps

1. Start with `Phase 0` - run the audit commands to establish baseline
2. Proceed through Phases 1-3 sequentially
3. After Phase 3, you'll have a working Linux bundle ready to ship
4. Return to this plan when you need multi-platform support
