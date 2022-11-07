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

#include "VulkanImageWrapper.h"

#include <android/bitmap.h>
#include <android/hardware_buffer_jni.h>
#include <jni.h>

#include <LogUtil.h>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace gain {
std::unique_ptr<Image> Image::createDeviceLocal(
    const std::shared_ptr<vks::VulkanDeviceWrapper> context, vk::Queue queue,
    ImageBasicInfo &imageInfo) {
  auto image = std::make_unique<Image>(context, queue, imageInfo);
  bool success = image->createDeviceLocalImage();
  if (image->isYUVFormat()) {
    success = success && image->createSamplerYcbcrConversionInfo();
  }
  success = success && image->createImageView();
  // Sampler is only needed for sampled images.
  if (imageInfo.usage & vk::ImageUsageFlagBits::eSampled) {
    success = success && image->createSampler();
  }
  return success ? std::move(image) : nullptr;
}

std::unique_ptr<Image> Image::createFromAHardwareBuffer(
    const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper,
    vk::Queue queue, AHardwareBuffer *buffer, ImageBasicInfo &imageInfo) {
  auto image = std::make_unique<Image>(deviceWrapper, queue, imageInfo);
  bool success = image->createSamplerYcbcrConversionFromAHardwareBuffer(buffer);
  success = success && image->setContentFromHardwareBuffer(buffer);
  return success ? std::move(image) : nullptr;
}

bool Image::createDeviceLocalImage() {
  // Create an image
  vk::ImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.imageType = mImageInfo.imageType;
  imageCreateInfo.format = mImageInfo.format;
  imageCreateInfo.extent = mImageInfo.extent;
  imageCreateInfo.mipLevels = mImageInfo.mipLevels;
  imageCreateInfo.arrayLayers = mImageInfo.arrayLayers;
  imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
  imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
  imageCreateInfo.usage = mImageInfo.usage;
  imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
  imageCreateInfo.queueFamilyIndexCount = 0;
  imageCreateInfo.pQueueFamilyIndices = nullptr;
  imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;

  if (mImageInfo.arrayLayers == 6) {
    // This flag is required for cube map images
    imageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
  }
  if (isYUVFormat()) {
    imageCreateInfo.flags = vk::ImageCreateFlagBits::eDisjoint;
  }

  CALL_VK(mDeviceWrapper->logicalDevice.createImage(&imageCreateInfo, nullptr,
                                                    &mImage));

  // Allocate device memory
  if (mImageInfo.format == vk::Format::eG8B8R83Plane420Unorm) {
    vk::ImagePlaneMemoryRequirementsInfo imagePlaneMemoryRequirementsInfo = {};

    vk::ImageMemoryRequirementsInfo2 imageMemoryRequirementsInfo2 = {};
    imageMemoryRequirementsInfo2.pNext = &imagePlaneMemoryRequirementsInfo;
    imageMemoryRequirementsInfo2.image = mImage;

    // Get memory requirement for each plane
    imagePlaneMemoryRequirementsInfo.planeAspect =
        vk::ImageAspectFlagBits::ePlane0;
    vk::MemoryRequirements2 memoryRequirements2Plane0 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    mDeviceWrapper->logicalDevice.getImageMemoryRequirements2(
        &imageMemoryRequirementsInfo2, &memoryRequirements2Plane0);

    imagePlaneMemoryRequirementsInfo.planeAspect =
        vk::ImageAspectFlagBits::ePlane1;
    vk::MemoryRequirements2 memoryRequirements2Plane1 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    mDeviceWrapper->logicalDevice.getImageMemoryRequirements2(
        &imageMemoryRequirementsInfo2, &memoryRequirements2Plane1);

    imagePlaneMemoryRequirementsInfo.planeAspect =
        vk::ImageAspectFlagBits::ePlane2;
    vk::MemoryRequirements2 memoryRequirements2Plane2 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    mDeviceWrapper->logicalDevice.getImageMemoryRequirements2(
        &imageMemoryRequirementsInfo2, &memoryRequirements2Plane2);

    // Allocate memory for each plane
    vk::MemoryAllocateInfo memoryAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};

    memoryAllocateInfo.allocationSize =
        memoryRequirements2Plane0.memoryRequirements.size;
    mDeviceWrapper->logicalDevice.allocateMemory(&memoryAllocateInfo, nullptr,
                                                 &mYMemory);

    memoryAllocateInfo.allocationSize =
        memoryRequirements2Plane1.memoryRequirements.size;
    mDeviceWrapper->logicalDevice.allocateMemory(&memoryAllocateInfo, nullptr,
                                                 &mUMemory);

    memoryAllocateInfo.allocationSize =
        memoryRequirements2Plane2.memoryRequirements.size;
    mDeviceWrapper->logicalDevice.allocateMemory(&memoryAllocateInfo, nullptr,
                                                 &mVMemory);

    // Bind memory for each plane
    vk::BindImagePlaneMemoryInfo bindImagePlaneMemoryInfoY{};
    bindImagePlaneMemoryInfoY.planeAspect = vk::ImageAspectFlagBits::ePlane0;
    vk::BindImageMemoryInfo bindImageMemoryInfoY{};
    bindImageMemoryInfoY.pNext = &bindImagePlaneMemoryInfoY;
    bindImageMemoryInfoY.image = mImage;
    bindImageMemoryInfoY.memory = mYMemory;

    vk::BindImagePlaneMemoryInfo bindImagePlaneMemoryInfoU{};
    bindImagePlaneMemoryInfoU.planeAspect = vk::ImageAspectFlagBits::ePlane1;
    vk::BindImageMemoryInfo bindImageMemoryInfoU{};
    bindImageMemoryInfoU.pNext = &bindImagePlaneMemoryInfoU;
    bindImageMemoryInfoU.image = mImage;
    bindImageMemoryInfoU.memory = mUMemory;

    vk::BindImagePlaneMemoryInfo bindImagePlaneMemoryInfoV{};
    bindImagePlaneMemoryInfoV.planeAspect = vk::ImageAspectFlagBits::ePlane2;
    vk::BindImageMemoryInfo bindImageMemoryInfoV{};
    bindImageMemoryInfoV.pNext = &bindImagePlaneMemoryInfoV;
    bindImageMemoryInfoV.image = mImage;
    bindImageMemoryInfoV.memory = mVMemory;

    std::vector<vk::BindImageMemoryInfo> bindImageMemoryInfos = {
        bindImageMemoryInfoY, bindImageMemoryInfoU, bindImageMemoryInfoV};

    mDeviceWrapper->logicalDevice.bindImageMemory2(
        static_cast<uint32_t>(bindImageMemoryInfos.size()),
        bindImageMemoryInfos.data());
  } else {
    vk::MemoryRequirements memoryRequirements;

    mDeviceWrapper->logicalDevice.getImageMemoryRequirements(
        mImage, &memoryRequirements);
    uint32_t memoryTypeIndex =
        mDeviceWrapper->getMemoryType(memoryRequirements.memoryTypeBits,
                                      vk::MemoryPropertyFlagBits::eDeviceLocal);
    const vk::MemoryAllocateInfo allocateInfo = {memoryRequirements.size,
                                                 memoryTypeIndex};
    CALL_VK(mDeviceWrapper->logicalDevice.allocateMemory(&allocateInfo, nullptr,
                                                         &mMemory));
    mDeviceWrapper->logicalDevice.bindImageMemory(mImage, mMemory, 0);
  }

  // Set layout to mImageInfo.layout
  vk::CommandBuffer copyCommand;
  assert(mDeviceWrapper->beginSingleTimeCommand(&copyCommand));
  vk::ImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = mImageInfo.mipLevels;
  subresourceRange.layerCount = mImageInfo.arrayLayers;
  setImageLayout(copyCommand, subresourceRange, mImageInfo.layout, false,
                 vk::ImageLayout::eUndefined);
  mDeviceWrapper->endAndSubmitSingleTimeCommand(copyCommand, mVkQueue);

  return true;
}

bool Image::createSamplerYcbcrConversionFromAHardwareBuffer(
    AHardwareBuffer *buffer) {
  // Acquire the AHardwareBuffer and get the descriptor
  //	AHardwareBuffer_acquire(buffer);
  AHardwareBuffer_Desc ahwbDesc{};
  AHardwareBuffer_describe(buffer, &ahwbDesc);

  mHardwareBufferInfo.mBuffer = buffer;
  mImageInfo.extent = vk::Extent3D{ahwbDesc.width, ahwbDesc.height, 1};

  // Get AHardwareBuffer properties
  vk::AndroidHardwareBufferFormatPropertiesANDROID formatInfo{};
  vk::AndroidHardwareBufferPropertiesANDROID properties{};
  properties.pNext = &formatInfo;
  CALL_VK(
      mDeviceWrapper->logicalDevice.getAndroidHardwareBufferPropertiesANDROID(
          mHardwareBufferInfo.mBuffer, &properties));

  // Create an image to bind to our AHardwareBuffer
  vk::ExternalMemoryImageCreateInfo externalCreateInfo{};
  externalCreateInfo.handleTypes =
      vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;

  vk::ExternalFormatANDROID externalFormatAndroid{};

  // Create sampler
  vk::SamplerYcbcrConversionCreateInfo conv_info = {};

  if (formatInfo.format == vk::Format::eUndefined) {
    externalFormatAndroid.externalFormat = formatInfo.externalFormat;
    conv_info.pNext = &externalFormatAndroid;
    conv_info.format = vk::Format::eUndefined;
    conv_info.ycbcrModel = formatInfo.suggestedYcbcrModel;
  } else {
    conv_info.pNext = &externalFormatAndroid;
    conv_info.format = formatInfo.format;
    conv_info.ycbcrModel = vk::SamplerYcbcrModelConversion::eYcbcr601;
  }

  conv_info.ycbcrRange = formatInfo.suggestedYcbcrRange;
  conv_info.components = formatInfo.samplerYcbcrConversionComponents;
  conv_info.xChromaOffset = formatInfo.suggestedXChromaOffset;
  conv_info.yChromaOffset = formatInfo.suggestedYChromaOffset;
  conv_info.chromaFilter = vk::Filter::eNearest;
  conv_info.forceExplicitReconstruction = false;

  mSamplerYcbcrConversion =
      mDeviceWrapper->logicalDevice.createSamplerYcbcrConversionKHR(conv_info);

  vk::SamplerYcbcrConversionInfo conv_sampler_info = {};
  conv_sampler_info.conversion = mSamplerYcbcrConversion;

  vk::SamplerCreateInfo sampler_info = {};
  sampler_info.pNext = &conv_sampler_info;
  sampler_info.magFilter = vk::Filter::eNearest;
  sampler_info.minFilter = vk::Filter::eNearest;
  sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
  sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
  sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
  sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
  sampler_info.mipLodBias = 0.0f;
  sampler_info.anisotropyEnable = false;
  sampler_info.maxAnisotropy = 1.0f;
  sampler_info.compareEnable = false;
  sampler_info.compareOp = vk::CompareOp::eNever;
  sampler_info.minLod = 0.0f;
  sampler_info.maxLod = 0.0f;
  sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
  sampler_info.unnormalizedCoordinates = false;

  CALL_VK(mDeviceWrapper->logicalDevice.createSampler(&sampler_info, nullptr,
                                                      &mSampler));
  return true;
}

bool Image::setContentFromHardwareBuffer(AHardwareBuffer *buffer) {
  // Acquire the AHardwareBuffer and get the descriptor
  //	AHardwareBuffer_acquire(buffer);
  AHardwareBuffer_Desc ahwbDesc{};
  AHardwareBuffer_describe(buffer, &ahwbDesc);

  mHardwareBufferInfo.mBuffer = buffer;
  mImageInfo.extent = vk::Extent3D{ahwbDesc.width, ahwbDesc.height, 1};

  // Get AHardwareBuffer properties
  vk::AndroidHardwareBufferFormatPropertiesANDROID formatInfo{};
  vk::AndroidHardwareBufferPropertiesANDROID properties{};
  properties.pNext = &formatInfo;
  CALL_VK(
      mDeviceWrapper->logicalDevice.getAndroidHardwareBufferPropertiesANDROID(
          mHardwareBufferInfo.mBuffer, &properties));

  // Create an image to bind to our AHardwareBuffer
  vk::ExternalMemoryImageCreateInfo externalCreateInfo{};
  externalCreateInfo.handleTypes =
      vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;

  vk::ExternalFormatANDROID externalFormatAndroid{};
  externalFormatAndroid.pNext = &externalCreateInfo;

  vk::ImageCreateInfo createInfo{};
  createInfo.imageType = vk::ImageType::e2D;
  createInfo.extent = vk::Extent3D{ahwbDesc.width, ahwbDesc.height, 1u};
  createInfo.mipLevels = 1u;
  createInfo.arrayLayers = ahwbDesc.layers;
  createInfo.samples = vk::SampleCountFlagBits::e1;
  createInfo.tiling = vk::ImageTiling::eOptimal;
  createInfo.usage = vk::ImageUsageFlagBits::eSampled;
  createInfo.sharingMode = vk::SharingMode::eExclusive;
  createInfo.queueFamilyIndexCount = 0;
  createInfo.pQueueFamilyIndices = nullptr;
  createInfo.initialLayout = vk::ImageLayout::eUndefined;

  if (formatInfo.format == vk::Format::eUndefined) {
    externalFormatAndroid.externalFormat = formatInfo.externalFormat;
    createInfo.pNext = &externalFormatAndroid;
    createInfo.format = vk::Format::eUndefined;
  } else {
    createInfo.pNext = &externalFormatAndroid;
    createInfo.format = formatInfo.format;
  }

  if (mImage) {
    mDeviceWrapper->logicalDevice.destroyImage(mImage);
    mImage = nullptr;
  }

  CALL_VK(
      mDeviceWrapper->logicalDevice.createImage(&createInfo, nullptr, &mImage));

  mHardwareBufferInfo.memoryTypeIndex = mDeviceWrapper->getMemoryType(
      properties.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
      /*external*/ true);
  mHardwareBufferInfo.allocationSize = properties.allocationSize;

  // Allocate device memory
  vk::ImportAndroidHardwareBufferInfoANDROID androidHardwareBufferInfo{};
  androidHardwareBufferInfo.buffer = mHardwareBufferInfo.mBuffer;

  vk::MemoryDedicatedAllocateInfo memoryAllocateInfo{};
  memoryAllocateInfo.pNext = &androidHardwareBufferInfo;
  memoryAllocateInfo.image = mImage;
  memoryAllocateInfo.buffer = nullptr;

  if (mMemory) {
    mDeviceWrapper->logicalDevice.freeMemory(mMemory);
    mMemory = nullptr;
  }

  vk::MemoryAllocateInfo allocateInfo{};
  allocateInfo.pNext = &memoryAllocateInfo;
  allocateInfo.allocationSize = mHardwareBufferInfo.allocationSize;
  allocateInfo.memoryTypeIndex = mHardwareBufferInfo.memoryTypeIndex;
  CALL_VK(mDeviceWrapper->logicalDevice.allocateMemory(&allocateInfo, nullptr,
                                                       &mMemory));
  // Bind image to the device memory
  vk::BindImageMemoryInfo bind_info;

  bind_info.image = mImage;
  bind_info.memory = mMemory;
  bind_info.memoryOffset = 0;

  mDeviceWrapper->logicalDevice.bindImageMemory2KHR(bind_info);

  vk::ImageMemoryRequirementsInfo2 mem_reqs_info;

  mem_reqs_info.image = mImage;

  vk::MemoryDedicatedRequirements ded_mem_reqs;
  vk::MemoryRequirements2 mem_reqs2;

  mem_reqs2.pNext = &ded_mem_reqs;

  mDeviceWrapper->logicalDevice.getImageMemoryRequirements2KHR(&mem_reqs_info,
                                                               &mem_reqs2);

  if (!ded_mem_reqs.prefersDedicatedAllocation ||
      !ded_mem_reqs.requiresDedicatedAllocation)
    return false;

  mSamplerYcbcrConversionInfo = {mSamplerYcbcrConversion};

  vk::ImageViewCreateInfo img_view_info;

  img_view_info.pNext = &mSamplerYcbcrConversionInfo;
  img_view_info.format = formatInfo.format;
  img_view_info.image = mImage;
  img_view_info.viewType = vk::ImageViewType::e2D;
  img_view_info.components = {
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  img_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  img_view_info.subresourceRange.baseMipLevel = 0;
  img_view_info.subresourceRange.levelCount = 1;
  img_view_info.subresourceRange.baseArrayLayer = 0;
  img_view_info.subresourceRange.layerCount = 1;

  if (mImageView) {
    mDeviceWrapper->logicalDevice.destroyImageView(mImageView);
    mImageView = nullptr;
  }

  mImageView = mDeviceWrapper->logicalDevice.createImageView(img_view_info);

  if (mImageInfo.layout != vk::ImageLayout::eUndefined &&
      mImageInfo.layout != vk::ImageLayout::ePreinitialized) {
    // Set layout to mImageInfo.layout
    vk::CommandBuffer copyCommand;
    assert(mDeviceWrapper->beginSingleTimeCommand(&copyCommand));
    vk::ImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mImageInfo.mipLevels;
    subresourceRange.layerCount = mImageInfo.arrayLayers;
    setImageLayout(copyCommand, subresourceRange, mImageInfo.layout, false,
                   vk::ImageLayout::eUndefined);
    mDeviceWrapper->endAndSubmitSingleTimeCommand(copyCommand, mVkQueue);
  }

  return true;
}

bool Image::createSampler() {
  vk::SamplerCreateInfo samplerCreateInfo{};
  samplerCreateInfo.pNext =
      isYUVFormat() ? &mSamplerYcbcrConversionInfo : nullptr,
  // 如果用于LUT图，这里采样需要设置为linear的，这样可以做差值保持色彩精度
      samplerCreateInfo.magFilter = vk::Filter::eLinear;
  samplerCreateInfo.minFilter = vk::Filter::eLinear;
  if (mImageInfo.unnormalizedCoordinates == VK_TRUE) {
    samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.compareEnable = VK_FALSE;
  } else {
    samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = (float)mImageInfo.mipLevels;
    samplerCreateInfo.compareEnable = VK_TRUE;
  }
  // Use clamp to edge for BLUR filter
  samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
  samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
  samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
  samplerCreateInfo.maxAnisotropy = 1;
  samplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
  // 不归一化坐标:默认false，即归一化坐标。
  samplerCreateInfo.unnormalizedCoordinates =
      mImageInfo.unnormalizedCoordinates;
  auto sam = mSampler;
  CALL_VK(mDeviceWrapper->logicalDevice.createSampler(&samplerCreateInfo,
                                                      nullptr, &sam));
  return true;
}

bool Image::createImageView() {
  const auto getImageViewType =
      [=](vk::ImageType imageType) -> vk::ImageViewType {
    switch (imageType) {
    case vk::ImageType::e2D:
      if (mImageInfo.arrayLayers > 1) {
        return vk::ImageViewType::eCube;
      } else {
        return vk::ImageViewType::e2D;
      }
    case vk::ImageType::e3D:
      return vk::ImageViewType::e3D;
    }
  };
  vk::ImageViewCreateInfo viewCreateInfo{};
  viewCreateInfo.pNext = isYUVFormat() ? &mSamplerYcbcrConversionInfo : nullptr;
  viewCreateInfo.image = mImage;
  viewCreateInfo.viewType = getImageViewType(mImageInfo.imageType);
  viewCreateInfo.format = mImageInfo.format;
  viewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0,
                                     mImageInfo.mipLevels, 0,
                                     mImageInfo.arrayLayers};
  viewCreateInfo.subresourceRange.layerCount = mImageInfo.arrayLayers;
  viewCreateInfo.subresourceRange.levelCount = mImageInfo.mipLevels;

  CALL_VK(mDeviceWrapper->logicalDevice.createImageView(&viewCreateInfo,
                                                        nullptr, &mImageView));
  return true;
}

bool Image::createSamplerYcbcrConversionInfo() {
  // Create conversion object that describes how to have the implementation do
  // the {YCbCr} conversion
  vk::SamplerYcbcrConversionCreateInfo samplerYcbcrConversionCreateInfo = {};

  // Which 3x3 YUV to RGB matrix is used?
  // 601 is generally used for SD content.
  // 709 for HD content.
  // 2020 for UHD content.
  // Can also use IDENTITY which lets you sample the raw YUV and
  // do the conversion in shader code.
  // At least you don't have to hit the texture unit 3 times.
  samplerYcbcrConversionCreateInfo.ycbcrModel =
      vk::SamplerYcbcrModelConversion::eYcbcr709;

  // TV (NARROW) or PC (FULL) range for YUV?
  // Usually, JPEG uses full range and broadcast content is narrow.
  // If using narrow, the YUV components need to be
  // rescaled before it can be converted.
  samplerYcbcrConversionCreateInfo.ycbcrRange = vk::SamplerYcbcrRange::eItuFull;

  // Deal with order of components.
  samplerYcbcrConversionCreateInfo.components = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
  };

  // With NEAREST, chroma is duplicated to a 2x2 block for YUV420p.
  // In fancy video players, you might even get bicubic/sinc
  // interpolation filters for chroma because why not ...
  samplerYcbcrConversionCreateInfo.chromaFilter = vk::Filter::eLinear;

  // COSITED or MIDPOINT
  samplerYcbcrConversionCreateInfo.xChromaOffset =
      vk::ChromaLocation::eMidpoint;
  samplerYcbcrConversionCreateInfo.yChromaOffset =
      vk::ChromaLocation::eMidpoint;

  samplerYcbcrConversionCreateInfo.forceExplicitReconstruction = VK_FALSE;

  // For YUV420p.
  samplerYcbcrConversionCreateInfo.format = mImageInfo.format;

  CALL_VK(mDeviceWrapper->logicalDevice.createSamplerYcbcrConversion(
      &samplerYcbcrConversionCreateInfo, nullptr, &mSamplerYcbcrConversion));

  mSamplerYcbcrConversionInfo = {mSamplerYcbcrConversion};

  return true;
}

bool Image::isYUVFormat() {
  switch (mImageInfo.format) {
  case vk::Format::eG8B8R82Plane420Unorm:
  case vk::Format::eG8B8R82Plane422Unorm:
  case vk::Format::eG16B16R162Plane420Unorm:
  case vk::Format::eG16B16R162Plane422Unorm:
  case vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16:
  case vk::Format::eG10X6B10X6R10X62Plane422Unorm3Pack16:
  case vk::Format::eG12X4B12X4R12X42Plane420Unorm3Pack16:
  case vk::Format::eG12X4B12X4R12X42Plane422Unorm3Pack16:
  case vk::Format::eG8B8R83Plane420Unorm:
  case vk::Format::eG8B8R83Plane422Unorm:
  case vk::Format::eG8B8R83Plane444Unorm:
  case vk::Format::eG16B16R163Plane420Unorm:
  case vk::Format::eG16B16R163Plane422Unorm:
  case vk::Format::eG16B16R163Plane444Unorm:
  case vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16:
  case vk::Format::eG10X6B10X6R10X63Plane422Unorm3Pack16:
  case vk::Format::eG10X6B10X6R10X63Plane444Unorm3Pack16:
  case vk::Format::eG12X4B12X4R12X43Plane420Unorm3Pack16:
  case vk::Format::eG12X4B12X4R12X43Plane422Unorm3Pack16:
  case vk::Format::eG12X4B12X4R12X43Plane444Unorm3Pack16:
    return true;
  }
  return false;
}

void Image::setImageLayout(vk::CommandBuffer cmdbuffer,
                           vk::ImageSubresourceRange subresourceRange,
                           vk::ImageLayout newImageLayout, bool ignoreOldLayout,
                           vk::ImageLayout oldImageLayout,
                           vk::PipelineStageFlags srcStageMask,
                           vk::PipelineStageFlags dstStageMask) {
  if (newImageLayout == vk::ImageLayout::eUndefined ||
      newImageLayout == vk::ImageLayout::ePreinitialized) {
    return;
  }

  vk::ImageLayout oldLayout;
  if (ignoreOldLayout) {
    oldLayout = mImageInfo.layout;
  } else {
    oldLayout = oldImageLayout;
  }

  // Create an image barrier object
  vk::ImageMemoryBarrier imageMemoryBarrier{};
  imageMemoryBarrier.oldLayout = oldLayout;
  imageMemoryBarrier.newLayout = newImageLayout;
  imageMemoryBarrier.image = mImage;
  imageMemoryBarrier.subresourceRange = subresourceRange;

  // Source layouts (old)
  // Source access mask controls actions that have to be finished on the old
  // layout before it will be transitioned to the new layout
  switch (oldLayout) {
  case vk::ImageLayout::eUndefined:
    // Image layout is undefined (or does not matter)
    // Only valid as initial layout
    // No flags required, listed only for completeness
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
    break;

  case vk::ImageLayout::ePreinitialized:
    // Image is preinitialized
    // Only valid as initial layout for linear images, preserves memory contents
    // Make sure host writes have been finished
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
    break;

  case vk::ImageLayout::eColorAttachmentOptimal:
    // Image is a color attachment
    // Make sure any writes to the color buffer have been finished
    imageMemoryBarrier.srcAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite;
    break;

  case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    // Image is a depth/stencil attachment
    // Make sure any writes to the depth/stencil buffer have been finished
    imageMemoryBarrier.srcAccessMask =
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    break;

  case vk::ImageLayout::eTransferSrcOptimal:
    // Image is a transfer source
    // Make sure any reads from the image have been finished
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    break;

  case vk::ImageLayout::eTransferDstOptimal:
    // Image is a transfer destination
    // Make sure any writes to the image have been finished
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    break;

  case vk::ImageLayout::eShaderReadOnlyOptimal:
    // Image is read by a shader
    // Make sure any shader reads from the image have been finished
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    break;
  default:
    // Other source layouts aren't handled (yet)
    break;
  }

  // Target layouts (new)
  // Destination access mask controls the dependency for the new image layout
  switch (newImageLayout) {
  case vk::ImageLayout::eTransferDstOptimal:
    // Image will be used as a transfer destination
    // Make sure any writes to the image have been finished
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    break;

  case vk::ImageLayout::eTransferSrcOptimal:
    // Image will be used as a transfer source
    // Make sure any reads from the image have been finished
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    break;

  case vk::ImageLayout::eColorAttachmentOptimal:
    // Image will be used as a color attachment
    // Make sure any writes to the color buffer have been finished
    imageMemoryBarrier.dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentWrite;
    break;

  case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    // Image layout will be used as a depth/stencil attachment
    // Make sure any writes to depth/stencil buffer have been finished
    imageMemoryBarrier.dstAccessMask =
        imageMemoryBarrier.dstAccessMask |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    break;

  case vk::ImageLayout::eShaderReadOnlyOptimal:
    // Image will be read in a shader (sampler, input attachment)
    // Make sure any writes to the image have been finished
    if (imageMemoryBarrier.srcAccessMask == vk::AccessFlagBits::eNone) {
      imageMemoryBarrier.srcAccessMask =
          vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
    }
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    break;
  default:
    // Other source layouts aren't handled (yet)
    break;
  }

  // Put barrier inside setup command buffer
  cmdbuffer.pipelineBarrier(srcStageMask, dstStageMask, vk::DependencyFlags(),
                            0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

  mImageInfo.layout = newImageLayout;
}

void Image::setImageLayout(vk::CommandBuffer cmdbuffer,
                           vk::ImageAspectFlags aspectMask,
                           vk::ImageLayout newImageLayout, bool ignoreOldLayout,
                           vk::ImageLayout oldImageLayout,
                           vk::PipelineStageFlags srcStageMask,
                           vk::PipelineStageFlags dstStageMask) {
  vk::ImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = aspectMask;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 1;
  setImageLayout(cmdbuffer, subresourceRange, newImageLayout, ignoreOldLayout,
                 oldImageLayout, srcStageMask, dstStageMask);
}

Image::Image(const std::shared_ptr<vks::VulkanDeviceWrapper> context,
             vk::Queue queue, const ImageBasicInfo &imageInfo)
    : mDeviceWrapper(context), mVkQueue(queue), mImageInfo(imageInfo) {}

} // namespace gain
