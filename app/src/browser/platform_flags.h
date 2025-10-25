#ifndef ATHENA_BROWSER_PLATFORM_FLAGS_H_
#define ATHENA_BROWSER_PLATFORM_FLAGS_H_

#include "include/cef_command_line.h"
#include <string>

namespace athena {
namespace browser {

/**
 * @brief Platform flag presets for CEF command line configuration
 *
 * This module provides platform-specific CEF flags based on:
 * - Build type (Debug vs Release)
 * - Operating system (Linux, Windows, macOS)
 * - Known CEF OSR issues and workarounds
 *
 * References:
 * - CEF Issue #3953 (Linux ANGLE/EGL): https://github.com/chromiumembedded/cef/issues/3953
 * - CEF Issue #3870 (Focus loss): https://github.com/chromiumembedded/cef/issues/3870
 * - QCefView battle-tested flags: QCefView/src/CefViewBrowserApp.cpp
 */

/**
 * @brief Flag preset categories
 */
enum class FlagPreset {
  /**
   * Debug preset: Enables verbose logging, validation, synchronous rendering
   * Use for development, debugging, issue investigation
   */
  DEBUG,

  /**
   * Release preset: Optimized for performance, minimal logging
   * Use for production builds
   */
  RELEASE,

  /**
   * Performance preset: Maximum performance, some features disabled
   * Use for benchmarking or resource-constrained environments
   */
  PERFORMANCE,

  /**
   * Compatibility preset: Maximum compatibility, slower but safer
   * Use when experiencing GPU/rendering issues
   */
  COMPATIBILITY
};

/**
 * @brief Apply platform-specific flags to CEF command line
 *
 * This function applies flags based on the current platform and build type.
 * It encapsulates all known workarounds and optimizations from:
 * - Athena's testing
 * - QCefView's production experience
 * - CEF forum recommendations
 *
 * @param command_line CEF command line object to modify
 * @param preset Flag preset to apply (defaults to Release in production)
 */
void ApplyPlatformFlags(CefRefPtr<CefCommandLine> command_line,
                        FlagPreset preset = FlagPreset::RELEASE);

/**
 * @brief Get description of what flags would be applied for a preset
 *
 * Useful for logging and diagnostics.
 *
 * @param preset The preset to describe
 * @return Human-readable description of flags
 */
std::string GetFlagPresetDescription(FlagPreset preset);

}  // namespace browser
}  // namespace athena

#endif  // ATHENA_BROWSER_PLATFORM_FLAGS_H_
