# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Athena Browser is a CEF-based desktop browser with Qt6 integration and React homepage. The project uses C++17 for the native application and TypeScript/React for the UI layer.

## Architecture

The codebase follows a clean, layered architecture with 5 main layers:

```
main.cpp
    ↓
core/application.cpp - Multi-window lifecycle management
    ↓
core/browser_window.cpp - High-level browser window wrapper
    ↓               ↓
platform/qt_mainwindow.cpp   browser/cef_engine.cpp
    ↓                             ↓
rendering/gl_renderer.cpp - OpenGL hardware-accelerated rendering
```

**Layer Responsibilities:**
- **Foundation:** Core types (Point, Size, Rect, ScaleFactor), error handling (Result<T>), logging
- **Rendering:** Buffer management, scaling, OpenGL rendering (uses CEF's official osr_renderer.cc)
- **Browser Engine:** CEF abstraction, browser lifecycle, navigation
- **Platform:** Qt6 window system abstraction, event handling
- **Application:** High-level API for creating windows and managing application lifecycle

**Key Design Principles:**
- Zero global state throughout the codebase
- RAII resource management everywhere
- Result<T> for error handling (Rust-inspired)
- Full dependency injection
- All files <800 lines (most <300)

## Build Commands

### Building the Browser

```bash
# Release build (default)
./scripts/build.sh

# Debug build
./scripts/build.sh --debug

# Use CMake presets directly
cmake --preset release
cmake --build --preset release
```

Build output: `build/release/app/athena-browser` or `build/debug/app/athena-browser`

### Running the Browser

```bash
# Run with default homepage (Google)
./scripts/run.sh

# Run with custom URL
DEV_URL=https://example.com ./scripts/run.sh

# Direct execution
build/release/app/athena-browser
```

### Development Mode (Homepage)

```bash
# Start Vite dev server + browser with HMR
./scripts/dev.sh
```

This script:
1. Builds the browser in debug mode if needed
2. Starts Vite dev server on http://localhost:5173
3. Launches browser pointing to Vite

### Testing

```bash
# Build and run all tests
./scripts/build.sh && ctest --test-dir build/release --output-on-failure

# Run specific test suite
./build/release/app/tests/buffer_manager_test
./build/release/app/tests/browser_window_test
./build/release/app/tests/core_types_test

# Run tests with pattern matching
ctest --test-dir build/release -R "buffer|window" --output-on-failure

# Run tests with verbose output
ctest --test-dir build/release --verbose
```

**Current Test Count:** Active tests (Qt migration in progress)
- Core types: 42 tests
- Error handling: 24 tests
- Logging: 8 tests
- Buffer management: 30 tests
- Scaling management: 51 tests
- CEF engine: 35 tests
- CEF client: 16 tests
- JS execution utils: Tests available
- Platform/Application tests: Being migrated to Qt

### Code Formatting

```bash
# Format all C++ files
./scripts/format.sh

# Requires clang-format
sudo apt-get install clang-format
```

Uses Google C++ Style Guide with modifications (see .clang-format).

## Development Guidelines

### Writing Tests

When fixing bugs or adding features:

1. **Write a failing test first** (from user's CLAUDE.md)
2. Implement the feature/fix
3. Ensure all tests pass
4. Test coverage should remain >85% for all components

Test structure follows Google Test framework:

```cpp
#include <gtest/gtest.h>
#include "module/class.h"

class MyClassTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup test fixtures
  }
};

TEST_F(MyClassTest, DescriptiveTestName) {
  // Arrange
  MyClass obj(params);

  // Act
  auto result = obj.DoSomething();

  // Assert
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(result.Value(), expected_value);
}
```

For Result<T> functions, always check IsOk() before accessing Value().

### Error Handling Pattern

All fallible operations return `utils::Result<T>`:

```cpp
// Success case
auto result = DoSomething();
if (result.IsOk()) {
  auto value = result.Value();
  // use value
}

// Error case
if (result.IsError()) {
  std::cerr << result.GetError().Message() << std::endl;
}

// Helper functions
return utils::Ok();                    // For Result<void>
return utils::Ok(42);                  // For Result<int>
return utils::Err("error message");    // For errors
```

### Logging Pattern

Athena uses a unified logging approach across C++ and Node.js with consistent behavior and configuration.

**Architecture:**
- **C++**: `utils::Logger` class with RAII and thread safety (app/src/utils/logging.{h,cpp})
- **Node.js**: Winston-based structured logging to stderr (agent/src/logger.ts)
- **Control**: `LOG_LEVEL` environment variable works for both C++ and Node.js
- **Output**: Both write to stderr (not stdout) to avoid conflicts with IPC

**Using Logger in C++:**

```cpp
#include "utils/logging.h"

class MyClass {
 public:
  MyClass() : logger_("MyClass") {
    logger_.Info("MyClass initialized");
  }

  void DoWork() {
    logger_.Debug("Starting work");

    // Template-based formatting with {}
    logger_.Info("Processing {} items", count);
    logger_.Warn("Resource usage at {}%", usage);

    if (error) {
      logger_.Error("Operation failed: {}", error.Message());
    }
  }

 private:
  utils::Logger logger_;
};
```

**Using Logger in Node.js:**

```typescript
import { Logger } from './logger';

class MyService {
  private logger: Logger;

  constructor() {
    this.logger = new Logger('MyService');
    this.logger.info('MyService initialized');
  }

  async doWork() {
    this.logger.debug('Starting work');

    // Structured logging with metadata
    this.logger.info('Processing items', { count: items.length });
    this.logger.warn('Resource usage high', { usage: 85 });

    if (error) {
      this.logger.error('Operation failed', { error: error.message });
    }
  }
}
```

**Log Levels:**

Both C++ and Node.js support the same log levels:
- `debug` - Detailed debugging information (verbose)
- `info` - General informational messages (default)
- `warn` - Warning messages for potentially harmful situations
- `error` - Error messages for failures
- `fatal` - Critical errors (C++ only)

**Controlling Log Output:**

```bash
# Default: info level (shows info, warn, error)
./scripts/run.sh

# Debug mode: all logs including debug messages
LOG_LEVEL=debug ./scripts/run.sh

# Quiet mode: only warnings and errors
LOG_LEVEL=warn ./scripts/run.sh

# View logs in real-time with pretty formatting
journalctl --user -u athena-browser.service -f | ./scripts/view-logs.sh

# View recent logs
journalctl --user -u athena-browser.service -n 100 | ./scripts/view-logs.sh
```

**Testing Logging:**

```bash
# Demo script showing all log modules
./scripts/test-all-logs.sh

# View logs with jq pretty-printing
cat /tmp/athena.log | ./scripts/view-logs.sh
```

**Log Module Naming:**

Use descriptive module names that identify the component:

C++ modules:
- Application, BrowserWindow, CEFEngine, GLRenderer, NodeRuntime

Node.js modules:
- Server, NativeController, BrowserApiClient, SessionManager, ClaudeClient, MCPServer

**Important Notes:**

- **Never use std::cout/cerr** directly in application code - use Logger
- **Never use console.log/error** in Node.js - use Logger
- **Exception**: stdout/stderr OK in signal handlers and after fork() (not async-signal-safe to use Logger)
- **Exception**: `console.log("READY ...")` in server.ts startup is intentional for parent process detection
- Logger instances are passed via dependency injection (no global state)
- All logs written to stderr to avoid stdout IPC conflicts
- Both C++ and Node.js produce compatible log formats

### CEF Integration Notes

**Browser Creation Lifecycle:**
- Browser creation happens asynchronously after Qt widget initialization
- GLRenderer must be initialized before creating browser
- Browser creation uses Qt's event system and signals/slots

**CEF Subprocess Handling:**
```cpp
// main.cpp must handle subprocess execution
CefMainArgs main_args(argc, argv);
int exit_code = CefExecuteProcess(main_args, app, nullptr);
if (exit_code >= 0) {
  return exit_code;  // Subprocess completed
}
// Continue with main process
```

**Event Handling:**
- Qt uses signals/slots for event handling
- No global state for CEF clients or renderers
- Clean separation between Qt UI layer and CEF rendering

**Thread Safety (CEF↔Qt):**

CEF runs callbacks on the CEF UI thread, but Qt widgets must only be accessed from the Qt main thread. Athena uses a thread-safe marshaling pattern for all CEF→Qt callbacks:

```cpp
#include "browser/thread_safety.h"

// Safe pattern: marshal from CEF thread → Qt main thread
tab.cef_client->SetTitleChangeCallback([this, browser_id](const std::string& title) {
  SafeInvokeQtCallback(
      this,  // QObject* to validate
      [browser_id, title](QtMainWindow* window) {
        // This runs on Qt main thread with validated window pointer
        window->UpdateTabTitle(browser_id, title);
      });
});
```

The `SafeInvokeQtCallback` helper provides:
- **Weak pointer validation** using `QPointer<T>` (silently drops callback if object destroyed)
- **Thread marshaling** via `QMetaObject::invokeMethod` with `Qt::QueuedConnection`
- **Crash prevention** for widget destruction during async callbacks

**IMPORTANT:** Always use `SafeInvokeQtCallback` for CEF→Qt communication. Never call Qt widget methods directly from CEF callbacks.

See `app/src/browser/thread_safety.h` for implementation details and `docs/KNOWN_ISSUES.md#3` for full documentation.

**Platform Flag Presets:**

Athena uses a centralized platform flags system (`app/src/browser/platform_flags.{h,cpp}`) that applies battle-tested CEF command-line flags based on:
- Platform (Linux, Windows, macOS)
- Build type (Debug, Release)
- Use case (Performance, Compatibility)

Available presets:
- `DEBUG` - Verbose logging, GPU validation, easier debugging
- `RELEASE` - Optimized flags, minimal logging (default for production)
- `PERFORMANCE` - Maximum performance, zero-copy, minimal overhead
- `COMPATIBILITY` - Software rendering fallback for GPU issues

Control via environment variable:

```bash
# Default: RELEASE preset in release builds, DEBUG in debug builds
./scripts/build.sh && ./build/release/app/athena-browser

# Force debug flags (verbose CEF logging)
ATHENA_FLAG_PRESET=debug ./build/release/app/athena-browser

# Performance mode (benchmarking)
ATHENA_FLAG_PRESET=performance ./build/release/app/athena-browser

# Compatibility mode (troubleshooting GPU issues)
ATHENA_FLAG_PRESET=compatibility ./build/release/app/athena-browser
```

Platform-specific flags applied automatically:
- **Linux**: `--use-angle=gl-egl`, `--ozone-platform=x11` (required for OSR)
- **Windows**: `--use-angle=d3d11`, DPI awareness
- **macOS**: `--use-angle=metal`, Retina display support

See `docs/KNOWN_ISSUES.md` for detailed explanations of each flag and known CEF issues.

### OpenGL Rendering

The browser uses CEF's official OpenGL renderer (`osr_renderer.cc`):

```cpp
class GLRenderer {
  // Wraps CEF's OsrRenderer
  Result<void> Initialize(void* widget);
  void OnPaint(CefRefPtr<CefBrowser>, ...);  // Called by CEF
  Result<void> Render();                     // Called by Qt
};
```

Hardware-accelerated, capable of 60+ FPS.

**Performance Optimizations:**

1. **Dirty Rect Optimization** (Implemented)
   - CEF's OsrRenderer automatically uses dirty rectangles for texture updates
   - Only changed regions are uploaded to GPU using `glTexSubImage2D`
   - Full texture updates only when necessary (size changes, full-screen updates)
   - Provides ~2x FPS improvement for incremental page updates
   - Debug logging available: `LOG_LEVEL=debug` shows partial vs full updates

2. **Per-Tab Cookie/Cache Isolation** (Implemented)
   - Optional per-tab `CefRequestContext` for isolated cookie/cache sessions
   - Enabled via `BrowserConfig::isolate_cookies = true`
   - Shares disk storage to avoid duplicate cache files
   - Use cases: Testing, privacy, multi-account workflows

```cpp
// Example: Create browser with isolated cookies/cache
BrowserConfig config;
config.url = "https://example.com";
config.isolate_cookies = true;  // Enable per-tab isolation
auto result = engine->CreateBrowser(config);
```

### Adding New Files

When adding new source files:

1. Add to `app/CMakeLists.txt` in the appropriate section (by phase)
2. Add corresponding test to `app/tests/CMakeLists.txt` using `add_athena_test()` helper
3. Follow namespace structure: `athena::<layer>::<component>`
4. Include header guards and proper #include ordering (see .clang-format)

### Platform-Specific Code

Qt-specific code is isolated in `app/src/platform/qt_*.cpp` files. When modifying:

- Use Qt signals/slots for event handling
- Follow Qt's object ownership model (parent-child relationships)
- Never use global state
- Use Qt's MOC (Meta-Object Compiler) for signals/slots

## Common Tasks

### Adding a New Test Suite

```bash
# 1. Create test file in app/tests/<category>/
touch app/tests/core/my_feature_test.cpp

# 2. Add to app/tests/CMakeLists.txt
add_athena_test(my_feature_test
  core/my_feature_test.cpp
  ../src/core/my_feature.cpp
)

# 3. Run tests
./scripts/build.sh && ctest --test-dir build/release
```

### Debugging Test Failures

```bash
# Run single test with verbose output
./build/release/app/tests/buffer_manager_test --gtest_filter="BufferManagerTest.SpecificTest"

# Run with GDB
gdb --args ./build/release/app/tests/buffer_manager_test

# Run with AddressSanitizer
cmake -B build/asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
cmake --build build/asan
./build/asan/app/tests/buffer_manager_test
```

### Updating CEF Version

1. Update `CEF_VERSION` in `CMakeLists.txt`
2. Update `cmake/DownloadCEF.cmake` with new version
3. Download CEF binary to `third_party/cef_binary_${CEF_VERSION}_${PLATFORM}/`
4. Rebuild: `rm -rf build && ./scripts/build.sh`

### Homepage Development

**Development mode (HMR):**
```bash
./scripts/dev.sh
```

**Production build:**
```bash
./scripts/build-homepage.sh  # Builds and copies to resources/homepage/
./scripts/build.sh            # Automatically builds homepage first
```

**Manual homepage development:**
```bash
cd homepage
npm run dev       # Development server
npm run build     # Production build
npm run preview   # Preview production build
```

Homepage is built with Vite + React. Production builds are automatically copied to `resources/homepage/` by the build scripts.

## Important File Locations

### Configuration
- `CMakeLists.txt` - Main build configuration
- `CMakePresets.json` - Build presets (debug/release)
- `.clang-format` - Code formatting rules
- `app/CMakeLists.txt` - Source file list
- `app/tests/CMakeLists.txt` - Test configuration

### Core Implementation
- `app/src/core/` - Application and window management
- `app/src/browser/` - CEF integration
- `app/src/platform/` - Qt6 platform layer
- `app/src/rendering/` - OpenGL rendering
- `app/src/utils/` - Error handling, logging
- `app/src/resources/` - Custom scheme handlers

### Tests
- `app/tests/core/` - Application layer tests
- `app/tests/browser/` - Browser engine tests
- `app/tests/rendering/` - Rendering tests
- `app/tests/utils/` - Utility tests
- `app/tests/mocks/` - Mock implementations for testing

### Documentation
- `README.md` - Project overview and setup
- `app/tests/README.md` - Comprehensive testing guide
- `REFACTOR_PROGRESS.md` - Architecture evolution notes (historical)

## Troubleshooting

### Build Issues

**CEF not found:**
```bash
# Download CEF binary manually
# Extract to third_party/cef_binary_<version>_<platform>/
# Ensure CMakeLists.txt CEF_VERSION matches
```

**Qt6 not found:**
```bash
sudo apt-get install qt6-base-dev qt6-tools-dev libqt6opengl6-dev
```

**GTK headers missing (needed for CEF):**
```bash
sudo apt-get install libgtk-3-dev libx11-dev pkg-config
```

**OpenGL errors:**
```bash
sudo apt-get install libgl1-mesa-dev
```

### Runtime Issues

**Black screen or no rendering:**
- Verify GLRenderer initialized before browser creation
- Check browser creation happens after Qt widget initialization
- Ensure OpenGL context is properly created

**Input not working:**
- Verify Qt event handling is connected properly
- Check that CEF client receives input events

**Browser crashes on startup:**
- Ensure CEF subprocess handling in main.cpp
- Check CEF resources copied to binary directory
- Verify CEF sandbox disabled in config (requires root otherwise)

### Testing Issues

**Tests failing after refactor:**
- Run `./scripts/format.sh` to ensure consistent formatting
- Check for uninitialized variables (use valgrind)
- Verify all RAII objects properly initialized

**Slow tests:**
- Current test suite runs in <1s - if slower, investigate test setup/teardown
- Use `--gtest_filter` to isolate slow tests

## Browser Status

🚧 **Qt Migration Complete** (Core functionality working)
- Core tests passing (206+ tests)
- Qt6-based window system
- Zero compiler warnings
- Zero global state
- Full RAII resource management
- Hardware-accelerated rendering (60+ FPS)
- Complete input support (mouse, keyboard, focus)
- Multi-window support
- Clean architecture with 5 layers
- Platform/Application tests being migrated to Qt

The browser is functional with Qt6. Some platform-specific tests need to be rewritten for the Qt layer.
