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
#pragma once

#include "../util/LogUtil.h"
#include <string>
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

namespace vks {
namespace debug {
extern bool debuggable;

// Load debug function pointers and set debug callback
void setupDebugging(vk::Instance instance);

// Clear debug callback
void freeDebugCallback(vk::Instance instance);

// Sets the debug name of an object
// All Objects in Vulkan are represented by their 64-bit handles which are
// passed into this function along with the object type
void setObjectName(vk::Device device, uint64_t object,
                   vk::ObjectType objectType, const char *name);

// Set the tag for an object
void setObjectTag(vk::Device device, uint64_t object, vk::ObjectType objectType,
                  uint64_t name, size_t tagSize, const void *tag);

// Start a new debug marker region
void beginRegion(vk::CommandBuffer cmdbuffer, const char *pMarkerName,
                 glm::vec4 color);

// Insert a new debug marker into the command buffer
void insert(vk::CommandBuffer cmdbuffer, std::string markerName,
            glm::vec4 color);

// End the current debug marker region
void endRegion(vk::CommandBuffer cmdBuffer);

// Object specific naming functions
void setCommandBufferName(vk::Device device, vk::CommandBuffer cmdBuffer,
                          const char *name);

void setQueueName(vk::Device device, vk::Queue queue, const char *name);

void setImageName(vk::Device device, vk::Image image, const char *name);

void setSamplerName(vk::Device device, vk::Sampler sampler, const char *name);

void setBufferName(vk::Device device, vk::Buffer buffer, const char *name);

void setDeviceMemoryName(vk::Device device, vk::DeviceMemory memory,
                         const char *name);

void setShaderModuleName(vk::Device device, vk::ShaderModule shaderModule,
                         const char *name);

void setPipelineName(vk::Device device, vk::Pipeline pipeline,
                     const char *name);

void setPipelineLayoutName(vk::Device device, vk::PipelineLayout pipelineLayout,
                           const char *name);

void setRenderPassName(vk::Device device, vk::RenderPass renderPass,
                       const char *name);

void setFramebufferName(vk::Device device, vk::Framebuffer framebuffer,
                        const char *name);

void setDescriptorSetLayoutName(vk::Device device,
                                vk::DescriptorSetLayout descriptorSetLayout,
                                const char *name);

void setDescriptorSetName(vk::Device device, vk::DescriptorSet descriptorSet,
                          const char *name);

void setSemaphoreName(vk::Device device, vk::Semaphore semaphore,
                      const char *name);

void setFenceName(vk::Device device, vk::Fence fence, const char *name);

void setEventName(vk::Device device, vk::Event _event, const char *name);
} // namespace debug
} // namespace vks
