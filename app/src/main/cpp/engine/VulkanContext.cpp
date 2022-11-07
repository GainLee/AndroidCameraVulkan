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

#include "VulkanContext.h"
#include "../util/LogUtil.h"
#include "VulkanDebug.h"
#include "includes/cube_data.h"
#include <array>
#include <cmath>
#include <optional>

// Instantiate the default dispatcher
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

bool VulkanContext::create(AAssetManager *assetManager) {
  mAssetManager = assetManager;
  getDeviceConfig();
  bool ret =
      createInstance() && pickPhysicalDeviceAndQueueFamily() && createDevice();

  return ret;
}

void VulkanContext::getDeviceConfig() {
  // Screen density
  AConfiguration *config = AConfiguration_new();
  AConfiguration_fromAssetManager(config, mAssetManager);
  mScreenDensity = AConfiguration_getDensity(config);
  AConfiguration_delete(config);
}

bool VulkanContext::createInstance() {
  static vk::DynamicLoader dl;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
      dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

  // Required instance layers
  std::vector<const char *> instanceLayers;
  if (vks::debug::debuggable) {
    instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
  }

  // Required instance extensions
  std::vector<const char *> instanceExtensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  if (vks::debug::debuggable) {
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  // Create instance
  const vk::ApplicationInfo applicationDesc{
      "VulkanCamera",
      VK_MAKE_VERSION(0, 0, 1),
      "VkCamEngine",
      static_cast<uint32_t>((VK_VERSION_MINOR(mInstanceVersion) >= 1)
                                ? VK_API_VERSION_1_1
                                : VK_API_VERSION_1_0),
  };

  vk::InstanceCreateInfo instanceDesc{
      {},
      &applicationDesc,
      static_cast<uint32_t>(instanceLayers.size()),
      instanceLayers.data(),
      static_cast<uint32_t>(instanceExtensions.size()),
      instanceExtensions.data(),
  };

  mInstance = vk::createInstance(instanceDesc);

  // initialize the Vulkan-Hpp default dispatcher on the instance
  VULKAN_HPP_DEFAULT_DISPATCHER.init(mInstance);

  if (vks::debug::debuggable) {
    vks::debug::setupDebugging(mInstance);
  }

  return true;
}

bool VulkanContext::pickPhysicalDeviceAndQueueFamily(
    vk::QueueFlags requestedQueueTypes) {
  uint32_t numDevices = 0;
  CALL_VK(mInstance.enumeratePhysicalDevices(&numDevices, nullptr));
  std::vector<vk::PhysicalDevice> devices(numDevices);
  CALL_VK(mInstance.enumeratePhysicalDevices(&numDevices, devices.data()));

  for (auto device : devices) {
    uint32_t numQueueFamilies = 0;
    device.getQueueFamilyProperties(&numQueueFamilies, nullptr);
    std::vector<vk::QueueFamilyProperties> queueFamilies(numQueueFamilies);
    device.getQueueFamilyProperties(&numQueueFamilies, queueFamilies.data());

    bool haveQf = false;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
      if (queueFamilies[i].queueFlags & requestedQueueTypes) {
        haveQf = true;
        break;
      }
    }
    if (!haveQf)
      continue;
    mDeviceWrapper = std::make_shared<vks::VulkanDeviceWrapper>(device);
    break;
  }

  return true;
}

bool VulkanContext::createDevice(vk::QueueFlags requestedQueueTypes) {
  // Required device extensions
  // These extensions are required to import an AHardwareBuffer to Vulkan.
  std::vector<const char *> deviceExtensions = {
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
      VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
      VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
      VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
  };

  vk::PhysicalDeviceFeatures enabledFeatures{};

  mDeviceWrapper->createLogicalDevice(enabledFeatures, deviceExtensions,
                                      requestedQueueTypes);

  mDeviceWrapper->logicalDevice.getQueue(
      mDeviceWrapper->queueFamilyIndices.graphics, 0, &mGraphicsQueue);
  mDeviceWrapper->logicalDevice.getQueue(
      mDeviceWrapper->queueFamilyIndices.compute, 0, &mComputeQueue);

  createPipelineCache();

  return true;
}

void VulkanContext::createPipelineCache() {
  vk::PipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  CALL_VK(device().createPipelineCache(&pipelineCacheCreateInfo, nullptr,
                                       &mPipelineCache));
}

VulkanContext::~VulkanContext() {
  device().waitIdle();

  if (mPipelineCache) {
    device().destroyPipelineCache(mPipelineCache);
  }

  if (vks::debug::debuggable) {
    vks::debug::freeDebugCallback(mInstance);
  }

  if (mInstance) {
    mInstance.destroy();
  }
}