#include "rendering/buffer_manager.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

using namespace athena::rendering;
using namespace athena::core;
using namespace athena::utils;

class BufferManagerTest : public ::testing::Test {
 protected:
  BufferManager manager_;
};

// ============================================================================
// Buffer Construction Tests
// ============================================================================

TEST_F(BufferManagerTest, BufferConstructionValidSize) {
  Size size{800, 600};
  BufferManager::Buffer buffer(size);

  EXPECT_TRUE(buffer.IsValid());
  EXPECT_EQ(buffer.physical_size, size);
  EXPECT_EQ(buffer.stride, 800 * 4);
  EXPECT_NE(buffer.GetData(), nullptr);
  EXPECT_EQ(buffer.GetSizeInBytes(), 800 * 4 * 600);
}

TEST_F(BufferManagerTest, BufferConstructionEmptySize) {
  Size size{0, 0};
  BufferManager::Buffer buffer(size);

  EXPECT_FALSE(buffer.IsValid());
  EXPECT_EQ(buffer.GetData(), nullptr);
}

TEST_F(BufferManagerTest, BufferConstructionZeroWidth) {
  Size size{0, 600};
  BufferManager::Buffer buffer(size);

  EXPECT_FALSE(buffer.IsValid());
}

TEST_F(BufferManagerTest, BufferConstructionZeroHeight) {
  Size size{800, 0};
  BufferManager::Buffer buffer(size);

  EXPECT_FALSE(buffer.IsValid());
}

TEST_F(BufferManagerTest, BufferInitializedToZero) {
  Size size{10, 10};
  BufferManager::Buffer buffer(size);

  ASSERT_TRUE(buffer.IsValid());

  // Check that buffer is initialized to zero (transparent black)
  const uint8_t* data = buffer.GetData();
  for (size_t i = 0; i < buffer.GetSizeInBytes(); ++i) {
    EXPECT_EQ(data[i], 0) << "Byte at index " << i << " is not zero";
  }
}

// ============================================================================
// Buffer Move Semantics Tests
// ============================================================================

TEST_F(BufferManagerTest, BufferMoveConstruction) {
  Size size{100, 100};
  BufferManager::Buffer buffer1(size);
  ASSERT_TRUE(buffer1.IsValid());

  uint8_t* original_ptr = buffer1.GetData();

  BufferManager::Buffer buffer2(std::move(buffer1));

  EXPECT_TRUE(buffer2.IsValid());
  EXPECT_EQ(buffer2.GetData(), original_ptr);
  EXPECT_EQ(buffer2.physical_size, size);
}

TEST_F(BufferManagerTest, BufferMoveAssignment) {
  Size size{100, 100};
  BufferManager::Buffer buffer1(size);
  ASSERT_TRUE(buffer1.IsValid());

  uint8_t* original_ptr = buffer1.GetData();

  Size dummy_size{50, 50};
  BufferManager::Buffer buffer2(dummy_size);
  buffer2 = std::move(buffer1);

  EXPECT_TRUE(buffer2.IsValid());
  EXPECT_EQ(buffer2.GetData(), original_ptr);
  EXPECT_EQ(buffer2.physical_size, size);
}

// ============================================================================
// AllocateBuffer Tests
// ============================================================================

TEST_F(BufferManagerTest, AllocateBufferValidSize) {
  Size size{800, 600};
  auto result = manager_.AllocateBuffer(size);

  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();
  ASSERT_NE(buffer, nullptr);
  EXPECT_TRUE(buffer->IsValid());
  EXPECT_EQ(buffer->physical_size, size);
}

TEST_F(BufferManagerTest, AllocateBufferSmallSize) {
  Size size{1, 1};
  auto result = manager_.AllocateBuffer(size);

  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();
  EXPECT_TRUE(buffer->IsValid());
}

TEST_F(BufferManagerTest, AllocateBufferLargeSize) {
  Size size{3840, 2160};  // 4K resolution
  auto result = manager_.AllocateBuffer(size);

  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();
  EXPECT_TRUE(buffer->IsValid());
}

TEST_F(BufferManagerTest, AllocateBufferInvalidZeroSize) {
  Size size{0, 0};
  auto result = manager_.AllocateBuffer(size);

  EXPECT_TRUE(result.IsError());
  EXPECT_NE(result.GetError().Message().find("Invalid buffer size"), std::string::npos);
}

TEST_F(BufferManagerTest, AllocateBufferInvalidNegativeWidth) {
  Size size{-100, 100};
  auto result = manager_.AllocateBuffer(size);

  EXPECT_TRUE(result.IsError());
}

TEST_F(BufferManagerTest, AllocateBufferInvalidNegativeHeight) {
  Size size{100, -100};
  auto result = manager_.AllocateBuffer(size);

  EXPECT_TRUE(result.IsError());
}

TEST_F(BufferManagerTest, AllocateBufferTooLargeWidth) {
  Size size{10000, 100};  // Exceeds MAX_WIDTH
  auto result = manager_.AllocateBuffer(size);

  EXPECT_TRUE(result.IsError());
}

TEST_F(BufferManagerTest, AllocateBufferTooLargeHeight) {
  Size size{100, 10000};  // Exceeds MAX_HEIGHT
  auto result = manager_.AllocateBuffer(size);

  EXPECT_TRUE(result.IsError());
}

TEST_F(BufferManagerTest, AllocateBuffer8KResolution) {
  Size size{7680, 4320};  // 8K resolution
  auto result = manager_.AllocateBuffer(size);

  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();
  EXPECT_TRUE(buffer->IsValid());
}

// ============================================================================
// CopyFromCEF Tests
// ============================================================================

TEST_F(BufferManagerTest, CopyFromCEFValidCopy) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  // Create fake CEF buffer with pattern
  std::vector<uint8_t> cef_buffer(size.width * 4 * size.height);
  for (size_t i = 0; i < cef_buffer.size(); ++i) {
    cef_buffer[i] = static_cast<uint8_t>(i % 256);
  }

  auto copy_result = manager_.CopyFromCEF(*buffer, cef_buffer.data(), size);
  ASSERT_TRUE(copy_result.IsOk());

  // Verify data was copied correctly
  const uint8_t* buffer_data = buffer->GetData();
  for (int y = 0; y < size.height; ++y) {
    for (int x = 0; x < size.width * 4; ++x) {
      size_t src_idx = y * size.width * 4 + x;
      size_t dest_idx = y * buffer->stride + x;
      EXPECT_EQ(buffer_data[dest_idx], cef_buffer[src_idx])
          << "Mismatch at (" << x << ", " << y << ")";
    }
  }
}

TEST_F(BufferManagerTest, CopyFromCEFNullSource) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  auto copy_result = manager_.CopyFromCEF(*buffer, nullptr, size);
  EXPECT_TRUE(copy_result.IsError());
  EXPECT_NE(copy_result.GetError().Message().find("null"), std::string::npos);
}

TEST_F(BufferManagerTest, CopyFromCEFInvalidBuffer) {
  Size size{0, 0};
  BufferManager::Buffer invalid_buffer(size);

  std::vector<uint8_t> cef_buffer(100 * 100 * 4);
  auto copy_result = manager_.CopyFromCEF(invalid_buffer, cef_buffer.data(), Size{100, 100});

  EXPECT_TRUE(copy_result.IsError());
  EXPECT_NE(copy_result.GetError().Message().find("invalid"), std::string::npos);
}

TEST_F(BufferManagerTest, CopyFromCEFSizeMismatch) {
  Size buffer_size{100, 100};
  auto result = manager_.AllocateBuffer(buffer_size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  Size src_size{200, 200};
  std::vector<uint8_t> cef_buffer(src_size.width * 4 * src_size.height);

  auto copy_result = manager_.CopyFromCEF(*buffer, cef_buffer.data(), src_size);
  EXPECT_TRUE(copy_result.IsError());
  EXPECT_NE(copy_result.GetError().Message().find("mismatch"), std::string::npos);
}

// ============================================================================
// CopyFromCEFDirty Tests
// ============================================================================

TEST_F(BufferManagerTest, CopyFromCEFDirtySingleRect) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  // Create CEF buffer with pattern
  std::vector<uint8_t> cef_buffer(size.width * 4 * size.height);
  for (size_t i = 0; i < cef_buffer.size(); ++i) {
    cef_buffer[i] = static_cast<uint8_t>((i * 7) % 256);
  }

  // Copy only a small dirty rect
  Rect dirty_rect{10, 10, 20, 20};
  std::vector<Rect> dirty_rects{dirty_rect};

  auto copy_result = manager_.CopyFromCEFDirty(*buffer, cef_buffer.data(), size, dirty_rects);
  ASSERT_TRUE(copy_result.IsOk());

  // Verify only dirty rect was copied (rest should still be zero)
  const uint8_t* buffer_data = buffer->GetData();

  for (int y = 0; y < size.height; ++y) {
    for (int x = 0; x < size.width; ++x) {
      for (int c = 0; c < 4; ++c) {
        size_t dest_idx = y * buffer->stride + x * 4 + c;

        if (dirty_rect.Contains(Point{x, y})) {
          // Inside dirty rect - should match CEF buffer
          size_t src_idx = y * size.width * 4 + x * 4 + c;
          EXPECT_EQ(buffer_data[dest_idx], cef_buffer[src_idx])
              << "Dirty rect pixel mismatch at (" << x << ", " << y << ")";
        } else {
          // Outside dirty rect - should still be zero
          EXPECT_EQ(buffer_data[dest_idx], 0)
              << "Non-dirty pixel changed at (" << x << ", " << y << ")";
        }
      }
    }
  }
}

TEST_F(BufferManagerTest, CopyFromCEFDirtyMultipleRects) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  std::vector<uint8_t> cef_buffer(size.width * 4 * size.height, 255);

  std::vector<Rect> dirty_rects{{0, 0, 10, 10}, {50, 50, 20, 20}, {80, 80, 20, 20}};

  auto copy_result = manager_.CopyFromCEFDirty(*buffer, cef_buffer.data(), size, dirty_rects);
  ASSERT_TRUE(copy_result.IsOk());

  // Count non-zero bytes (should equal dirty rect pixels * 4)
  int expected_dirty_pixels = 0;
  for (const auto& rect : dirty_rects) {
    expected_dirty_pixels += rect.Area();
  }

  const uint8_t* buffer_data = buffer->GetData();
  int non_zero_pixels = 0;

  for (int y = 0; y < size.height; ++y) {
    for (int x = 0; x < size.width; ++x) {
      size_t pixel_idx = y * buffer->stride + x * 4;
      bool is_non_zero = false;
      for (int c = 0; c < 4; ++c) {
        if (buffer_data[pixel_idx + c] != 0) {
          is_non_zero = true;
          break;
        }
      }
      if (is_non_zero) {
        ++non_zero_pixels;
      }
    }
  }

  EXPECT_EQ(non_zero_pixels, expected_dirty_pixels);
}

TEST_F(BufferManagerTest, CopyFromCEFDirtyEmptyRectList) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  std::vector<uint8_t> cef_buffer(size.width * 4 * size.height, 255);
  std::vector<Rect> dirty_rects;  // Empty

  auto copy_result = manager_.CopyFromCEFDirty(*buffer, cef_buffer.data(), size, dirty_rects);
  ASSERT_TRUE(copy_result.IsOk());

  // Should copy everything when dirty rects is empty
  const uint8_t* buffer_data = buffer->GetData();
  for (int y = 0; y < size.height; ++y) {
    for (int x = 0; x < size.width * 4; ++x) {
      size_t idx = y * buffer->stride + x;
      EXPECT_EQ(buffer_data[idx], 255);
    }
  }
}

TEST_F(BufferManagerTest, CopyFromCEFDirtyInvalidRect) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  std::vector<uint8_t> cef_buffer(size.width * 4 * size.height, 255);

  // Rect outside bounds
  std::vector<Rect> dirty_rects{{200, 200, 10, 10}};

  auto copy_result = manager_.CopyFromCEFDirty(*buffer, cef_buffer.data(), size, dirty_rects);
  ASSERT_TRUE(copy_result.IsOk());

  // Invalid rect should be skipped - buffer should still be zero
  const uint8_t* buffer_data = buffer->GetData();
  for (size_t i = 0; i < buffer->GetSizeInBytes(); ++i) {
    EXPECT_EQ(buffer_data[i], 0);
  }
}

TEST_F(BufferManagerTest, CopyFromCEFDirtyPartiallyOutOfBounds) {
  Size size{100, 100};
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  std::vector<uint8_t> cef_buffer(size.width * 4 * size.height, 255);

  // Rect partially out of bounds
  std::vector<Rect> dirty_rects{{90, 90, 20, 20}};

  auto copy_result = manager_.CopyFromCEFDirty(*buffer, cef_buffer.data(), size, dirty_rects);
  ASSERT_TRUE(copy_result.IsOk());

  // Should skip the invalid rect
  const uint8_t* buffer_data = buffer->GetData();
  for (size_t i = 0; i < buffer->GetSizeInBytes(); ++i) {
    EXPECT_EQ(buffer_data[i], 0);
  }
}

// ============================================================================
// Stride Calculation Tests
// ============================================================================

TEST_F(BufferManagerTest, StrideCalculationAligned) {
  Size size{100, 100};  // 100 * 4 = 400 bytes (already 4-byte aligned)
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  EXPECT_EQ(buffer->stride, 400);
  EXPECT_EQ(buffer->stride % 4, 0);
}

TEST_F(BufferManagerTest, StrideCalculationSmallWidth) {
  Size size{1, 1};  // 1 * 4 = 4 bytes (4-byte aligned)
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  EXPECT_EQ(buffer->stride, 4);
  EXPECT_EQ(buffer->stride % 4, 0);
}

TEST_F(BufferManagerTest, StrideCalculationOddWidth) {
  Size size{123, 100};  // 123 * 4 = 492 bytes (4-byte aligned)
  auto result = manager_.AllocateBuffer(size);
  ASSERT_TRUE(result.IsOk());
  auto buffer = std::move(result).Value();

  EXPECT_EQ(buffer->stride, 492);
  EXPECT_EQ(buffer->stride % 4, 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BufferManagerTest, MultipleAllocations) {
  std::vector<std::unique_ptr<BufferManager::Buffer>> buffers;

  for (int i = 0; i < 10; ++i) {
    Size size{100 * (i + 1), 100 * (i + 1)};
    auto result = manager_.AllocateBuffer(size);
    ASSERT_TRUE(result.IsOk());
    buffers.push_back(std::move(result).Value());
  }

  // All buffers should be valid
  for (const auto& buffer : buffers) {
    EXPECT_TRUE(buffer->IsValid());
  }
}

TEST_F(BufferManagerTest, BufferOwnershipTransfer) {
  auto result = manager_.AllocateBuffer(Size{100, 100});
  ASSERT_TRUE(result.IsOk());

  auto buffer1 = std::move(result).Value();
  EXPECT_TRUE(buffer1->IsValid());

  auto buffer2 = std::move(buffer1);
  EXPECT_TRUE(buffer2->IsValid());
}
