# Test configuration for Athena Browser

include(GoogleTest)

# Common test dependencies
set(TEST_COMMON_INCLUDES
  ${CMAKE_SOURCE_DIR}/app/src
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
# add_athena_test(osr_renderer_test rendering/osr_renderer_test.cpp ../src/rendering/osr_renderer.cpp)

# Platform tests (will be added in Phase 2-3)
# add_athena_test(input_events_test platform/input_events_test.cpp)
# add_athena_test(gtk_input_test platform/gtk_input_test.cpp ../src/platform/gtk_input.cpp)

# Browser tests (will be added in Phase 3)
# add_athena_test(cef_engine_test browser/cef_engine_test.cpp ../src/browser/cef_engine.cpp)

# Integration tests (will be added in Phase 6)
# add_athena_test(integration_test integration/startup_test.cpp)
