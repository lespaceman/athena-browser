#ifndef ATHENA_RENDERING_BUFFER_MANAGER_H_
#define ATHENA_RENDERING_BUFFER_MANAGER_H_

#include "core/types.h"
#include "utils/error.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace athena {
namespace rendering {

// BufferManager handles memory management for pixel buffers using RAII principles.
// This eliminates manual memory management and ensures exception-safe buffer handling.
class BufferManager {
 public:
  // Buffer represents a pixel buffer with automatic memory management (RAII)
  struct Buffer {
    std::unique_ptr<uint8_t[]> data;  // Automatic cleanup, no manual delete needed
    core::Size physical_size;         // Size in physical pixels
    int stride;                       // Bytes per row (may be padded)

    // Construct a buffer with the given size
    // Stride is calculated to be 4-byte aligned (width * 4 bytes per pixel)
    explicit Buffer(const core::Size& size);

    // Move-only (no copying large buffers)
    Buffer(Buffer&&) = default;
    Buffer& operator=(Buffer&&) = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Destructor is automatic - std::unique_ptr handles cleanup

    // Get raw pointer to buffer data
    uint8_t* GetData() { return data.get(); }
    const uint8_t* GetData() const { return data.get(); }

    // Get buffer size in bytes
    size_t GetSizeInBytes() const { return stride * physical_size.height; }

    // Check if buffer is valid
    bool IsValid() const { return data != nullptr && !physical_size.IsEmpty(); }
  };

  BufferManager() = default;
  ~BufferManager() = default;

  // Move-only
  BufferManager(BufferManager&&) = default;
  BufferManager& operator=(BufferManager&&) = default;
  BufferManager(const BufferManager&) = delete;
  BufferManager& operator=(const BufferManager&) = delete;

  // Allocate a new buffer with the given physical size
  // Returns Error if size is invalid or allocation fails
  utils::Result<std::unique_ptr<Buffer>> AllocateBuffer(const core::Size& physical_size);

  // Copy data from CEF buffer to our buffer
  // src: Source buffer (from CEF OnPaint)
  // dest: Destination buffer (must be already allocated)
  // size: Size in pixels (not bytes)
  // Returns Error if copy fails or sizes don't match
  utils::Result<void> CopyFromCEF(Buffer& dest, const void* src, const core::Size& size);

  // Copy data from CEF buffer with dirty rects optimization
  // Only copies the specified dirty rectangles instead of the entire buffer
  utils::Result<void> CopyFromCEFDirty(Buffer& dest,
                                       const void* src,
                                       const core::Size& size,
                                       const std::vector<core::Rect>& dirty_rects);

 private:
  // Calculate stride (bytes per row) with 4-byte alignment
  static int CalculateStride(int width);

  // Validate that a size is valid for buffer allocation
  static bool IsValidSize(const core::Size& size);
};

}  // namespace rendering
}  // namespace athena

#endif  // ATHENA_RENDERING_BUFFER_MANAGER_H_
