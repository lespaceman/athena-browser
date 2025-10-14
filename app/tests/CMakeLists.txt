# Test configuration for Athena Browser

include(GoogleTest)

# Common test dependencies
set(TEST_COMMON_INCLUDES
  ${CMAKE_SOURCE_DIR}/app/src
  ${CMAKE_SOURCE_DIR}/app/tests
  ${CEF_ROOT}
  ${CEF_ROOT}/include
)

# Helper function to create a test target
function(add_athena_test test_name)
  add_executable(${test_name} ${ARGN})

  target_include_directories(${test_name} PRIVATE ${TEST_COMMON_INCLUDES})

  target_link_libraries(${test_name} PRIVATE
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
  )

  # Add CEF and GTK if needed for integration tests
  if(CEF_FOUND)
    target_link_libraries(${test_name} PRIVATE
      libcef_dll_wrapper
      ${CEF_ROOT}/Release/libcef.so
    )
  endif()

  # Add GTK3 and OpenGL for tests that need it (browser tests)
  if(UNIX AND NOT APPLE)
    find_package(OpenGL QUIET)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(GTK3_TEST QUIET gtk+-3.0)
      if(GTK3_TEST_FOUND)
        target_include_directories(${test_name} PRIVATE ${GTK3_TEST_INCLUDE_DIRS})
        target_link_libraries(${test_name} PRIVATE ${GTK3_TEST_LIBRARIES})
      endif()
    endif()
    if(OPENGL_FOUND)
      target_link_libraries(${test_name} PRIVATE ${OPENGL_LIBRARIES})
    endif()
  endif()

  gtest_discover_tests(${test_name})

  # Suppress warnings in test code
  if(NOT MSVC)
    target_compile_options(${test_name} PRIVATE -Wno-error)
  endif()
endfunction()

# Core types tests (Phase 1)
add_athena_test(core_types_test core/types_test.cpp)

# Utils tests (Phase 1)
add_athena_test(logging_test utils/logging_test.cpp ../src/utils/logging.cpp)
add_athena_test(error_test utils/error_test.cpp)

# Rendering tests (Phase 2)
add_athena_test(buffer_manager_test rendering/buffer_manager_test.cpp ../src/rendering/buffer_manager.cpp)
add_athena_test(scaling_manager_test rendering/scaling_manager_test.cpp ../src/rendering/scaling_manager.cpp)

# Browser tests (Phase 3)
add_athena_test(cef_client_test
  browser/cef_client_test.cpp
  ../src/browser/cef_client.cpp
  ../src/rendering/gl_renderer.cpp
  ${CEF_ROOT}/tests/cefclient/browser/osr_renderer.cc
)

add_athena_test(cef_engine_test
  browser/cef_engine_test.cpp
  ../src/browser/cef_engine.cpp
  ../src/browser/cef_client.cpp
  ../src/browser/app_handler.cpp
  ../src/resources/scheme_handler.cpp
  ../src/rendering/gl_renderer.cpp
  ${CEF_ROOT}/tests/cefclient/browser/osr_renderer.cc
)

# Platform tests (Phase 4)
# Note: These tests expose critical bugs in the tab implementation
# Tests are DISABLED by default (they will fail until bugs are fixed)
add_athena_test(gtk_window_tabs_test
  platform/gtk_window_tabs_test.cpp
  ../src/platform/gtk_window.cpp
  ../src/platform/gtk_window_callbacks.cpp
  ../src/platform/gtk_window_tabs.cpp
  ../src/rendering/gl_renderer.cpp
  ../src/browser/cef_client.cpp
  ../src/browser/cef_engine.cpp
  ../src/browser/app_handler.cpp
  ../src/resources/scheme_handler.cpp
  ../src/runtime/node_runtime.cpp
  ../src/utils/logging.cpp
  ${CEF_ROOT}/tests/cefclient/browser/osr_renderer.cc
)

# Application layer tests (Phase 5)
add_athena_test(browser_window_test
  core/browser_window_test.cpp
  ../src/core/browser_window.cpp
  ../src/runtime/node_runtime.cpp
  ../src/utils/logging.cpp
)

add_athena_test(application_test
  core/application_test.cpp
  ../src/core/application.cpp
  ../src/core/browser_window.cpp
  ../src/platform/gtk_window.cpp
  ../src/platform/gtk_window_callbacks.cpp
  ../src/platform/gtk_window_tabs.cpp
  ../src/rendering/gl_renderer.cpp
  ../src/browser/cef_client.cpp
  ../src/browser/cef_engine.cpp
  ../src/browser/app_handler.cpp
  ../src/resources/scheme_handler.cpp
  ../src/runtime/node_runtime.cpp
  ../src/runtime/browser_control_server.cpp
  ../src/utils/logging.cpp
  ${CEF_ROOT}/tests/cefclient/browser/osr_renderer.cc
)

# Integration tests (will be added in Phase 6)
# add_athena_test(integration_test integration/startup_test.cpp)
