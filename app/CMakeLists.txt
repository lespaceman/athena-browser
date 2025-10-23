# ============================================================================
# Qt6 Configuration
# ============================================================================
message(STATUS "Building with Qt6 support")

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS
  Core
  Widgets
  OpenGL
  OpenGLWidgets
  Network
)

# Enable Qt MOC (Meta-Object Compiler) for signals/slots
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Define platform source files
set(PLATFORM_SOURCES
  src/platform/qt_mainwindow.cpp
  src/platform/qt_browserwidget.cpp
  src/platform/qt_agent_panel.cpp
)

add_compile_definitions(ATHENA_USE_QT)

# ============================================================================
# Main Executable
# ============================================================================

# Runtime sources
set(RUNTIME_SOURCES
  src/runtime/node_runtime.cpp
  src/runtime/browser_control_server.cpp
  src/runtime/browser_control_server_routing.cpp
  src/runtime/browser_control_handlers_navigation.cpp
  src/runtime/browser_control_handlers_tabs.cpp
  src/runtime/browser_control_handlers_content.cpp
  src/runtime/browser_control_handlers_extraction.cpp
  src/runtime/js_execution_utils.cpp
)

add_executable(athena-browser
  src/main.cpp
  # Phase 1: Utilities
  src/utils/logging.cpp
  # Phase 2: Rendering Layer
  src/rendering/buffer_manager.cpp
  src/rendering/scaling_manager.cpp
  src/rendering/gl_renderer.cpp
  # Phase 3: Browser Engine Layer
  src/browser/cef_client.cpp
  src/browser/cef_engine.cpp
  src/browser/app_handler.cpp
  # Phase 4: Platform Layer (Qt)
  ${PLATFORM_SOURCES}
  # Phase 5: Application Layer
  src/core/browser_window.cpp
  src/core/application.cpp
  # Phase 6: Runtime Layer
  ${RUNTIME_SOURCES}
  # Resources
  src/resources/scheme_handler.cpp
  # CEF's official OpenGL renderer
  ${CEF_ROOT}/tests/cefclient/browser/osr_renderer.cc
)

target_include_directories(athena-browser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(athena-browser PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(CEF_FOUND)
  target_include_directories(athena-browser SYSTEM PRIVATE ${CEF_ROOT} ${CEF_ROOT}/include)

  # Link CEF shared library and wrapper
  target_link_libraries(athena-browser PRIVATE
    libcef_dll_wrapper
    ${CEF_ROOT}/Release/libcef.so
    ${CEF_STANDARD_LIBS}
    nlohmann_json::nlohmann_json
  )

  if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)

    # Find GTK3 (needed for headers because CEF has GTK dependencies)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    # Qt build: Include GTK headers (for gtk_disable_setlocale), link Qt and OpenGL
    target_include_directories(athena-browser PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(athena-browser PRIVATE
      Qt6::Core
      Qt6::Widgets
      Qt6::OpenGL
      Qt6::OpenGLWidgets
      Qt6::Network
      ${X11_LIBRARIES}
      ${OPENGL_LIBRARIES}
      ${GTK3_LIBRARIES}
    )

    # Ensure the runtime loader can find libcef.so next to the executable
    set_target_properties(athena-browser PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
  endif()
else()
  message(WARNING "Building without CEF linkage; place CEF at ${CEF_ROOT} to enable a full build.")
endif()

if(MSVC)
  target_compile_options(athena-browser PRIVATE /permissive- /W4)
else()
  # Override CEF's -Werror for missing field initializers
  target_compile_options(athena-browser PRIVATE -Wall -Wextra -Wpedantic -Wno-error)
endif()

# Post-build: copy CEF Resources (icudtl.dat, locales/) next to the binary
if(CEF_FOUND)
  add_custom_command(TARGET athena-browser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Resources" "$<TARGET_FILE_DIR:athena-browser>"
    COMMENT "Copying CEF Resources and locales to output dir")

  # Copy essential CEF binary files next to the executable (Linux)
  set(CEF_BIN_DIR "${CEF_ROOT}/Release")
  foreach(f IN ITEMS chrome-sandbox libcef.so libEGL.so libGLESv2.so v8_context_snapshot.bin vk_swiftshader_icd.json libvk_swiftshader.so libvulkan.so.1)
    if(EXISTS "${CEF_BIN_DIR}/${f}")
      add_custom_command(TARGET athena-browser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BIN_DIR}/${f}" "$<TARGET_FILE_DIR:athena-browser>/${f}")
    endif()
  endforeach()
endif()

# Install rules (staging minimal runtime layout)
install(TARGETS athena-browser
  RUNTIME DESTINATION bin
  BUNDLE DESTINATION .)

if(CEF_FOUND)
  install(DIRECTORY "${CEF_ROOT}/Resources/" DESTINATION bin)
endif()

# Install web assets if present (built by scripts/build.sh)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/web/" DESTINATION bin/resources/web OPTIONAL)

# Add tests subdirectory
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
