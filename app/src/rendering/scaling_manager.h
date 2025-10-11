#ifndef ATHENA_RENDERING_SCALING_MANAGER_H_
#define ATHENA_RENDERING_SCALING_MANAGER_H_

#include "core/types.h"
#include <mutex>

namespace athena {
namespace rendering {

// ScalingManager handles coordinate transformations between logical and physical
// coordinate spaces for HiDPI displays.
//
// Logical coordinates: What the window/widget thinks it is (e.g., 800x600)
// Physical coordinates: What CEF renders at (e.g., 1600x1200 at 2x scale)
//
// This class is thread-safe for reading and writing the scale factor.
class ScalingManager {
 public:
  // Create with default 1.0x scaling
  ScalingManager();

  // Create with specific scale factor
  explicit ScalingManager(core::ScaleFactor scale);

  // Create with float scale value
  explicit ScalingManager(float scale);

  ~ScalingManager() = default;

  // Move-only (contains mutex)
  ScalingManager(ScalingManager&&) noexcept;
  ScalingManager& operator=(ScalingManager&&) noexcept;
  ScalingManager(const ScalingManager&) = delete;
  ScalingManager& operator=(const ScalingManager&) = delete;

  // ============================================================================
  // Scale Factor Management
  // ============================================================================

  // Get current scale factor (thread-safe)
  core::ScaleFactor GetScaleFactor() const;

  // Set scale factor (thread-safe)
  void SetScaleFactor(core::ScaleFactor scale);
  void SetScaleFactor(float scale);

  // ============================================================================
  // Point Transformations
  // ============================================================================

  // Convert logical point to physical coordinates
  // Example: LogicalToPhysical({100, 100}) with 2x scale -> {200, 200}
  core::Point LogicalToPhysical(const core::Point& logical) const;

  // Convert physical point to logical coordinates
  // Example: PhysicalToLogical({200, 200}) with 2x scale -> {100, 100}
  core::Point PhysicalToLogical(const core::Point& physical) const;

  // ============================================================================
  // Size Transformations
  // ============================================================================

  // Convert logical size to physical size
  // Example: LogicalToPhysical({800, 600}) with 2x scale -> {1600, 1200}
  core::Size LogicalToPhysical(const core::Size& logical) const;

  // Convert physical size to logical size
  // Example: PhysicalToLogical({1600, 1200}) with 2x scale -> {800, 600}
  core::Size PhysicalToLogical(const core::Size& physical) const;

  // ============================================================================
  // Rectangle Transformations
  // ============================================================================

  // Convert logical rectangle to physical coordinates and size
  // Example: LogicalToPhysical({10, 10, 100, 100}) with 2x scale -> {20, 20, 200, 200}
  core::Rect LogicalToPhysical(const core::Rect& logical) const;

  // Convert physical rectangle to logical coordinates and size
  // Example: PhysicalToLogical({20, 20, 200, 200}) with 2x scale -> {10, 10, 100, 100}
  core::Rect PhysicalToLogical(const core::Rect& physical) const;

  // ============================================================================
  // Scalar Transformations
  // ============================================================================

  // Scale a scalar value (dimension, distance, etc.)
  int ScaleValue(int value) const;

  // Unscale a scalar value
  int UnscaleValue(int value) const;

  // ============================================================================
  // Utility Methods
  // ============================================================================

  // Check if scaling is enabled (scale != 1.0)
  bool IsScalingEnabled() const;

  // Get scale factor as float
  float GetScaleValue() const;

 private:
  core::ScaleFactor scale_;
  mutable std::mutex mutex_;  // Protects scale_
};

}  // namespace rendering
}  // namespace athena

#endif  // ATHENA_RENDERING_SCALING_MANAGER_H_
