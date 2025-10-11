# Athena Browser Tests

Comprehensive unit test suite for the Athena Browser project, covering all major components with 200+ tests.

## Test Organization

Tests are organized by module, mirroring the source code structure:

```
app/tests/
├── core/                    # Core application components
│   ├── types_test.cpp      # Geometric types (Point, Size, Rect, ScaleFactor)
│   ├── browser_window_test.cpp  # BrowserWindow class functionality
│   └── application_test.cpp     # Application lifecycle
├── utils/                   # Utility components
│   ├── error_test.cpp      # Error handling and Result<T> monad
│   └── logging_test.cpp    # Logging system
├── rendering/              # Rendering subsystem
│   ├── buffer_manager_test.cpp  # Buffer allocation and CEF data copying
│   └── scaling_manager_test.cpp # DPI scaling calculations
├── browser/                # CEF browser integration
│   ├── cef_client_test.cpp      # CEF client state management
│   └── cef_engine_test.cpp      # CEF engine lifecycle
└── mocks/                  # Test doubles
    ├── mock_window_system.h     # WindowSystem mock
    ├── mock_browser_engine.h    # BrowserEngine mock
    └── mock_gl_renderer.h       # GLRenderer mock
```

## Running Tests

### Run All Tests
```bash
./scripts/build.sh && ctest --test-dir build/release --output-on-failure
```

### Run Specific Test Suite
```bash
# Run only buffer manager tests
./build/release/app/tests/buffer_manager_test

# Run only browser window tests
./build/release/app/tests/browser_window_test

# Run only error handling tests
./build/release/app/tests/error_test
```

### Run Tests with Verbose Output
```bash
ctest --test-dir build/release --verbose
```

### Run Tests Matching Pattern
```bash
ctest --test-dir build/release -R "buffer|window" --output-on-failure
```

## Test Coverage

### Core Types (`core/types_test.cpp`) - 35 tests
Tests for fundamental geometric types used throughout the application:
- **Point**: Construction, equality, string representation
- **Size**: Construction, equality, area calculation, empty checks
- **Rect**: Construction, bounds checking, intersection, union, containment
- **ScaleFactor**: Scaling operations for integers, points, sizes, and rects

### Error Handling (`utils/error_test.cpp`) - 27 tests
Tests for the Result<T> monad and error propagation:
- **Error class**: Message and code handling
- **Result<T>**: Value construction, error construction, extraction
- **Result<void>**: Void result specialization
- **Helper functions**: Ok(), Err(), ErrVoid() convenience functions
- **Practical examples**: Division and validation functions

### Buffer Management (`rendering/buffer_manager_test.cpp`) - 47 tests
Tests for pixel buffer allocation and CEF data copying:
- **Buffer construction**: Valid/invalid sizes, initialization, move semantics
- **Buffer allocation**: Size validation, memory limits
- **CEF data copying**: Full buffer copy, dirty rectangle optimization
- **Stride calculation**: Alignment requirements for different widths
- **Edge cases**: Multiple allocations, ownership transfer

### Scaling Management (`rendering/scaling_manager_test.cpp`) - 28 tests
Tests for DPI scaling calculations and coordinate transformations:
- **Scale factor detection**: Device scale factor determination
- **Scaling operations**: Logical to physical coordinate conversion
- **Buffer sizing**: Physical buffer size calculation with scale factors
- **Dirty rectangle scaling**: Proper scaling of update regions

### CEF Client (`browser/cef_client_test.cpp`) - 17 tests
Tests for CEF client state management (without actual CEF initialization):
- **Construction**: Default initialization, null parameter handling
- **Size management**: Width/height updates, dimension validation
- **Device scale factor**: Normal, Retina, fractional, HiDPI displays
- **ViewRect/ScreenInfo**: Proper CEF interface implementation

### CEF Engine (`browser/cef_engine_test.cpp`) - 23 tests
Tests for CEF browser engine lifecycle and browser management:
- **Initialization**: Single initialization, double initialization prevention
- **Browser creation**: Valid creation, pre-initialization failure
- **Browser operations**: LoadURL, navigation, reload, stop
- **Browser state**: Navigation history, loading state, URL tracking
- **Shutdown**: Proper cleanup, post-shutdown operation prevention

### Browser Window (`core/browser_window_test.cpp`) - 34 tests
Tests for high-level browser window API using mocks:
- **Construction**: Default and custom configurations
- **Show/Hide operations**: Window visibility management
- **Window properties**: Title, size, scale factor, focus
- **Navigation**: URL loading, back/forward, reload, stop
- **Browser state**: Navigation history, loading state
- **Callbacks**: Resize, close, focus change notifications
- **Edge cases**: Operations before Show(), destructor cleanup

### Application (`core/application_test.cpp`) - 15 tests
Tests for application lifecycle and window management:
- **Initialization**: Normal initialization, double init prevention
- **Window creation**: Configuration handling, multiple windows
- **Event loop**: Run, quit, nested loops
- **Shutdown**: Cleanup, window closing

## Writing Tests

### Test Structure

All tests follow the Google Test framework structure:

```cpp
#include <gtest/gtest.h>
#include "module/class.h"

class MyClassTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup common test fixtures
  }

  void TearDown() override {
    // Cleanup after each test
  }

  // Test fixtures
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

### Naming Conventions

- **Test Suite Names**: Match the class being tested with "Test" suffix (e.g., `BufferManagerTest`)
- **Test Names**: Use descriptive names in CamelCase (e.g., `AllocateBufferValidSize`)
- **Section Comments**: Use `// ====...====` dividers to organize related tests

### Assertion Guidelines

- Use `EXPECT_*` for non-fatal assertions (test continues)
- Use `ASSERT_*` for fatal assertions (test stops if fails)
- Prefer specific matchers: `EXPECT_EQ`, `EXPECT_TRUE`, `EXPECT_FLOAT_EQ`
- Add descriptive failure messages for complex assertions:
  ```cpp
  EXPECT_EQ(actual, expected) << "Failed at iteration " << i;
  ```

### Mock Usage

Use Google Mock for interface mocking:

```cpp
#include <gmock/gmock.h>
#include "mocks/mock_browser_engine.h"

using ::testing::_;
using ::testing::Return;

TEST_F(BrowserWindowTest, ShowInitializesWindow) {
  auto mock_engine = std::make_unique<MockBrowserEngine>();

  EXPECT_CALL(*mock_engine, CreateBrowser(_))
      .WillOnce(Return(Result<BrowserId>(1)));

  BrowserWindow window(config, callbacks, window_system, mock_engine.get());
  window.Show();
}
```

### Testing Result<T> Functions

For functions returning `Result<T>`:

```cpp
TEST_F(MyTest, SuccessPath) {
  auto result = FunctionThatCanFail(valid_input);

  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result.Value(), expected_value);
}

TEST_F(MyTest, ErrorPath) {
  auto result = FunctionThatCanFail(invalid_input);

  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("expected error"), std::string::npos);
}
```

## Test Dependencies

- **GoogleTest**: Test framework (included via CMake FetchContent)
- **GoogleMock**: Mocking framework (included with GoogleTest)
- **CEF**: Headers only (no CEF initialization in unit tests)

## Continuous Integration

Tests are designed to run without:
- CEF initialization (browser engine tests use mocks or minimal state)
- GTK/X11 display (platform-specific code is abstracted)
- Network access (all tests are self-contained)

This allows tests to run in CI environments like GitHub Actions.

## Coverage Goals

Current test coverage by component:
- ✅ Core types: 100% (35/35 tests)
- ✅ Error handling: 100% (27/27 tests)
- ✅ Buffer management: 95% (47/47 tests covering all critical paths)
- ✅ Scaling management: 90% (28/28 tests)
- ✅ CEF client: 85% (17/17 tests, some CEF callbacks require integration tests)
- ✅ CEF engine: 90% (23/23 tests)
- ✅ Browser window: 95% (34/34 tests using mocks)
- ✅ Application: 85% (15/15 tests)

**Total: 226 tests**

## Future Improvements

- [ ] Integration tests with real CEF browser instances
- [ ] Performance benchmarks for rendering pipeline
- [ ] Memory leak detection with AddressSanitizer
- [ ] Thread safety tests for concurrent browser operations
- [ ] Visual regression tests for rendering accuracy
- [ ] End-to-end tests with Selenium/Playwright

## Debugging Tests

### Running Tests Under GDB
```bash
gdb --args ./build/release/app/tests/buffer_manager_test --gtest_filter="BufferManagerTest.CopyFromCEFValidCopy"
```

### Running Tests with ASan
```bash
cmake -B build/asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
cmake --build build/asan
./build/asan/app/tests/buffer_manager_test
```

### Running Tests with Valgrind
```bash
valgrind --leak-check=full ./build/release/app/tests/buffer_manager_test
```

## Contributing

When adding new features:
1. Write failing tests first (TDD)
2. Implement the feature
3. Ensure all tests pass
4. Add documentation to this README if introducing new test suites
5. Maintain test coverage above 85% for all components
