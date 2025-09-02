add_executable(athena-browser
  src/main.cpp
  src/app_handler.cpp
  src/scheme_handler.cpp
)

target_compile_definitions(athena-browser PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(CEF_FOUND)
  target_include_directories(athena-browser PRIVATE ${CEF_ROOT} ${CEF_ROOT}/include)
  # Link against the CEF helper wrapper; the shared CEF library is loaded at runtime.
  # Link CEF shared library and wrapper
  target_link_libraries(athena-browser PRIVATE 
    libcef_dll_wrapper 
    ${CEF_ROOT}/Release/libcef.so
    ${CEF_STANDARD_LIBS}
  )
  # Ensure the runtime loader can find libcef.so next to the executable on Linux.
  if(UNIX AND NOT APPLE)
    set_target_properties(athena-browser PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
  endif()
else()
  message(WARNING "Building without CEF linkage; place CEF at ${CEF_ROOT} to enable a full build.")
endif()

if(MSVC)
  target_compile_options(athena-browser PRIVATE /permissive- /W4)
else()
  target_compile_options(athena-browser PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Post-build: copy CEF Resources (icudtl.dat, locales/) next to the binary for all configs
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
