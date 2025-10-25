# Athena Browser Documentation

This directory contains detailed documentation for developers working on Athena Browser.

## Documentation Index

### [KNOWN_ISSUES.md](KNOWN_ISSUES.md)
**Known Issues Playbook** - Comprehensive catalog of CEF OSR issues, platform quirks, and workarounds

Contains battle-tested solutions from:
- Athena's development experience
- QCefView production deployments
- CEF forum recommendations

Topics covered:
- **CEF Core Issues**: Focus loss (CEF #3870), message router lifecycle, renderer crashes
- **Platform-Specific Issues**: Linux ANGLE/EGL requirements, Windows DPI scaling, X11 vs Wayland
- **Performance Gotchas**: Dirty rect optimization, cache management, GPU memory leaks
- **Input Handling**: Keyboard focus, IME support, touch input
- **Rendering Issues**: Transparency, V-Sync, navigation artifacts
- **Debugging Tips**: CEF logging, DevTools, GPU diagnostics

### [CURSOR_VISIBILITY_FIX.md](CURSOR_VISIBILITY_FIX.md)
**Cursor Visibility Fix** - Implementation details for CEF cursor state synchronization

Documents the fix for cursor visibility issues in CEF OSR mode.

## Platform Flag Presets

See `app/src/browser/platform_flags.{h,cpp}` for implementation details.

Athena provides centralized platform-specific CEF flags:

### Available Presets

| Preset | Use Case | Characteristics |
|--------|----------|----------------|
| **DEBUG** | Development, debugging | Verbose logging, GPU validation, in-process GPU |
| **RELEASE** | Production (default) | Optimized GPU backend, minimal logging, stability |
| **PERFORMANCE** | Benchmarking | Zero-copy, no logging, maximum speed |
| **COMPATIBILITY** | GPU troubleshooting | Software rendering, older backends, diagnostics |

### Usage

```bash
# Default: RELEASE in release builds, DEBUG in debug builds
./build/release/app/athena-browser

# Override via environment variable
ATHENA_FLAG_PRESET=debug ./build/release/app/athena-browser
ATHENA_FLAG_PRESET=performance ./build/release/app/athena-browser
ATHENA_FLAG_PRESET=compatibility ./build/release/app/athena-browser

# Test script (shows preset details)
./scripts/test-platform-flags.sh
./scripts/test-platform-flags.sh debug
```

### Platform-Specific Flags

Automatically applied based on OS:

**Linux (Current Platform)**
- `--use-angle=gl-egl` (required for OSR on recent CEF)
- `--ozone-platform=x11` (Wayland support incomplete)
- `--enable-features=VaapiVideoDecoder` (hardware video decode)
- Debug: `--in-process-gpu`, `--enable-gpu-debugging`
- Release: Separate GPU process
- Performance: `--enable-zero-copy`
- Compatibility: `--disable-gpu-compositing` (software fallback)

**Windows**
- `--use-angle=d3d11` (D3D11 backend, best compatibility)
- `--high-dpi-support` (DPI awareness)
- Performance: `--enable-zero-copy`
- Compatibility: `--use-angle=d3d9` (older, safer)

**macOS**
- `--use-angle=metal` (Metal backend, modern GPUs)
- `--force-device-scale-factor` (Retina support)
- Compatibility: `--use-angle=gl` (OpenGL fallback)

## Quick Links

- [Main README](../README.md) - Project overview and setup
- [CLAUDE.md](../CLAUDE.md) - Development guide for Claude Code
- [Test Guide](../app/tests/README.md) - Testing documentation

## Contributing

When you encounter a new issue:

1. Document symptoms clearly in KNOWN_ISSUES.md
2. Identify root cause (use logs, debugger)
3. Describe solution/workaround
4. Add references (CEF forum, GitHub issues)
5. Update this index if adding new documentation

This documentation is living - keep it current as CEF evolves.
