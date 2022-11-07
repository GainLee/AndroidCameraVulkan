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

#include "VulkanBufferWrapper.h"

namespace vks {
std::unique_ptr<Buffer>
Buffer::create(const std::shared_ptr<vks::VulkanDeviceWrapper> context,
               uint32_t size, vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags properties) {
  auto buffer = std::make_unique<Buffer>(context, size);
  const bool success = buffer->initialize(usage, properties);
  return success ? std::move(buffer) : nullptr;
}

Buffer::Buffer(const std::shared_ptr<vks::VulkanDeviceWrapper> context,
               uint32_t size)
    : mContext(context), mSize(size) {}

bool Buffer::initialize(vk::BufferUsageFlags usage,
                        vk::MemoryPropertyFlags properties) {
  // Create buffer
  vk::BufferCreateInfo bufferCreateInfo{};
  bufferCreateInfo.size = vk::DeviceSize{mSize};
  bufferCreateInfo.usage = usage;
  bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
  CALL_VK(mContext->logicalDevice.createBuffer(&bufferCreateInfo, nullptr,
                                               &mBuffer));

  // Allocate memory for the buffer
  vk::MemoryRequirements memoryRequirements;
  mContext->logicalDevice.getBufferMemoryRequirements(mBuffer,
                                                      &memoryRequirements);
  const auto memoryTypeIndex =
      mContext->getMemoryType(memoryRequirements.memoryTypeBits, properties);
  const vk::MemoryAllocateInfo allocateInfo{memoryRequirements.size,
                                            memoryTypeIndex};
  CALL_VK(
      mContext->logicalDevice.allocateMemory(&allocateInfo, nullptr, &mMemory));

  mContext->logicalDevice.bindBufferMemory(mBuffer, mMemory, 0);

  vks::debug::setDeviceMemoryName(mContext->logicalDevice, mMemory,
                                  "VulkanResources-Buffer::initialize-mMemory");

  return true;
}

/**
 * Copies the specified data to the mapped buffer
 *
 * @param data Pointer to the data to copy
 * @param size Size of the data to copy in machine units
 *
 */
void Buffer::copyFrom(const void *data, vk::DeviceSize size) {
  assert(mapped);
  memcpy(mapped, data, size);
}

/**
 * Map a memory range of this buffer. If successful, mapped points to the
 * specified buffer range.
 *
 * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to
 * map the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the buffer mapping call
 */
vk::Result Buffer::map(vk::DeviceSize size, vk::DeviceSize offset) {
  return mContext->logicalDevice.mapMemory(mMemory, offset, size,
                                           vk::MemoryMapFlagBits{0}, &mapped);
}

/**
 * Unmap a mapped memory range
 *
 * @note Does not return a result as vkUnmapMemory can't fail
 */
void Buffer::unmap() {
  if (mapped) {
    mContext->logicalDevice.unmapMemory(mMemory);
    mapped = nullptr;
  }
}

/**
 * Flush a memory range of the buffer to make it visible to the device
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE
 * to flush the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the flush call
 */
vk::Result Buffer::flush(vk::DeviceSize size, vk::DeviceSize offset) {
  vk::MappedMemoryRange mappedRange = {};
  mappedRange.memory = mMemory;
  mappedRange.offset = offset;
  mappedRange.size = size;
  return mContext->logicalDevice.flushMappedMemoryRanges(1, &mappedRange);
}

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to invalidate. Pass
 * VK_WHOLE_SIZE to invalidate the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the invalidate call
 */
vk::Result Buffer::invalidate(vk::DeviceSize size, vk::DeviceSize offset) {
  vk::MappedMemoryRange mappedRange = {};
  mappedRange.memory = mMemory;
  mappedRange.offset = offset;
  mappedRange.size = size;
  return mContext->logicalDevice.invalidateMappedMemoryRanges(1, &mappedRange);
}
} // namespace vks