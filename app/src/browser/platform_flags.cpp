#include "browser/platform_flags.h"
#include <sstream>

namespace athena {
namespace browser {

namespace {

// Helper to add flag only once (avoid duplicates)
void AddSwitch(CefRefPtr<CefCommandLine> cmd, const std::string& name) {
  if (!cmd->HasSwitch(name)) {
    cmd->AppendSwitch(name);
  }
}

void AddSwitchWithValue(CefRefPtr<CefCommandLine> cmd,
                        const std::string& name,
                        const std::string& value) {
  if (!cmd->HasSwitch(name)) {
    cmd->AppendSwitchWithValue(name, value);
  }
}

/**
 * @brief Common flags for all platforms/presets
 *
 * These are safe, well-tested flags that improve CEF OSR experience.
 */
void ApplyCommonFlags(CefRefPtr<CefCommandLine> cmd) {
  // Disable GPU sandbox (often causes issues across platforms)
  AddSwitch(cmd, "disable-gpu-sandbox");

  // Enable logging infrastructure (actual verbosity controlled per-preset)
  AddSwitch(cmd, "enable-logging");
}

/**
 * @brief Linux-specific flags for OSR
 *
 * Based on:
 * - CEF Issue #3953 (ANGLE/EGL requirement)
 * - QCefView production testing
 * - Athena's own testing
 */
void ApplyLinuxFlags(CefRefPtr<CefCommandLine> cmd, FlagPreset preset) {
#if defined(OS_LINUX)
  // Force X11 platform for proper child window embedding
  // Wayland support in CEF OSR is still experimental (2025)
  AddSwitchWithValue(cmd, "ozone-platform", "x11");

  // Use ANGLE with OpenGL ES/EGL for better OSR compatibility
  // This is CRITICAL for recent CEF versions on Linux
  // Reference: https://github.com/chromiumembedded/cef/issues/3953
  AddSwitchWithValue(cmd, "use-angle", "gl-egl");

  switch (preset) {
    case FlagPreset::DEBUG:
      // In debug mode, use in-process GPU for easier debugging
      AddSwitch(cmd, "in-process-gpu");
      // Enable GPU validation layers
      AddSwitch(cmd, "enable-gpu-debugging");
      // Verbose logging
      AddSwitchWithValue(cmd, "v", "1");
      break;

    case FlagPreset::RELEASE:
      // Release: use separate GPU process for stability
      // No in-process-gpu (default is separate process)
      // Minimal logging
      AddSwitchWithValue(cmd, "log-severity", "warning");
      break;

    case FlagPreset::PERFORMANCE:
      // Performance: aggressive optimizations
      // Use in-process GPU to avoid IPC overhead
      AddSwitch(cmd, "in-process-gpu");
      // Enable zero-copy rasterizer
      AddSwitch(cmd, "enable-zero-copy");
      // Disable logging
      AddSwitch(cmd, "disable-logging");
      break;

    case FlagPreset::COMPATIBILITY:
      // Compatibility: maximum safety, minimum assumptions
      // Use in-process GPU
      AddSwitch(cmd, "in-process-gpu");
      // Disable GPU compositing (software fallback)
      AddSwitch(cmd, "disable-gpu-compositing");
      // Disable hardware acceleration entirely (last resort)
      // AddSwitch(cmd, "disable-gpu");  // Uncomment if GPU issues persist
      // Verbose logging for diagnostics
      AddSwitchWithValue(cmd, "v", "1");
      break;
  }

  // Optional: Enable VaapiVideoDecoder for hardware video decode
  // This can significantly improve video playback performance
  // Comment out if experiencing video codec issues
  AddSwitch(cmd, "enable-features");
  AddSwitchWithValue(cmd, "enable-features", "VaapiVideoDecoder");
#endif
}

/**
 * @brief Windows-specific flags for OSR
 *
 * Windows-specific optimizations and workarounds.
 */
void ApplyWindowsFlags([[maybe_unused]] CefRefPtr<CefCommandLine> cmd,
                       [[maybe_unused]] FlagPreset preset) {
#if defined(OS_WIN)
  switch (preset) {
    case FlagPreset::DEBUG:
      // Debug: enable validation and verbose logging
      AddSwitch(cmd, "enable-gpu-debugging");
      AddSwitchWithValue(cmd, "v", "1");
      break;

    case FlagPreset::RELEASE:
      // Release: use ANGLE D3D11 backend for best compatibility
      AddSwitchWithValue(cmd, "use-angle", "d3d11");
      AddSwitchWithValue(cmd, "log-severity", "warning");
      break;

    case FlagPreset::PERFORMANCE:
      // Performance: D3D11 with optimizations
      AddSwitchWithValue(cmd, "use-angle", "d3d11");
      AddSwitch(cmd, "enable-zero-copy");
      AddSwitch(cmd, "disable-logging");
      break;

    case FlagPreset::COMPATIBILITY:
      // Compatibility: software rendering fallback
      AddSwitch(cmd, "disable-gpu-compositing");
      // Use D3D9 (older, more compatible)
      AddSwitchWithValue(cmd, "use-angle", "d3d9");
      AddSwitchWithValue(cmd, "v", "1");
      break;
  }

  // Windows DPI awareness
  // Note: Qt may handle this, but CEF needs to know too
  AddSwitch(cmd, "high-dpi-support");
  AddSwitch(cmd, "force-device-scale-factor");
#endif
}

/**
 * @brief macOS-specific flags for OSR
 *
 * macOS-specific optimizations and workarounds.
 */
void ApplyMacOSFlags([[maybe_unused]] CefRefPtr<CefCommandLine> cmd,
                     [[maybe_unused]] FlagPreset preset) {
#if defined(OS_MAC)
  switch (preset) {
    case FlagPreset::DEBUG:
      // Debug: enable validation
      AddSwitch(cmd, "enable-gpu-debugging");
      AddSwitchWithValue(cmd, "v", "1");
      break;

    case FlagPreset::RELEASE:
      // Release: use Metal backend (modern macOS GPUs)
      AddSwitchWithValue(cmd, "use-angle", "metal");
      AddSwitchWithValue(cmd, "log-severity", "warning");
      break;

    case FlagPreset::PERFORMANCE:
      // Performance: Metal with optimizations
      AddSwitchWithValue(cmd, "use-angle", "metal");
      AddSwitch(cmd, "enable-zero-copy");
      AddSwitch(cmd, "disable-logging");
      break;

    case FlagPreset::COMPATIBILITY:
      // Compatibility: OpenGL fallback
      AddSwitchWithValue(cmd, "use-angle", "gl");
      AddSwitch(cmd, "disable-gpu-compositing");
      AddSwitchWithValue(cmd, "v", "1");
      break;
  }

  // macOS Retina display support
  AddSwitch(cmd, "force-device-scale-factor");
#endif
}

}  // namespace

void ApplyPlatformFlags(CefRefPtr<CefCommandLine> command_line, FlagPreset preset) {
  if (!command_line) {
    return;
  }

  // Apply common flags first
  ApplyCommonFlags(command_line);

  // Apply platform-specific flags
  ApplyLinuxFlags(command_line, preset);
  ApplyWindowsFlags(command_line, preset);
  ApplyMacOSFlags(command_line, preset);
}

std::string GetFlagPresetDescription(FlagPreset preset) {
  std::ostringstream desc;

  desc << "Platform Flag Preset: ";

  switch (preset) {
    case FlagPreset::DEBUG:
      desc << "DEBUG\n"
           << "  - Verbose logging (--v=1)\n"
           << "  - GPU validation layers\n"
           << "  - In-process GPU (easier debugging)\n"
           << "  - Synchronous rendering\n"
           << "  Use for: Development, debugging, issue investigation";
      break;

    case FlagPreset::RELEASE:
      desc << "RELEASE\n"
           << "  - Minimal logging (warnings only)\n"
           << "  - Optimized GPU backend (ANGLE D3D11/Metal/GL-EGL)\n"
           << "  - Separate GPU process (stability)\n"
           << "  - Hardware acceleration enabled\n"
           << "  Use for: Production builds, end users";
      break;

    case FlagPreset::PERFORMANCE:
      desc << "PERFORMANCE\n"
           << "  - No logging (--disable-logging)\n"
           << "  - Zero-copy rasterizer (--enable-zero-copy)\n"
           << "  - In-process GPU (reduced IPC overhead)\n"
           << "  - Maximum hardware acceleration\n"
           << "  Use for: Benchmarking, resource-constrained systems";
      break;

    case FlagPreset::COMPATIBILITY:
      desc << "COMPATIBILITY\n"
           << "  - Verbose logging for diagnostics\n"
           << "  - Software rendering fallback\n"
           << "  - Older/safer GPU backends\n"
           << "  - Conservative optimizations\n"
           << "  Use for: Troubleshooting GPU/rendering issues";
      break;
  }

  desc << "\n\nPlatform-specific flags applied for: ";
#if defined(OS_LINUX)
  desc << "Linux (X11 + ANGLE GL-EGL)";
#elif defined(OS_WIN)
  desc << "Windows (ANGLE D3D11)";
#elif defined(OS_MAC)
  desc << "macOS (ANGLE Metal)";
#else
  desc << "Unknown platform";
#endif

  return desc.str();
}

}  // namespace browser
}  // namespace athena
