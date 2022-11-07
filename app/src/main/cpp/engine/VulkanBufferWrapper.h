/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Gain
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef GAINVULKANSAMPLE_VULKANBUFFERWRAPPER_H
#define GAINVULKANSAMPLE_VULKANBUFFERWRAPPER_H

#include <memory>
#include <optional>
#include <vector>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include "VulkanDeviceWrapper.hpp"

namespace vks {
class Buffer {
public:
  // Create a buffer and allocate the memory.
  static std::unique_ptr<Buffer>
  create(const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper,
         uint32_t size, vk::BufferUsageFlags usage,
         vk::MemoryPropertyFlags properties);

  // Prefer Buffer::create
  Buffer(const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper,
         uint32_t size);

  ~Buffer() {
    if (mBuffer) {
      mContext->logicalDevice.destroyBuffer(mBuffer, nullptr);
    }

    if (mMemory) {
      mContext->logicalDevice.freeMemory(mMemory, nullptr);
    }
  }

  vk::Buffer getBufferHandle() const { return mBuffer; }

  vk::DeviceMemory getMemoryHandle() const { return mMemory; }

  vk::DescriptorBufferInfo getDescriptor() const { return {mBuffer, 0, mSize}; }

  vk::Result flush(vk::DeviceSize size = VK_WHOLE_SIZE,
                   vk::DeviceSize offset = 0);

  vk::Result map(vk::DeviceSize size = VK_WHOLE_SIZE,
                 vk::DeviceSize offset = 0);

  void unmap();

  void copyFrom(const void *data, vk::DeviceSize size);

  vk::Result invalidate(vk::DeviceSize size = VK_WHOLE_SIZE,
                        vk::DeviceSize offset = 0);

private:
  bool initialize(vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties);

  const std::shared_ptr<vks::VulkanDeviceWrapper> mContext;
  vk::Queue mVkQueue;
  uint32_t mSize;

  // Managed handles
  vk::Buffer mBuffer = nullptr;
  vk::DeviceMemory mMemory = nullptr;
  void *mapped = nullptr;
  /** @brief Usage flags to be filled by external source at buffer creation (to
   * query at some later point) */
  vk::BufferUsageFlags usageFlags;
  /** @brief Memory property flags to be filled by external source at buffer
   * creation (to query at some later point) */
  vk::MemoryPropertyFlags memoryPropertyFlags;
};
} // namespace vks

#endif // GAINVULKANSAMPLE_VULKANBUFFERWRAPPER_H
