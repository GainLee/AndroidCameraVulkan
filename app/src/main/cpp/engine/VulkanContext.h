/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 by Gain
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

#ifndef GAINVULKANSAMPLE_VULKANCONTEXTBASE_H
#define GAINVULKANSAMPLE_VULKANCONTEXTBASE_H

#include "VulkanDeviceWrapper.hpp"
#include "VulkanSwapChain.h"
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <android/configuration.h>
#include <android/hardware_buffer_jni.h>
#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_RADIANS
#include "VulkanBufferWrapper.h"
#include "camera.hpp"
#include <glm/glm.hpp>

class Context;

class VulkanContext {
private:
  void getDeviceConfig();
  void createPipelineCache();

public:
  bool create(AAssetManager *assetManager);
  // Prefer VulkanContext::create
  VulkanContext() : mInstance(nullptr), mPipelineCache(nullptr) {}

  virtual ~VulkanContext();

  AAssetManager *mAssetManager;

  uint32_t mScreenDensity;

  // Getters of the managed Vulkan objects
  const std::shared_ptr<vks::VulkanDeviceWrapper> deviceWrapper() const {
    return mDeviceWrapper;
  }
  vk::PhysicalDevice physicalDevice() const {
    return mDeviceWrapper->physicalDevice;
  }
  vk::Instance instance() const { return mInstance; }
  vk::Device device() const { return mDeviceWrapper->logicalDevice; }
  vk::Queue queue() const {
    // TODO 三种队列都要提供获取方式
    return mGraphicsQueue;
  }
  vk::PipelineCache pipelineCache() const { return mPipelineCache; }
  vk::CommandPool commandPool() const { return mDeviceWrapper->commandPool; }
  AAssetManager *_AAssetManager() const { return mAssetManager; }

protected:
  // Initialization
  bool createInstance();
  bool pickPhysicalDeviceAndQueueFamily(
      vk::QueueFlags requestedQueueTypes = vk::QueueFlagBits::eGraphics |
                                           vk::QueueFlagBits::eCompute);
  bool createDevice(
      vk::QueueFlags requestedQueueTypes = vk::QueueFlagBits::eGraphics |
                                           vk::QueueFlagBits::eCompute);

  uint32_t getQueueFamilyIndex(vk::QueueFlagBits queueFlags) const;

  // Instance
  uint32_t mInstanceVersion = 0;
  vk::Instance mInstance;

  // Device and queue
  std::shared_ptr<vks::VulkanDeviceWrapper> mDeviceWrapper;

  vk::Queue mGraphicsQueue = nullptr;
  vk::Queue mComputeQueue = nullptr;
  vk::Queue mPresentQueue = nullptr;

  vk::PipelineCache mPipelineCache = nullptr;
};

#endif // GAINVULKANSAMPLE_VULKANCONTEXTBASE_H
