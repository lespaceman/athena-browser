cmake_minimum_required(VERSION 3.21)

project(AthenaBrowser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure CEF
set(CEF_VERSION "139.0.38+g7656fe3+chromium-139.0.7258.139" CACHE STRING "Pinned CEF version")
include(cmake/DownloadCEF.cmake)

# Disable Doxygen documentation generation (prevents warnings from CEF headers)
set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen TRUE)

# Add CEF binary cmake tree if available
if(EXISTS "${CEF_ROOT}/CMakeLists.txt")
  message(STATUS "Using CEF from ${CEF_ROOT}")
  add_subdirectory(${CEF_ROOT} cef_binary)
  set(CEF_FOUND TRUE)
else()
  message(WARNING "CEF not found at ${CEF_ROOT}. Fetch or place the binary distribution there.")
  set(CEF_FOUND FALSE)
endif()

# Enable testing and fetch GTest before adding subdirectories
enable_testing()
find_package(GTest)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Fetch nlohmann/json for JSON parsing
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(app)

# Optionally: copy homepage production bundle to resources/homepage (handled by scripts/build)

