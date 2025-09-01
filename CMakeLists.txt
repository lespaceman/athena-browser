cmake_minimum_required(VERSION 3.21)

project(AthenaBrowser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure CEF
set(CEF_VERSION "139.0.38+g7656fe3+chromium-139.0.7258.139" CACHE STRING "Pinned CEF version")
include(cmake/DownloadCEF.cmake)

# Add CEF binary cmake tree if available
if(EXISTS "${CEF_ROOT}/CMakeLists.txt")
  message(STATUS "Using CEF from ${CEF_ROOT}")
  add_subdirectory(${CEF_ROOT} cef_binary)
  set(CEF_FOUND TRUE)
else()
  message(WARNING "CEF not found at ${CEF_ROOT}. Fetch or place the binary distribution there.")
  set(CEF_FOUND FALSE)
endif()

add_subdirectory(app)

# Optionally: copy frontend production bundle to resources/web (handled by scripts/build)

