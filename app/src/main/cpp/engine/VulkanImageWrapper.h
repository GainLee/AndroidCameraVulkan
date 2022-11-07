/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 by Gain
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef GAINVULKANSAMPLE_VULKANRESOURCES_H
#define GAINVULKANSAMPLE_VULKANRESOURCES_H

#include <android/bitmap.h>
#include <android/hardware_buffer_jni.h>
#include <jni.h>

#include <android/asset_manager.h>
#include <memory>
#include <optional>
#include <vector>

#include "VulkanBufferWrapper.h"
#include "VulkanDeviceWrapper.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace gain {
class Image {
public:
  struct ImageBasicInfo {
    vk::ImageType imageType = vk::ImageType::e2D;
    vk::Format format = vk::Format::eR8G8B8A8Unorm;
    uint32_t mipLevels = 1;
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::Extent3D extent;
    uint32_t arrayLayers = 1;
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    vk::Bool32 unnormalizedCoordinates =
        VK_FALSE; //不归一化坐标。使用在渲染管线用归一化，计算管线不归一化
  };

  // Create a image backed by device local memory.
  static std::unique_ptr<Image> createDeviceLocal(
      const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper,
      vk::Queue queue, ImageBasicInfo &imageInfo);

  // Create a image backed by the given AHardwareBuffer. The image will keep a
  // reference to the AHardwareBuffer so that callers can safely close buffer.
  static std::unique_ptr<Image> createFromAHardwareBuffer(
      const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper,
      vk::Queue queue, AHardwareBuffer *buffer, ImageBasicInfo &imageInfo);

  Image(const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper,
        vk::Queue queue, const ImageBasicInfo &imageInfo);

  ~Image() {
    if (mImage) {
      mDeviceWrapper->logicalDevice.destroyImage(mImage, nullptr);
    }

    if (mMemory) {
      mDeviceWrapper->logicalDevice.freeMemory(mMemory, nullptr);
    }

    if (mSampler) {
      mDeviceWrapper->logicalDevice.destroySampler(mSampler, nullptr);
    }

    if (mImageView) {
      mDeviceWrapper->logicalDevice.destroyImageView(mImageView, nullptr);
    }

    if (mHardwareBufferInfo.mBuffer != nullptr) {
      AHardwareBuffer_release(mHardwareBufferInfo.mBuffer);
    }

    if (mYMemory) {
      mDeviceWrapper->logicalDevice.freeMemory(mYMemory, nullptr);
    }
    if (mUMemory) {
      mDeviceWrapper->logicalDevice.freeMemory(mUMemory, nullptr);
    }
    if (mVMemory) {
      mDeviceWrapper->logicalDevice.freeMemory(mVMemory, nullptr);
    }
    if (mSamplerYcbcrConversion) {
      mDeviceWrapper->logicalDevice.destroySamplerYcbcrConversion(mSamplerYcbcrConversion, nullptr);
    }
  }

  uint32_t width() const { return mImageInfo.extent.width; }

  uint32_t height() const { return mImageInfo.extent.height; }

  vk::Image getImageHandle() const { return mImage; }

  vk::ImageView getImageViewHandle() const { return mImageView; }

  vk::Sampler getSamplerHandle() const { return mSampler; }

  AHardwareBuffer *getAHardwareBuffer() { return mHardwareBufferInfo.mBuffer; }

  bool setContentFromHardwareBuffer(AHardwareBuffer *buffer);

  // Put an image memory barrier for setting an image layout on the sub resource
  // into the given command buffer
  void
  setImageLayout(vk::CommandBuffer cmdbuffer,
                 vk::ImageSubresourceRange subresourceRange,
                 vk::ImageLayout newImageLayout, bool ignoreOldLayout = true,
                 vk::ImageLayout oldImageLayout = vk::ImageLayout::eUndefined,
                 vk::PipelineStageFlags srcStageMask =
                     vk::PipelineStageFlagBits::eAllCommands,
                 vk::PipelineStageFlags dstStageMask =
                     vk::PipelineStageFlagBits::eAllCommands);

  // Uses a fixed sub resource layout with first mip level and layer
  void
  setImageLayout(vk::CommandBuffer cmdbuffer, vk::ImageAspectFlags aspectMask,
                 vk::ImageLayout newImageLayout, bool ignoreOldLayout = true,
                 vk::ImageLayout oldImageLayout = vk::ImageLayout::eUndefined,
                 vk::PipelineStageFlags srcStageMask =
                     vk::PipelineStageFlagBits::eAllCommands,
                 vk::PipelineStageFlags dstStageMask =
                     vk::PipelineStageFlagBits::eAllCommands);

  vk::DescriptorImageInfo getDescriptor() const {
    return {mSampler, mImageView, mImageInfo.layout};
  }

private:
  bool createDeviceLocalImage();

  bool createSamplerYcbcrConversionFromAHardwareBuffer(AHardwareBuffer *buffer);

  bool createSampler();

  bool createImageView();

  bool createSamplerYcbcrConversionInfo();

  bool isYUVFormat();

  const std::shared_ptr<vks::VulkanDeviceWrapper> mDeviceWrapper;

  vk::Queue mVkQueue;

  ImageBasicInfo mImageInfo;

  // The managed HardwareBuffer info. Only valid if the image is created from
  // Image::createFromAHardwareBuffer.
  struct HardwareBufferInfo {
    AHardwareBuffer *mBuffer = nullptr;
    vk::DeviceSize allocationSize;
    uint32_t memoryTypeIndex;
  } mHardwareBufferInfo;

  // Managed handles
  vk::Image mImage = nullptr;
  vk::DeviceMemory mMemory = nullptr;
  vk::Sampler mSampler = nullptr;
  vk::ImageView mImageView = nullptr;

  // DeviceMemory for YUV
  vk::DeviceMemory mYMemory = nullptr;
  vk::DeviceMemory mUMemory = nullptr;
  vk::DeviceMemory mVMemory = nullptr;

  vk::SamplerYcbcrConversionKHR mSamplerYcbcrConversion = nullptr;
  vk::SamplerYcbcrConversionInfo mSamplerYcbcrConversionInfo;
};

} // namespace gain

#endif // GAINVULKANSAMPLE_VULKANRESOURCES_H
