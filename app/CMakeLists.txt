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
