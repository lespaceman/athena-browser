#include "rendering/scaling_manager.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace athena::rendering;
using namespace athena::core;

// ============================================================================
// Constructor Tests
// ============================================================================

TEST(ScalingManagerTest, DefaultConstructor) {
  ScalingManager manager;
  EXPECT_FLOAT_EQ(manager.GetScaleValue(), 1.0f);
  EXPECT_FALSE(manager.IsScalingEnabled());
}

TEST(ScalingManagerTest, ScaleFactorConstructor) {
  ScalingManager manager(ScaleFactor(2.0f));
  EXPECT_FLOAT_EQ(manager.GetScaleValue(), 2.0f);
  EXPECT_TRUE(manager.IsScalingEnabled());
}

TEST(ScalingManagerTest, FloatConstructor) {
  ScalingManager manager(1.5f);
  EXPECT_FLOAT_EQ(manager.GetScaleValue(), 1.5f);
  EXPECT_TRUE(manager.IsScalingEnabled());
}

TEST(ScalingManagerTest, MoveConstructor) {
  ScalingManager original(2.0f);
  ScalingManager moved(std::move(original));
  EXPECT_FLOAT_EQ(moved.GetScaleValue(), 2.0f);
}

TEST(ScalingManagerTest, MoveAssignment) {
  ScalingManager original(2.0f);
  ScalingManager target(1.0f);
  target = std::move(original);
  EXPECT_FLOAT_EQ(target.GetScaleValue(), 2.0f);
}

// ============================================================================
// Scale Factor Management Tests
// ============================================================================

TEST(ScalingManagerTest, GetScaleFactor) {
  ScalingManager manager(ScaleFactor(2.0f));
  ScaleFactor scale = manager.GetScaleFactor();
  EXPECT_FLOAT_EQ(scale.value, 2.0f);
}

TEST(ScalingManagerTest, SetScaleFactorWithScaleFactorType) {
  ScalingManager manager;
  manager.SetScaleFactor(ScaleFactor(3.0f));
  EXPECT_FLOAT_EQ(manager.GetScaleValue(), 3.0f);
}

TEST(ScalingManagerTest, SetScaleFactorWithFloat) {
  ScalingManager manager;
  manager.SetScaleFactor(2.5f);
  EXPECT_FLOAT_EQ(manager.GetScaleValue(), 2.5f);
}

TEST(ScalingManagerTest, IsScalingEnabledTrue) {
  ScalingManager manager(2.0f);
  EXPECT_TRUE(manager.IsScalingEnabled());
}

TEST(ScalingManagerTest, IsScalingEnabledFalse) {
  ScalingManager manager(1.0f);
  EXPECT_FALSE(manager.IsScalingEnabled());
}

// ============================================================================
// Point Transformation Tests
// ============================================================================

TEST(ScalingManagerTest, PointLogicalToPhysical_1x) {
  ScalingManager manager(1.0f);
  Point logical{100, 200};
  Point physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 100);
  EXPECT_EQ(physical.y, 200);
}

TEST(ScalingManagerTest, PointLogicalToPhysical_2x) {
  ScalingManager manager(2.0f);
  Point logical{100, 200};
  Point physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 200);
  EXPECT_EQ(physical.y, 400);
}

TEST(ScalingManagerTest, PointLogicalToPhysical_1_5x) {
  ScalingManager manager(1.5f);
  Point logical{100, 200};
  Point physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 150);
  EXPECT_EQ(physical.y, 300);
}

TEST(ScalingManagerTest, PointPhysicalToLogical_1x) {
  ScalingManager manager(1.0f);
  Point physical{100, 200};
  Point logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.x, 100);
  EXPECT_EQ(logical.y, 200);
}

TEST(ScalingManagerTest, PointPhysicalToLogical_2x) {
  ScalingManager manager(2.0f);
  Point physical{200, 400};
  Point logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.x, 100);
  EXPECT_EQ(logical.y, 200);
}

TEST(ScalingManagerTest, PointPhysicalToLogical_1_5x) {
  ScalingManager manager(1.5f);
  Point physical{150, 300};
  Point logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.x, 100);
  EXPECT_EQ(logical.y, 200);
}

TEST(ScalingManagerTest, PointRoundTrip_2x) {
  ScalingManager manager(2.0f);
  Point original{123, 456};
  Point physical = manager.LogicalToPhysical(original);
  Point back = manager.PhysicalToLogical(physical);
  EXPECT_EQ(back.x, original.x);
  EXPECT_EQ(back.y, original.y);
}

// ============================================================================
// Size Transformation Tests
// ============================================================================

TEST(ScalingManagerTest, SizeLogicalToPhysical_1x) {
  ScalingManager manager(1.0f);
  Size logical{800, 600};
  Size physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.width, 800);
  EXPECT_EQ(physical.height, 600);
}

TEST(ScalingManagerTest, SizeLogicalToPhysical_2x) {
  ScalingManager manager(2.0f);
  Size logical{800, 600};
  Size physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.width, 1600);
  EXPECT_EQ(physical.height, 1200);
}

TEST(ScalingManagerTest, SizeLogicalToPhysical_1_5x) {
  ScalingManager manager(1.5f);
  Size logical{800, 600};
  Size physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.width, 1200);
  EXPECT_EQ(physical.height, 900);
}

TEST(ScalingManagerTest, SizePhysicalToLogical_1x) {
  ScalingManager manager(1.0f);
  Size physical{800, 600};
  Size logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.width, 800);
  EXPECT_EQ(logical.height, 600);
}

TEST(ScalingManagerTest, SizePhysicalToLogical_2x) {
  ScalingManager manager(2.0f);
  Size physical{1600, 1200};
  Size logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.width, 800);
  EXPECT_EQ(logical.height, 600);
}

TEST(ScalingManagerTest, SizePhysicalToLogical_1_5x) {
  ScalingManager manager(1.5f);
  Size physical{1200, 900};
  Size logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.width, 800);
  EXPECT_EQ(logical.height, 600);
}

TEST(ScalingManagerTest, SizeRoundTrip_2x) {
  ScalingManager manager(2.0f);
  Size original{1920, 1080};
  Size physical = manager.LogicalToPhysical(original);
  Size back = manager.PhysicalToLogical(physical);
  EXPECT_EQ(back.width, original.width);
  EXPECT_EQ(back.height, original.height);
}

// ============================================================================
// Rectangle Transformation Tests
// ============================================================================

TEST(ScalingManagerTest, RectLogicalToPhysical_1x) {
  ScalingManager manager(1.0f);
  Rect logical{10, 20, 100, 200};
  Rect physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 10);
  EXPECT_EQ(physical.y, 20);
  EXPECT_EQ(physical.width, 100);
  EXPECT_EQ(physical.height, 200);
}

TEST(ScalingManagerTest, RectLogicalToPhysical_2x) {
  ScalingManager manager(2.0f);
  Rect logical{10, 20, 100, 200};
  Rect physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 20);
  EXPECT_EQ(physical.y, 40);
  EXPECT_EQ(physical.width, 200);
  EXPECT_EQ(physical.height, 400);
}

TEST(ScalingManagerTest, RectLogicalToPhysical_1_5x) {
  ScalingManager manager(1.5f);
  Rect logical{10, 20, 100, 200};
  Rect physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 15);
  EXPECT_EQ(physical.y, 30);
  EXPECT_EQ(physical.width, 150);
  EXPECT_EQ(physical.height, 300);
}

TEST(ScalingManagerTest, RectPhysicalToLogical_1x) {
  ScalingManager manager(1.0f);
  Rect physical{10, 20, 100, 200};
  Rect logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.x, 10);
  EXPECT_EQ(logical.y, 20);
  EXPECT_EQ(logical.width, 100);
  EXPECT_EQ(logical.height, 200);
}

TEST(ScalingManagerTest, RectPhysicalToLogical_2x) {
  ScalingManager manager(2.0f);
  Rect physical{20, 40, 200, 400};
  Rect logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.x, 10);
  EXPECT_EQ(logical.y, 20);
  EXPECT_EQ(logical.width, 100);
  EXPECT_EQ(logical.height, 200);
}

TEST(ScalingManagerTest, RectPhysicalToLogical_1_5x) {
  ScalingManager manager(1.5f);
  Rect physical{15, 30, 150, 300};
  Rect logical = manager.PhysicalToLogical(physical);
  EXPECT_EQ(logical.x, 10);
  EXPECT_EQ(logical.y, 20);
  EXPECT_EQ(logical.width, 100);
  EXPECT_EQ(logical.height, 200);
}

TEST(ScalingManagerTest, RectRoundTrip_2x) {
  ScalingManager manager(2.0f);
  Rect original{50, 100, 640, 480};
  Rect physical = manager.LogicalToPhysical(original);
  Rect back = manager.PhysicalToLogical(physical);
  EXPECT_EQ(back.x, original.x);
  EXPECT_EQ(back.y, original.y);
  EXPECT_EQ(back.width, original.width);
  EXPECT_EQ(back.height, original.height);
}

// ============================================================================
// Scalar Transformation Tests
// ============================================================================

TEST(ScalingManagerTest, ScaleValue_1x) {
  ScalingManager manager(1.0f);
  EXPECT_EQ(manager.ScaleValue(100), 100);
}

TEST(ScalingManagerTest, ScaleValue_2x) {
  ScalingManager manager(2.0f);
  EXPECT_EQ(manager.ScaleValue(100), 200);
}

TEST(ScalingManagerTest, ScaleValue_1_5x) {
  ScalingManager manager(1.5f);
  EXPECT_EQ(manager.ScaleValue(100), 150);
}

TEST(ScalingManagerTest, ScaleValue_Rounding) {
  ScalingManager manager(1.5f);
  EXPECT_EQ(manager.ScaleValue(7), 11);  // 7 * 1.5 = 10.5, rounds to 11
  EXPECT_EQ(manager.ScaleValue(3), 5);   // 3 * 1.5 = 4.5, rounds to 5
}

TEST(ScalingManagerTest, UnscaleValue_1x) {
  ScalingManager manager(1.0f);
  EXPECT_EQ(manager.UnscaleValue(100), 100);
}

TEST(ScalingManagerTest, UnscaleValue_2x) {
  ScalingManager manager(2.0f);
  EXPECT_EQ(manager.UnscaleValue(200), 100);
}

TEST(ScalingManagerTest, UnscaleValue_1_5x) {
  ScalingManager manager(1.5f);
  EXPECT_EQ(manager.UnscaleValue(150), 100);
}

TEST(ScalingManagerTest, UnscaleValue_Rounding) {
  ScalingManager manager(1.5f);
  EXPECT_EQ(manager.UnscaleValue(11), 7);  // 11 / 1.5 = 7.33, rounds to 7
  EXPECT_EQ(manager.UnscaleValue(5), 3);   // 5 / 1.5 = 3.33, rounds to 3
}

TEST(ScalingManagerTest, UnscaleValue_ZeroScale) {
  ScalingManager manager(0.0f);
  EXPECT_EQ(manager.UnscaleValue(100), 0);  // Should not crash
}

TEST(ScalingManagerTest, ScalarRoundTrip_2x) {
  ScalingManager manager(2.0f);
  int original = 123;
  int scaled = manager.ScaleValue(original);
  int back = manager.UnscaleValue(scaled);
  EXPECT_EQ(back, original);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(ScalingManagerTest, ZeroPoint) {
  ScalingManager manager(2.0f);
  Point zero{0, 0};
  Point physical = manager.LogicalToPhysical(zero);
  EXPECT_EQ(physical.x, 0);
  EXPECT_EQ(physical.y, 0);
}

TEST(ScalingManagerTest, ZeroSize) {
  ScalingManager manager(2.0f);
  Size zero{0, 0};
  Size physical = manager.LogicalToPhysical(zero);
  EXPECT_EQ(physical.width, 0);
  EXPECT_EQ(physical.height, 0);
}

TEST(ScalingManagerTest, ZeroRect) {
  ScalingManager manager(2.0f);
  Rect zero{0, 0, 0, 0};
  Rect physical = manager.LogicalToPhysical(zero);
  EXPECT_EQ(physical.x, 0);
  EXPECT_EQ(physical.y, 0);
  EXPECT_EQ(physical.width, 0);
  EXPECT_EQ(physical.height, 0);
}

TEST(ScalingManagerTest, NegativePoint) {
  ScalingManager manager(2.0f);
  Point negative{-100, -200};
  Point physical = manager.LogicalToPhysical(negative);
  EXPECT_EQ(physical.x, -200);
  EXPECT_EQ(physical.y, -400);
}

TEST(ScalingManagerTest, NegativeRect) {
  ScalingManager manager(2.0f);
  Rect negative{-10, -20, 100, 200};
  Rect physical = manager.LogicalToPhysical(negative);
  EXPECT_EQ(physical.x, -20);
  EXPECT_EQ(physical.y, -40);
  EXPECT_EQ(physical.width, 200);
  EXPECT_EQ(physical.height, 400);
}

TEST(ScalingManagerTest, HighDPI_3x) {
  ScalingManager manager(3.0f);
  Size logical{800, 600};
  Size physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.width, 2400);
  EXPECT_EQ(physical.height, 1800);
}

TEST(ScalingManagerTest, FractionalScale_1_25x) {
  ScalingManager manager(1.25f);
  Size logical{800, 600};
  Size physical = manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.width, 1000);
  EXPECT_EQ(physical.height, 750);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST(ScalingManagerTest, ConcurrentReads) {
  ScalingManager manager(2.0f);
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < 10; i++) {
    threads.emplace_back([&manager, &success_count]() {
      for (int j = 0; j < 1000; j++) {
        Point logical{100, 200};
        Point physical = manager.LogicalToPhysical(logical);
        if (physical.x == 200 && physical.y == 400) {
          success_count++;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(success_count, 10000);
}

TEST(ScalingManagerTest, ConcurrentWritesAndReads) {
  ScalingManager manager(1.0f);
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;

  // Writer thread
  threads.emplace_back([&manager, &stop]() {
    float scale = 1.0f;
    while (!stop) {
      manager.SetScaleFactor(scale);
      scale = (scale == 1.0f) ? 2.0f : 1.0f;
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  });

  // Reader threads
  for (int i = 0; i < 5; i++) {
    threads.emplace_back([&manager, &stop]() {
      while (!stop) {
        Point logical{100, 100};
        Point physical = manager.LogicalToPhysical(logical);
        // Should be either 100 (1x) or 200 (2x), never corrupted
        EXPECT_TRUE(physical.x == 100 || physical.x == 200);
        EXPECT_TRUE(physical.y == 100 || physical.y == 200);
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop = true;

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(ScalingManagerTest, MoveWhileReading) {
  ScalingManager manager(2.0f);
  std::atomic<bool> moved{false};
  std::thread reader([&manager, &moved]() {
    while (!moved) {
      Point logical{100, 100};
      Point physical = manager.LogicalToPhysical(logical);
      EXPECT_EQ(physical.x, 200);
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ScalingManager moved_manager(std::move(manager));
  moved = true;
  reader.join();

  // Verify moved object works
  Point logical{100, 100};
  Point physical = moved_manager.LogicalToPhysical(logical);
  EXPECT_EQ(physical.x, 200);
}
