#include "rendering/buffer_manager.h"

#include <algorithm>
#include <cstring>

namespace athena {
namespace rendering {

// Buffer implementation
BufferManager::Buffer::Buffer(const core::Size& size)
    : physical_size(size), stride(BufferManager::CalculateStride(size.width)) {
  if (size.IsEmpty()) {
    data = nullptr;
    return;
  }

  size_t buffer_size = stride * size.height;
  data = std::make_unique<uint8_t[]>(buffer_size);

  // Initialize buffer to transparent black (BGRA format)
  if (data) {
    std::memset(data.get(), 0, buffer_size);
  }
}

// BufferManager implementation
int BufferManager::CalculateStride(int width) {
  // Each pixel is 4 bytes (BGRA), and we want 4-byte alignment
  int stride = width * 4;

  // Ensure stride is 4-byte aligned (should already be, but be explicit)
  int remainder = stride % 4;
  if (remainder != 0) {
    stride += (4 - remainder);
  }

  return stride;
}

bool BufferManager::IsValidSize(const core::Size& size) {
  // Check for valid dimensions
  if (size.width <= 0 || size.height <= 0) {
    return false;
  }

  // Check for reasonable size limits (prevent excessive allocations)
  // Max 8K resolution: 7680 x 4320
  const int MAX_WIDTH = 8192;
  const int MAX_HEIGHT = 8192;

  if (size.width > MAX_WIDTH || size.height > MAX_HEIGHT) {
    return false;
  }

  // Check that total size doesn't overflow
  int64_t total_bytes =
      static_cast<int64_t>(CalculateStride(size.width)) * static_cast<int64_t>(size.height);

  // Limit to 256 MB per buffer
  const int64_t MAX_BUFFER_SIZE = 256 * 1024 * 1024;
  if (total_bytes > MAX_BUFFER_SIZE) {
    return false;
  }

  return true;
}

utils::Result<std::unique_ptr<BufferManager::Buffer>> BufferManager::AllocateBuffer(
    const core::Size& physical_size) {
  // Validate size
  if (!IsValidSize(physical_size)) {
    return utils::Error("Invalid buffer size: " + physical_size.ToString());
  }

  try {
    auto buffer = std::make_unique<Buffer>(physical_size);

    if (!buffer || !buffer->IsValid()) {
      return utils::Error("Failed to allocate buffer: out of memory");
    }

    return utils::Ok(std::move(buffer));
  } catch (const std::bad_alloc&) {
    return utils::Error("Failed to allocate buffer: out of memory");
  } catch (const std::exception& e) {
    return utils::Error(std::string("Failed to allocate buffer: ") + e.what());
  }
}

utils::Result<void> BufferManager::CopyFromCEF(Buffer& dest,
                                               const void* src,
                                               const core::Size& size) {
  // Validate inputs
  if (!src) {
    return utils::Error("Source buffer is null");
  }

  if (!dest.IsValid()) {
    return utils::Error("Destination buffer is invalid");
  }

  if (dest.physical_size != size) {
    return utils::Error("Size mismatch: dest=" + dest.physical_size.ToString() +
                        ", src=" + size.ToString());
  }

  // CEF provides buffers with width*4 stride (BGRA format)
  int src_stride = size.width * 4;

  try {
    // Copy row by row (handles different strides if needed)
    const uint8_t* src_ptr = static_cast<const uint8_t*>(src);
    uint8_t* dest_ptr = dest.GetData();

    int copy_bytes = std::min(src_stride, dest.stride);

    for (int y = 0; y < size.height; ++y) {
      std::memcpy(dest_ptr + y * dest.stride, src_ptr + y * src_stride, copy_bytes);
    }

    return utils::Ok();
  } catch (const std::exception& e) {
    return utils::Error(std::string("Failed to copy buffer: ") + e.what());
  }
}

utils::Result<void> BufferManager::CopyFromCEFDirty(Buffer& dest,
                                                    const void* src,
                                                    const core::Size& size,
                                                    const std::vector<core::Rect>& dirty_rects) {
  // Validate inputs
  if (!src) {
    return utils::Error("Source buffer is null");
  }

  if (!dest.IsValid()) {
    return utils::Error("Destination buffer is invalid");
  }

  if (dest.physical_size != size) {
    return utils::Error("Size mismatch: dest=" + dest.physical_size.ToString() +
                        ", src=" + size.ToString());
  }

  // If no dirty rects, copy everything
  if (dirty_rects.empty()) {
    return CopyFromCEF(dest, src, size);
  }

  // CEF provides buffers with width*4 stride (BGRA format)
  int src_stride = size.width * 4;
  const uint8_t* src_ptr = static_cast<const uint8_t*>(src);
  uint8_t* dest_ptr = dest.GetData();

  try {
    // Copy only dirty rectangles
    for (const auto& rect : dirty_rects) {
      // Validate rect is within bounds
      if (rect.x < 0 || rect.y < 0 || rect.x + rect.width > size.width ||
          rect.y + rect.height > size.height) {
        continue;  // Skip invalid rects
      }

      if (rect.IsEmpty()) {
        continue;
      }

      // Copy this dirty rect row by row
      int copy_bytes = rect.width * 4;

      for (int y = 0; y < rect.height; ++y) {
        int src_offset = (rect.y + y) * src_stride + rect.x * 4;
        int dest_offset = (rect.y + y) * dest.stride + rect.x * 4;

        std::memcpy(dest_ptr + dest_offset, src_ptr + src_offset, copy_bytes);
      }
    }

    return utils::Ok();
  } catch (const std::exception& e) {
    return utils::Error(std::string("Failed to copy dirty rects: ") + e.what());
  }
}

}  // namespace rendering
}  // namespace athena
