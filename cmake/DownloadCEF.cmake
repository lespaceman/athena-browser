# Minimal CEF setup helper. Keeps network operations out of configure by default.

if(NOT DEFINED CEF_VERSION)
  set(CEF_VERSION "139.0.38+g7656fe3+chromium-139.0.7258.139")
endif()

# Derive platform suffix used by CEF binary distribution names.
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CEF_PLATFORM windows64)
  else()
    set(CEF_PLATFORM windows32)
  endif()
elseif(APPLE)
  set(CEF_PLATFORM macosx64)
else()
  set(CEF_PLATFORM linux64)
endif()

# Expected local path for the unpacked CEF distribution.
set(CEF_ROOT "${CMAKE_SOURCE_DIR}/third_party/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}" CACHE PATH "Path to unpacked CEF binary distribution")

# Reference download URL (manual fetch recommended to keep CMake side-effects minimal):
# set(CEF_TARBALL_URL "https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2")
# set(CEF_TARBALL      "${CMAKE_SOURCE_DIR}/third_party/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2")
# To fetch:
#   file(DOWNLOAD ${CEF_TARBALL_URL} ${CEF_TARBALL} SHOW_PROGRESS)
#   execute_process(COMMAND ${CMAKE_COMMAND} -E tar xjf ${CEF_TARBALL} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party)

message(STATUS "CEF version: ${CEF_VERSION}")
message(STATUS "CEF platform: ${CEF_PLATFORM}")
message(STATUS "CEF root: ${CEF_ROOT}")

