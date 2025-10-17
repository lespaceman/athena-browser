# ============================================================================
# Build Configuration Options
# ============================================================================
option(USE_QT "Build with Qt instead of GTK (Phase 1 migration)" ON)

# ============================================================================
# Qt Configuration (if enabled)
# ============================================================================
if(USE_QT)
  message(STATUS "Building with Qt6 support")

  # Find Qt6 packages
  find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    OpenGLWidgets
  )

  # Enable Qt MOC (Meta-Object Compiler) for signals/slots
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  set(CMAKE_AUTOUIC ON)

  # Define platform source files for Qt
  set(PLATFORM_SOURCES
    src/platform/qt_mainwindow.cpp
    src/platform/qt_browserwidget.cpp
    src/platform/qt_claude_panel.cpp
  )

  add_compile_definitions(ATHENA_USE_QT)
else()
  message(STATUS "Building with GTK3 support")

  # Define platform source files for GTK
  set(PLATFORM_SOURCES
    src/platform/gtk_window.cpp
    src/platform/gtk_window_callbacks.cpp
    src/platform/gtk_window_system.cpp
    src/platform/gtk_window_tabs.cpp
    src/platform/gtk_window_sidebar.cpp
    src/platform/gtk_window_browser_control.cpp
  )
endif()

# ============================================================================
# Main Executable
# ============================================================================

# Runtime sources (Qt and GTK both support browser_control_server now)
set(RUNTIME_SOURCES
  src/runtime/node_runtime.cpp
  src/runtime/browser_control_server.cpp
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
  # Phase 4: Platform Layer (conditional - Qt or GTK)
  ${PLATFORM_SOURCES}
  # Phase 5: Application Layer
  src/core/browser_window.cpp
  src/core/application.cpp
  # Phase 6: Runtime Layer (conditional based on platform)
  ${RUNTIME_SOURCES}
  # Resources
  src/resources/scheme_handler.cpp
  # CEF's official OpenGL renderer
  ${CEF_ROOT}/tests/cefclient/browser/osr_renderer.cc
)

target_include_directories(athena-browser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(athena-browser PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(CEF_FOUND)
  target_include_directories(athena-browser PRIVATE ${CEF_ROOT} ${CEF_ROOT}/include)

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

    # Find GTK3 (needed for headers even in Qt builds because CEF has GTK dependencies)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    if(USE_QT)
      # Qt build: Include GTK headers (for gtk_disable_setlocale), but only link Qt
      target_include_directories(athena-browser PRIVATE ${GTK3_INCLUDE_DIRS})
      target_link_libraries(athena-browser PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        ${X11_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${GTK3_LIBRARIES}
      )
    else()
      # Find and link GTK3 and X11 for proper window embedding
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
      pkg_check_modules(GDK3_X11 REQUIRED gdk-x11-3.0)

      target_include_directories(athena-browser PRIVATE ${GTK3_INCLUDE_DIRS} ${GDK3_X11_INCLUDE_DIRS})
      target_link_libraries(athena-browser PRIVATE
        ${GTK3_LIBRARIES}
        ${GDK3_X11_LIBRARIES}
        ${X11_LIBRARIES}
        ${OPENGL_LIBRARIES}
      )
      target_compile_options(athena-browser PRIVATE ${GTK3_CFLAGS_OTHER})
    endif()

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

# ============================================================================
# Additional test executables for scaling experiments
# ============================================================================
# NOTE: Commented out during refactoring - will be removed in Phase 7

# # Integer scaling test version
# add_executable(athena_gtk_osr_int_scale
#   src/main_gtk_osr_int_scale.cpp
#   src/app_handler.cpp
#   src/scheme_handler.cpp
# )
#
# target_compile_definitions(athena_gtk_osr_int_scale PRIVATE $<$<CONFIG:Debug>:DEBUG>)
#
# if(CEF_FOUND)
#   target_include_directories(athena_gtk_osr_int_scale PRIVATE ${CEF_ROOT} ${CEF_ROOT}/include)
#   target_link_libraries(athena_gtk_osr_int_scale PRIVATE
#     libcef_dll_wrapper
#     ${CEF_ROOT}/Release/libcef.so
#     ${CEF_STANDARD_LIBS}
#   )
#
#   if(UNIX AND NOT APPLE)
#     target_include_directories(athena_gtk_osr_int_scale PRIVATE ${GTK3_INCLUDE_DIRS} ${GDK3_X11_INCLUDE_DIRS})
#     target_link_libraries(athena_gtk_osr_int_scale PRIVATE ${GTK3_LIBRARIES} ${GDK3_X11_LIBRARIES} ${X11_LIBRARIES})
#     target_compile_options(athena_gtk_osr_int_scale PRIVATE ${GTK3_CFLAGS_OTHER})
#     set_target_properties(athena_gtk_osr_int_scale PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
#   endif()
# endif()
#
# if(NOT MSVC)
#   target_compile_options(athena_gtk_osr_int_scale PRIVATE -Wall -Wextra -Wpedantic -Wno-error)
# endif()
#
# # 1.5x scaling test version (sweet spot between 1x and 2x)
# add_executable(athena_gtk_osr_1.5x
#   src/main_gtk_osr_1.5x.cpp
#   src/app_handler.cpp
#   src/scheme_handler.cpp
# )
#
# target_compile_definitions(athena_gtk_osr_1.5x PRIVATE $<$<CONFIG:Debug>:DEBUG>)
#
# if(CEF_FOUND)
#   target_include_directories(athena_gtk_osr_1.5x PRIVATE ${CEF_ROOT} ${CEF_ROOT}/include)
#   target_link_libraries(athena_gtk_osr_1.5x PRIVATE
#     libcef_dll_wrapper
#     ${CEF_ROOT}/Release/libcef.so
#     ${CEF_STANDARD_LIBS}
#   )
#
#   if(UNIX AND NOT APPLE)
#     target_include_directories(athena_gtk_osr_1.5x PRIVATE ${GTK3_INCLUDE_DIRS} ${GDK3_X11_INCLUDE_DIRS})
#     target_link_libraries(athena_gtk_osr_1.5x PRIVATE ${GTK3_LIBRARIES} ${GDK3_X11_LIBRARIES} ${X11_LIBRARIES})
#     target_compile_options(athena_gtk_osr_1.5x PRIVATE ${GTK3_CFLAGS_OTHER})
#     set_target_properties(athena_gtk_osr_1.5x PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
#   endif()
# endif()
#
# if(NOT MSVC)
#   target_compile_options(athena_gtk_osr_1.5x PRIVATE -Wall -Wextra -Wpedantic -Wno-error)
# endif()

# Add tests subdirectory
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
