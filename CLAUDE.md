# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Athena Browser is a CEF-based desktop browser with GTK3 integration and React frontend. The project uses C++17 for the native application and TypeScript/React for the UI layer.

## Architecture

The codebase follows a clean, layered architecture with 5 main layers:

```
main.cpp (152 LOC)
    ↓
core/application.cpp (232 LOC) - Multi-window lifecycle management
    ↓
core/browser_window.cpp (308 LOC) - High-level browser window wrapper
    ↓               ↓
platform/gtk_window.cpp (782 LOC)   browser/cef_engine.cpp (315 LOC)
    ↓                                    ↓
rendering/gl_renderer.cpp (180 LOC) - OpenGL hardware-accelerated rendering
```

**Layer Responsibilities:**
- **Foundation:** Core types (Point, Size, Rect, ScaleFactor), error handling (Result<T>), logging
- **Rendering:** Buffer management, scaling, OpenGL rendering (uses CEF's official osr_renderer.cc)
- **Browser Engine:** CEF abstraction, browser lifecycle, navigation
- **Platform:** GTK3 window system abstraction, event handling
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

# Direct execution (must set GDK_BACKEND)
GDK_BACKEND=x11 build/release/app/athena-browser
```

**Important:** Browser requires `GDK_BACKEND=x11` for proper CEF window embedding (works on both X11 and Wayland).

### Development Mode (Frontend)

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

**Current Test Count:** 267 tests (100% passing)
- Core types: 42 tests
- Error handling: 24 tests
- Logging: 8 tests
- Buffer management: 30 tests
- Scaling management: 51 tests
- CEF engine: 35 tests
- CEF client: 16 tests
- Browser window: 32 tests
- Application: 29 tests

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

### CEF Integration Notes

**Browser Creation Lifecycle:**
- Browser creation MUST happen asynchronously after GTK window realization
- GLRenderer must be initialized before creating browser
- Browser creation happens in `GtkWindow::OnRealize()` callback
- Never call `gtk_main_iteration()` before `gtk_main()` - causes deadlock

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
- All GTK callbacks use dependency injection via `user_data` pointer
- No global state for CEF clients or renderers
- Follow CEF's official patterns from `browser_window_osr_gtk.cc`

### OpenGL Rendering

The browser uses CEF's official OpenGL renderer (`osr_renderer.cc`):

```cpp
class GLRenderer {
  // Wraps CEF's OsrRenderer
  Result<void> Initialize(GtkWidget* gl_area);
  void OnPaint(CefRefPtr<CefBrowser>, ...);  // Called by CEF
  Result<void> Render();                     // Called by GTK
};
```

Hardware-accelerated, capable of 60+ FPS.

### Adding New Files

When adding new source files:

1. Add to `app/CMakeLists.txt` in the appropriate section (by phase)
2. Add corresponding test to `app/tests/CMakeLists.txt` using `add_athena_test()` helper
3. Follow namespace structure: `athena::<layer>::<component>`
4. Include header guards and proper #include ordering (see .clang-format)

### Platform-Specific Code

GTK-specific code is isolated in `app/src/platform/gtk_window.cpp`. When modifying:

- All event handlers use static C functions (required by GTK)
- Use `user_data` parameter for dependency injection
- Never use global state
- All widget pointers are non-owning (GTK manages lifecycle)

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

### Running Frontend Only

```bash
cd frontend
npm run dev       # Development server
npm run build     # Production build
npm run preview   # Preview production build
```

Frontend is built with Vite + React. Production builds are copied to `resources/web/`.

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
- `app/src/platform/` - GTK platform layer
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

**GTK headers missing:**
```bash
sudo apt-get install libgtk-3-dev libx11-dev pkg-config
```

**OpenGL errors:**
```bash
sudo apt-get install libgl1-mesa-dev
```

### Runtime Issues

**Black screen or no rendering:**
- Check `GDK_BACKEND=x11` is set
- Verify GLRenderer initialized before browser creation
- Check browser created in `OnRealize()` callback, not synchronously

**Input not working:**
- Verify `cef_client_` is cached in `GtkWindow::SetBrowser()`
- Check all event handlers receive valid `BrowserContext*`

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

✅ **Production Ready** (as of 2025-10-12)
- 267/267 tests passing (100% pass rate)
- Zero compiler warnings
- Zero global state
- Full RAII resource management
- Hardware-accelerated rendering (60+ FPS)
- Complete input support (mouse, keyboard, focus)
- Multi-window support
- Clean architecture with 5 layers

The browser is fully functional and can be used for development or extended with new features.
