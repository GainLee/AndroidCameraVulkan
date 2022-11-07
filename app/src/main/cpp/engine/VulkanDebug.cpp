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
#include "VulkanDebug.h"
#include <iostream>
#include <sstream>

#if defined(_MSC_VER) && !defined(_WIN64)
#define NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(type, x) static_cast<type>(x)
#else
#define NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(type, x)                        \
  reinterpret_cast<uint64_t>(static_cast<type>(x))
#endif

namespace vks {
namespace debug {
bool debuggable = true;

#define IF_NOT_DEBUGGABLE_RETURN                                               \
  if (!debuggable) {                                                           \
    return;                                                                    \
  }

vk::DebugUtilsMessengerEXT debugUtilsMessenger = nullptr;

VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsMessengerCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
  // Select prefix depending on flags passed to the callback
  std::string prefix("");

  if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
    prefix = "VERBOSE: ";
  } else if (messageSeverity &
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
    prefix = "INFO: ";
  } else if (messageSeverity &
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
    prefix = "WARNING: ";
  } else if (messageSeverity &
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
    prefix = "ERROR: ";
  }

  // Display message to default output (console/logcat)
  std::stringstream debugMessage;
  debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "]["
               << pCallbackData->pMessageIdName
               << "] : " << pCallbackData->pMessage;

  if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
    // Android logcat 有最大长度限制，这里提前截断，循环打印，防止debug信息不全
    auto msg = debugMessage.str();
    int start = 0;
    while (msg.size() > 1023) {
      LOGCATE("%s", msg.c_str());
      start += 1023;
      msg = msg.substr(start, 1023);
    }
    LOGCATE("%s", msg.c_str());
  } else {
    LOGCATD("%s", debugMessage.str().c_str());
  }

  return VK_FALSE;
}

void setupDebugging(vk::Instance instance) {
  IF_NOT_DEBUGGABLE_RETURN

  vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
  debugUtilsMessengerCI.messageSeverity =
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
  debugUtilsMessengerCI.messageType =
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  debugUtilsMessengerCI.pfnUserCallback =
      reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(
          debugUtilsMessengerCallback);
  debugUtilsMessenger =
      instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCI);
}

void freeDebugCallback(vk::Instance instance) {
  if (debugUtilsMessenger) {
    instance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger);
  }
}

void setObjectName(vk::Device device, uint64_t object,
                   vk::ObjectType objectType, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  vk::DebugUtilsObjectNameInfoEXT nameInfo = {};
  nameInfo.objectType = objectType;
  nameInfo.objectHandle = object;
  nameInfo.pObjectName = name;
  device.setDebugUtilsObjectNameEXT(nameInfo);
}

void setObjectTag(vk::Device device, uint64_t object, vk::ObjectType objectType,
                  uint64_t name, size_t tagSize, const void *tag) {
  IF_NOT_DEBUGGABLE_RETURN

  vk::DebugUtilsObjectTagInfoEXT tagInfo = {};
  tagInfo.objectType = objectType;
  tagInfo.objectHandle = object;
  tagInfo.tagName = name;
  tagInfo.tagSize = tagSize;
  tagInfo.pTag = tag;
  device.setDebugUtilsObjectTagEXT(tagInfo);
}

void beginRegion(vk::CommandBuffer cmdbuffer, const char *pMarkerName,
                 glm::vec4 color) {
  IF_NOT_DEBUGGABLE_RETURN

  vk::DebugUtilsLabelEXT markerInfo = {};
  memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
  markerInfo.pLabelName = pMarkerName;
  cmdbuffer.beginDebugUtilsLabelEXT(markerInfo);
}

void insert(vk::CommandBuffer cmdbuffer, std::string markerName,
            glm::vec4 color) {
  IF_NOT_DEBUGGABLE_RETURN

  vk::DebugUtilsLabelEXT markerInfo = {};
  memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
  markerInfo.pLabelName = markerName.c_str();
  cmdbuffer.insertDebugUtilsLabelEXT(markerInfo);
}

void endRegion(vk::CommandBuffer cmdBuffer) {
  IF_NOT_DEBUGGABLE_RETURN

  cmdBuffer.endDebugUtilsLabelEXT();
}

void setCommandBufferName(vk::Device device, vk::CommandBuffer cmdBuffer,
                          const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(
      device,
      NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkCommandBuffer, cmdBuffer),
      vk::ObjectType::eCommandBuffer, name);
}

void setQueueName(vk::Device device, vk::Queue queue, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkQueue, queue),
                vk::ObjectType::eQueue, name);
}

void setImageName(vk::Device device, vk::Image image, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkImage, image),
                vk::ObjectType::eImage, name);
}

void setSamplerName(vk::Device device, vk::Sampler sampler, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device,
                NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkSampler, sampler),
                vk::ObjectType::eSampler, name);
}

void setBufferName(vk::Device device, vk::Buffer buffer, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device,
                NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkBuffer, buffer),
                vk::ObjectType::eBuffer, name);
}

void setDeviceMemoryName(vk::Device device, vk::DeviceMemory memory,
                         const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device,
                NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkDeviceMemory, memory),
                vk::ObjectType::eDeviceMemory, name);
}

void setShaderModuleName(vk::Device device, vk::ShaderModule shaderModule,
                         const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(
      device,
      NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkShaderModule, shaderModule),
      vk::ObjectType::eShaderModule, name);
}

void setPipelineName(vk::Device device, vk::Pipeline pipeline,
                     const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device,
                NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkPipeline, pipeline),
                vk::ObjectType::ePipeline, name);
}

void setPipelineLayoutName(vk::Device device, vk::PipelineLayout pipelineLayout,
                           const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(
      device,
      NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkPipelineLayout, pipelineLayout),
      vk::ObjectType::ePipelineLayout, name);
}

void setRenderPassName(vk::Device device, vk::RenderPass renderPass,
                       const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(
      device, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkRenderPass, renderPass),
      vk::ObjectType::eRenderPass, name);
}

void setFramebufferName(vk::Device device, vk::Framebuffer framebuffer,
                        const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(
      device,
      NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkFramebuffer, framebuffer),
      vk::ObjectType::eFramebuffer, name);
}

void setDescriptorSetLayoutName(vk::Device device,
                                vk::DescriptorSetLayout descriptorSetLayout,
                                const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device,
                NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkDescriptorSetLayout,
                                                       descriptorSetLayout),
                vk::ObjectType::eDescriptorSetLayout, name);
}

void setDescriptorSetName(vk::Device device, vk::DescriptorSet descriptorSet,
                          const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(
      device,
      NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkDescriptorSet, descriptorSet),
      vk::ObjectType::eDescriptorSet, name);
}

void setSemaphoreName(vk::Device device, vk::Semaphore semaphore,
                      const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device,
                NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkSemaphore, semaphore),
                vk::ObjectType::eSemaphore, name);
}

void setFenceName(vk::Device device, vk::Fence fence, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkFence, fence),
                vk::ObjectType::eFence, name);
}

void setEventName(vk::Device device, vk::Event _event, const char *name) {
  IF_NOT_DEBUGGABLE_RETURN

  setObjectName(device, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkEvent, _event),
                vk::ObjectType::eEvent, name);
}
} // namespace debug
} // namespace vks
