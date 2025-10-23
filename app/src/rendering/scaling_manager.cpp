#include "rendering/scaling_manager.h"

#include <algorithm>
#include <cmath>

namespace athena {
namespace rendering {

// ============================================================================
// Constructors
// ============================================================================

ScalingManager::ScalingManager() : scale_(1.0f) {}

ScalingManager::ScalingManager(core::ScaleFactor scale) : scale_(scale) {}

ScalingManager::ScalingManager(float scale) : scale_(scale) {}

// ============================================================================
// Move Operations
// ============================================================================

ScalingManager::ScalingManager(ScalingManager&& other) noexcept : scale_(other.scale_) {
  // Note: mutex cannot be moved, but we don't need to - it's for protecting
  // scale_ and we've already copied the value
}

ScalingManager& ScalingManager::operator=(ScalingManager&& other) noexcept {
  if (this != &other) {
    std::lock_guard<std::mutex> lock_this(mutex_);
    std::lock_guard<std::mutex> lock_other(other.mutex_);
    scale_ = other.scale_;
  }
  return *this;
}

// ============================================================================
// Scale Factor Management
// ============================================================================

core::ScaleFactor ScalingManager::GetScaleFactor() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_;
}

void ScalingManager::SetScaleFactor(core::ScaleFactor scale) {
  std::lock_guard<std::mutex> lock(mutex_);
  scale_ = scale;
}

void ScalingManager::SetScaleFactor(float scale) {
  SetScaleFactor(core::ScaleFactor(scale));
}

// ============================================================================
// Point Transformations
// ============================================================================

core::Point ScalingManager::LogicalToPhysical(const core::Point& logical) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.Scale(logical);
}

core::Point ScalingManager::PhysicalToLogical(const core::Point& physical) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.Unscale(physical);
}

// ============================================================================
// Size Transformations
// ============================================================================

core::Size ScalingManager::LogicalToPhysical(const core::Size& logical) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.Scale(logical);
}

core::Size ScalingManager::PhysicalToLogical(const core::Size& physical) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.Unscale(physical);
}

// ============================================================================
// Rectangle Transformations
// ============================================================================

core::Rect ScalingManager::LogicalToPhysical(const core::Rect& logical) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.Scale(logical);
}

core::Rect ScalingManager::PhysicalToLogical(const core::Rect& physical) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.Unscale(physical);
}

// ============================================================================
// Scalar Transformations
// ============================================================================

int ScalingManager::ScaleValue(int value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<int>(std::round(value * scale_.value));
}

int ScalingManager::UnscaleValue(int value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (scale_.value == 0.0f) {
    return 0;  // Avoid division by zero
  }
  return static_cast<int>(std::round(value / scale_.value));
}

// ============================================================================
// Utility Methods
// ============================================================================

bool ScalingManager::IsScalingEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.value != 1.0f;
}

float ScalingManager::GetScaleValue() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scale_.value;
}

}  // namespace rendering
}  // namespace athena
