/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 by Gain
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

#pragma once

#include "VulkanDebug.h"
#include <LogUtil.h>
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <exception>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vks {
struct VulkanDeviceWrapper {
  vk::PhysicalDevice physicalDevice;
  vk::Device logicalDevice;
  vk::PhysicalDeviceProperties properties;
  vk::PhysicalDeviceFeatures features;
  vk::PhysicalDeviceFeatures enabledFeatures;
  vk::PhysicalDeviceMemoryProperties memoryProperties;
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties;
  vk::CommandPool commandPool = VK_NULL_HANDLE;
  uint32_t workGroupSize = 0;

  struct {
    uint32_t graphics;
    uint32_t compute;
  } queueFamilyIndices;

  operator vk::Device() { return logicalDevice; };

  /**
   * Default constructor
   *
   * @param physicalDevice Physical device that is to be used
   */
  VulkanDeviceWrapper(vk::PhysicalDevice physicalDevice) {
    assert(physicalDevice);
    this->physicalDevice = physicalDevice;

    // Store Properties features, limits and properties of the physical device
    // for later use Device properties also contain limits and sparse properties
    physicalDevice.getProperties(&properties);
    // Features should be checked by the examples before using them
    physicalDevice.getFeatures(&features);
    // Memory properties are used regularly for creating all kinds of buffers
    physicalDevice.getMemoryProperties(&memoryProperties);
    // Queue family properties, used for setting up requested queues upon device
    // creation
    uint32_t queueFamilyCount;
    physicalDevice.getQueueFamilyProperties(&queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    physicalDevice.getQueueFamilyProperties(&queueFamilyCount,
                                            queueFamilyProperties.data());
  }

  /**
   * Default destructor
   *
   * @note Frees the logical device
   */
  ~VulkanDeviceWrapper() {
    if (commandPool) {
      logicalDevice.destroyCommandPool(commandPool, nullptr);
    }
    if (logicalDevice) {
      logicalDevice.destroy(nullptr);
    }
  }

  // Choose the work group size of the compute shader.
  // In this sample app, we are using a square execution dimension.
  uint32_t chooseWorkGroupSize(const vk::PhysicalDeviceLimits &limits) {
    // Use 64 as the baseline.
    uint32_t size = 64;

    // Make sure the size does not exceed the limit along the X and Y axis.
    size = std::min<uint32_t>(size, limits.maxComputeWorkGroupSize[0]);
    size = std::min<uint32_t>(size, limits.maxComputeWorkGroupSize[1]);

    // Make sure the total number of invocations does not exceed the limit.
    size = std::min<uint32_t>(
        size,
        static_cast<uint32_t>(sqrt(limits.maxComputeWorkGroupInvocations)));

    // We prefer the workgroup size to be a multiple of 4.
    size &= ~(3u);

    LOGCATI(
        "maxComputeWorkGroupInvocations: %d, maxComputeWorkGroupSize: (%d, %d)",
        limits.maxComputeWorkGroupInvocations,
        limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1]);
    LOGCATI("Choose workgroup size: (%d, %d)", size, size);
    return size;
  }

  /**
   * Get the index of a memory type that has all the requested property bits set
   *
   * @param typeBits Bitmask with bits set for each memory type supported by the
   * resource to request for (from vk::MemoryRequirements)
   * @param properties Bitmask of properties for the memory type to request
   * @param (Optional) isExternal External memory
   *
   * @return Index of the requested memory type
   *
   * @throw Throws an exception if memTypeFound is null and no memory type could
   * be found that supports the requested properties
   */
  uint32_t getMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties,
                         bool isExternal = false) const {
    int mem_index{0};

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
      if ((typeBits & 1) == 1) {
        if (isExternal) {
          if ((memoryProperties.memoryTypes[i].propertyFlags &
               vk::MemoryPropertyFlags{0}) == vk::MemoryPropertyFlags{0}) {
            return i;
          }
        } else {
          if (memoryProperties.memoryTypes[i].propertyFlags & properties) {
            return i;
          }
        }
      }
      typeBits >>= 1;
    }

    return mem_index;
  }

  /**
   * Get the index of a queue family that supports the requested queue flags
   *
   * @param queueFlags Queue flags to find a queue family index for
   *
   * @return Index of the queue family index that matches the flags
   *
   * @throw Throws an exception if no queue family index could be found that
   * supports the requested flags
   */
  uint32_t getQueueFamilyIndex(vk::QueueFlagBits queueFlags) {
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if (queueFlags & vk::QueueFlagBits::eCompute) {
      for (uint32_t i = 0;
           i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
        if ((queueFamilyProperties[i].queueFlags & queueFlags) &&
            (!(queueFamilyProperties[i].queueFlags &
               vk::QueueFlagBits::eGraphics))) {
          return i;
        }
      }
    }

    // For other queue types or if no separate compute queue is present, return
    // the first one to support the requested flags
    for (uint32_t i = 0;
         i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
      if (queueFamilyProperties[i].queueFlags & queueFlags) {
        return i;
      }
    }

    throw std::runtime_error("Could not find a matching queue family index");
  }

  void getDepthFormat(vk::Format &depthFormat) {
    // Get depth format
    //  Since all depth formats may be optional, we need to find a suitable
    //  depth format to use Start with the highest precision packed format
    std::vector<vk::Format> depthFormats = {
        vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat,
        vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint,
        vk::Format::eD16Unorm};

    for (auto &format : depthFormats) {
      vk::FormatProperties formatProps;
      physicalDevice.getFormatProperties(format, &formatProps);
      // Format must support depth stencil attachment for optimal tiling
      if (formatProps.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
        depthFormat = format;
        break;
      }
    }
  }

  /**
   * Create the logical device based on the assigned physical device, also gets
   * default queue family indices
   *
   * @param enabledFeatures Can be used to enable certain features upon device
   * creation
   * @param requestedQueueTypes Bit flags specifying the queue types to be
   * requested from the device
   *
   * @return vk::Result of the device creation call
   */
  vk::Result createLogicalDevice(
      vk::PhysicalDeviceFeatures enabledFeatures,
      std::vector<const char *> enabledExtensions,
      vk::QueueFlags requestedQueueTypes = vk::QueueFlagBits::eGraphics |
                                           vk::QueueFlagBits::eCompute) {
    // Desired queues need to be requested upon logical device creation
    // Due to differing queue family configurations of Vulkan implementations
    // this can be a bit tricky, especially if the application requests
    // different queue types

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};

    // Get queue family indices for the requested queue family types
    // Note that the indices may overlap depending on the implementation

    // 挑选支持指定Flag Bit的设备
    // 需要了解的是，不同的硬件可能支持不同的 Flag Bit 组合，但 Vulkan
    // 规范指出，任何 Vulkan 设备至少有一种 Graphics 和 Compute
    // 的组合，同时，无论是 Graphics 还是 Compute，都隐含的支持 Transfer
    // 命令，也就是说， 任何 Vulkan 硬件，都至少有一个可以执行 Graphics、Compute
    // 和 Transfer 命令的 Queue 族。尽管如此， Vulkan 规范并不要求实现必须要上报
    // Transfer，这是实现的可选项，所以通过
    // vkGetPhysicalDeviceQueueFamilyProperties 获取到的 Graphics | Compute
    // 组合可能不一定包含 Transfer Flag Bit，但实际上是支持 Transfer 命令的。
    const float defaultQueuePriority(0.0f);

    // Graphics queue
    if (requestedQueueTypes & vk::QueueFlagBits::eGraphics) {
      queueFamilyIndices.graphics =
          getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
      vk::DeviceQueueCreateInfo queueInfo{};
      queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
      queueInfo.queueCount = 1;
      queueInfo.pQueuePriorities = &defaultQueuePriority;
      queueCreateInfos.push_back(queueInfo);
    } else {
      queueFamilyIndices.graphics = VK_QUEUE_FAMILY_IGNORED;
    }

    // Dedicated compute queue
    if (requestedQueueTypes & vk::QueueFlagBits::eCompute) {
      queueFamilyIndices.compute =
          getQueueFamilyIndex(vk::QueueFlagBits::eCompute);
      if (queueFamilyIndices.compute != queueFamilyIndices.graphics) {
        // If compute family index differs, we need an additional queue create
        // info for the compute queue
        vk::DeviceQueueCreateInfo queueInfo{};
        queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
      }
    } else {
      // Else we use the same queue
      queueFamilyIndices.compute = queueFamilyIndices.graphics;
    }

    // Create the logical device representation
    std::vector<const char *> deviceExtensions(enabledExtensions);
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    vk::DeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    ;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    if (deviceExtensions.size() > 0) {
      deviceCreateInfo.enabledExtensionCount =
          (uint32_t)deviceExtensions.size();
      deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    // Enable extensions' features
    auto isEnabled = [&](const char *extension) {
      return std::find_if(enabledExtensions.begin(), enabledExtensions.end(),
                          [extension](const char *enabled_extension) {
                            return strcmp(extension, enabled_extension) == 0;
                          }) != enabledExtensions.end();
    };
    if (isEnabled(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME)) {
      vk::PhysicalDeviceSamplerYcbcrConversionFeaturesKHR
          samplerYcbcrConversionFeature = {
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
              nullptr};
      vk::PhysicalDeviceFeatures2KHR enabledFeatures{};
      enabledFeatures.pNext = &samplerYcbcrConversionFeature;
      samplerYcbcrConversionFeature.samplerYcbcrConversion = VK_TRUE;
      deviceCreateInfo.pNext = &enabledFeatures;
      deviceCreateInfo.pEnabledFeatures = nullptr;
    }

    vk::Result result =
        physicalDevice.createDevice(&deviceCreateInfo, nullptr, &logicalDevice);

    if (result == vk::Result::eSuccess) {
      if (queueFamilyIndices.graphics != VK_QUEUE_FAMILY_IGNORED) {
        commandPool = createCommandPool(queueFamilyIndices.graphics);
      } else {
        commandPool = createCommandPool(queueFamilyIndices.compute);
      }
    } else {
      throw "vkCreateDevice failed!";
    }

    this->enabledFeatures = enabledFeatures;

    workGroupSize = chooseWorkGroupSize(properties.limits);

    return result;
  }

  /**
   * Create a command pool for allocation command buffers from
   *
   * @param queueFamilyIndex Family index of the queue to create the command
   * pool for
   * @param createFlags (Optional) Command pool creation flags (Defaults to
   * VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
   *
   * @note Command buffers allocated from the created pool can only be submitted
   * to a queue with the same family index
   *
   * @return A handle to the created command buffer
   */
  vk::CommandPool
  createCommandPool(uint32_t queueFamilyIndex,
                    vk::CommandPoolCreateFlags createFlags =
                        vk::CommandPoolCreateFlagBits::eResetCommandBuffer) {
    vk::CommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.flags = createFlags;
    vk::CommandPool cmdPool;
    CALL_VK(logicalDevice.createCommandPool(&cmdPoolInfo, nullptr, &cmdPool));
    return cmdPool;
  }

  /**
   * Allocate a command buffer from the command pool
   *
   * @param level Level of the new command buffer (primary or secondary)
   * @param (Optional) begin If true, recording on the new command buffer will
   * be started (vkBeginCommandBuffer) (Defaults to false)
   *
   * @return A handle to the allocated command buffer
   */
  vk::CommandBuffer createCommandBuffer(vk::CommandBufferLevel level,
                                        bool begin = false) {
    vk::CommandBufferAllocateInfo cmdBufAllocateInfo{};
    cmdBufAllocateInfo.commandPool = commandPool;
    cmdBufAllocateInfo.level = level;
    cmdBufAllocateInfo.commandBufferCount = 1;

    vk::CommandBuffer cmdBuffer;
    CALL_VK(
        logicalDevice.allocateCommandBuffers(&cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start recording for the new command buffer
    if (begin) {
      vk::CommandBufferBeginInfo commandBufferBI{};
      CALL_VK(cmdBuffer.begin(&commandBufferBI));
    }

    return cmdBuffer;
  }

  void beginCommandBuffer(vk::CommandBuffer commandBuffer) {
    vk::CommandBufferBeginInfo commandBufferBI{};
    CALL_VK(commandBuffer.begin(&commandBufferBI));
  }

  // Create a command buffer with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  // and begin command buffer recording.
  bool beginSingleTimeCommand(vk::CommandBuffer *commandBuffer) const {
    if (commandBuffer == nullptr)
      return false;

    // Allocate a command buffer
    const vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {
        commandPool, vk::CommandBufferLevel::ePrimary, 1};
    CALL_VK(logicalDevice.allocateCommandBuffers(&commandBufferAllocateInfo,
                                                 commandBuffer));

    // Begin command buffer recording
    const vk::CommandBufferBeginInfo beginInfo = {
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    CALL_VK((*commandBuffer).begin(&beginInfo));
    return true;
  }

  // End the command buffer recording, submit it to the queue, and wait until it
  // is finished.
  void endAndSubmitSingleTimeCommand(vk::CommandBuffer commandBuffer,
                                     vk::Queue queue, bool free = true) const {
    vks::debug::setCommandBufferName(logicalDevice, commandBuffer,
                                     "SingleTimeCommand");

    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Create fence to ensure that the command buffer has finished executing
    vk::FenceCreateInfo fenceInfo{};
    vk::Fence fence;
    CALL_VK(logicalDevice.createFence(&fenceInfo, nullptr, &fence));

    // Submit to the queue
    CALL_VK(queue.submit(1, &submitInfo, fence));
    // Wait for the fence to signal that command buffer has finished executing
    CALL_VK(logicalDevice.waitForFences(1, &fence, VK_TRUE, 100000000000));

    logicalDevice.destroyFence(fence, nullptr);

    if (free) {
      logicalDevice.freeCommandBuffers(commandPool, 1, &commandBuffer);
    }
  }
};
} // namespace vks
