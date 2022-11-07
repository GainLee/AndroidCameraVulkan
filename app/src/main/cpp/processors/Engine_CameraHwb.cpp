//
// Created by Gain on 2022/8/31.
//

#include "Engine_CameraHwb.h"

#include "includes/cube_data.h"

#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void Engine_CameraHwb::setHdwImage(AHardwareBuffer *buffer, int orientation) {
  mBuffer = buffer;
  mOrientation = orientation;
}

void Engine_CameraHwb::prepareHdwImage() {
  Image::ImageBasicInfo imageInfo = {
    usage : vk::ImageUsageFlagBits::eSampled,
    layout : vk::ImageLayout::eShaderReadOnlyOptimal,
    format : vk::Format::eR8G8B8A8Unorm
  };
  mImage = Image::createFromAHardwareBuffer(vulkanContext()->deviceWrapper(),
                                            vulkanContext()->queue(), mBuffer,
                                            imageInfo);
  vks::debug::setImageName(vulkanContext()->device(), mImage->getImageHandle(),
                           "HDW-Image");

  Image::ImageBasicInfo compImageInfo = {
    usage : vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
    layout : vk::ImageLayout::eGeneral,
    format : vk::Format::eR8G8B8A8Unorm,
    extent : {mImage->width(), mImage->height(), 1}
  };
}

void Engine_CameraHwb::prepare(JNIEnv *env) {
  if (!mPrepared) {
    EngineContext::prepare(env);

    prepareHdwImage();

    prepareSynchronizationPrimitives();
    prepareVertices(true, g_vb_bitmap_texture_Data,
                    sizeof(g_vb_bitmap_texture_Data));
    setupDescriptorPool();
    setupDescriptorSetLayout();
    prepareUniformBuffers();
    createDescriptorSet();
    createPipelines();

    mPrepared = true;
  }
}

void Engine_CameraHwb::updateTexture() {
  // 确保更新数据的时候上一帧绘制已经完成
  vulkanContext()->device().waitIdle();
  mImage->setContentFromHardwareBuffer(mBuffer);
}

void Engine_CameraHwb::setupDescriptorPool() {
  vk::DescriptorPoolSize typeCounts[2];

  typeCounts[0].type = vk::DescriptorType::eUniformBuffer;
  typeCounts[0].descriptorCount = 1;

  typeCounts[1].type = vk::DescriptorType::eCombinedImageSampler;
  typeCounts[1].descriptorCount = 2;

  // Create the global descriptor pool
  // All descriptors used in this example are allocated from this pool
  vk::DescriptorPoolCreateInfo descriptorPoolInfo = {};
  descriptorPoolInfo.pNext = nullptr;
  descriptorPoolInfo.poolSizeCount = 2;
  descriptorPoolInfo.pPoolSizes = typeCounts;
  // Set the max. number of descriptor sets that can be requested from this pool
  // (requesting beyond this limit will result in an error)
  descriptorPoolInfo.maxSets = 2;

  CALL_VK(vulkanContext()->device().createDescriptorPool(
      &descriptorPoolInfo, nullptr, &mDescriptorPool));
}

void Engine_CameraHwb::setupDescriptorSetLayout() {
  // Create descriptor set layout
  vk::DescriptorSetLayoutCreateInfo descriptorLayout = {};

  vk::DescriptorSetLayoutBinding layoutBinding[2];
  // Binding 0: Uniform buffer (Vertex shader)
  layoutBinding[0] = {{0},
                      vk::DescriptorType::eUniformBuffer,
                      1,
                      vk::ShaderStageFlagBits::eVertex};

  // Binding 1: Combined Image Sampler (Fragment shader)
  layoutBinding[1] = {{1},
                      vk::DescriptorType::eCombinedImageSampler,
                      1,
                      vk::ShaderStageFlagBits::eFragment};
  auto sampler = mImage->getSamplerHandle();
  layoutBinding[1].pImmutableSamplers = &sampler;

  descriptorLayout.bindingCount = 2;
  descriptorLayout.pBindings = layoutBinding;

  CALL_VK(vulkanContext()->device().createDescriptorSetLayout(
      &descriptorLayout, nullptr, &mDescriptorSetLayout));
  vks::debug::setDescriptorSetLayoutName(
      vulkanContext()->device(), mDescriptorSetLayout, "mDescriptorSetLayout");

  // Create the pipeline layout that is used to generate the rendering pipelines
  // that are based on this descriptor set layout In a more complex scenario you
  // would have different pipeline layouts for different descriptor set layouts
  // that could be reused
  vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
  pPipelineLayoutCreateInfo.setLayoutCount = 1;
  pPipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;

  CALL_VK(vulkanContext()->device().createPipelineLayout(
      &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
}

void Engine_CameraHwb::createDescriptorSet() {
  // Allocate a new descriptor set from the global descriptor pool
  vk::DescriptorSetAllocateInfo allocInfo = {};
  allocInfo.descriptorPool = mDescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &mDescriptorSetLayout;

  CALL_VK(vulkanContext()->device().allocateDescriptorSets(&allocInfo,
                                                           &mDescriptorSet));
  vks::debug::setDescriptorSetName(vulkanContext()->device(), mDescriptorSet,
                                   "mDescriptorSet");
}

void Engine_CameraHwb::updateDescriptorSets() {
  std::vector<vk::WriteDescriptorSet> writeDescriptorSet = {};

  // Binding 0 : Uniform buffer
  auto uboDescriptor = mUniformBuffer->getDescriptor();
  writeDescriptorSet.push_back(vk::WriteDescriptorSet(
      mDescriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
      &uboDescriptor, nullptr));

  // Binding 1 : Combined Image Sampler
  const auto inputImageInfo = mImage->getDescriptor();
  writeDescriptorSet.push_back(vk::WriteDescriptorSet(
      mDescriptorSet, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler,
      &inputImageInfo, nullptr, nullptr));

  mVulkanContext->device().updateDescriptorSets(
      static_cast<uint32_t>(writeDescriptorSet.size()),
      writeDescriptorSet.data(), 0, nullptr);
}

void Engine_CameraHwb::prepareSynchronizationPrimitives() {
  // Semaphores (Used for correct command ordering)
  vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.pNext = nullptr;

  // Semaphore used to ensures that image presentation is complete before
  // starting to submit again
  CALL_VK(vulkanContext()->device().createSemaphore(
      &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore));

  // Semaphore used to ensures that all commands submitted have been finished
  // before submitting the image to the queue
  CALL_VK(vulkanContext()->device().createSemaphore(
      &semaphoreCreateInfo, nullptr, &renderCompleteSemaphore));
}

void Engine_CameraHwb::prepareUniformBuffers() {
  // Prepare and initialize a uniform buffer block containing shader uniforms
  // Single uniforms like in OpenGL are no longer present in Vulkan. All Shader
  // uniforms are passed via uniform buffer blocks
  mUniformBuffer =
      vks::Buffer::create(vulkanContext()->deviceWrapper(), sizeof(uboVS),
                          vk::BufferUsageFlagBits::eUniformBuffer,
                          vk::MemoryPropertyFlagBits::eHostVisible |
                              vk::MemoryPropertyFlagBits::eHostCoherent);

  updateUniformBuffers();
}

void Engine_CameraHwb::updateUniformBuffers() {
  float winRatio = static_cast<float>(mWindow.windowWidth) /
                   static_cast<float>(mWindow.windowHeight);

  uint32_t bmpWidth = mImage->width();
  uint32_t bmpHeight = mImage->height();

  // Pass matrices to the shaders
  uboVS.projectionMatrix = glm::mat4(1.0f);
  uboVS.viewMatrix = glm::mat4(1.0f);

  if (mOrientation % 180 != 0) {
    uint32_t temp = bmpWidth;
    bmpWidth = bmpHeight;
    bmpHeight = temp;
  }

  float bmpRatio = static_cast<float>(bmpWidth) / static_cast<float>(bmpHeight);

  if (bmpRatio >= winRatio) {
    // The bitmap's width is to large and the width is compressed, so we
    // compress the height
    uboVS.modelMatrix =
        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, winRatio / bmpRatio, 1.0f));
  } else {
    // The bitmap's height is to large and the height is compressed, so we
    // compress the width
    uboVS.modelMatrix =
        glm::scale(glm::mat4(1.0f), glm::vec3(bmpRatio / winRatio, 1.0f, 1.0f));
  }

  uboVS.modelMatrix =
      glm::rotate(uboVS.modelMatrix, glm::radians((float)mOrientation),
                  glm::vec3(0.0f, 0.0f, 1.0f));

  mUniformBuffer->map();
  mUniformBuffer->copyFrom(&uboVS, sizeof(uboVS));
  mUniformBuffer->unmap();
}

void Engine_CameraHwb::createPipelines() {

  vk::GraphicsPipelineCreateInfo pipelineCreateInfo = {};
  // The layout used for this pipeline (can be shared among multiple pipelines
  // using the same layout)
  pipelineCreateInfo.layout = mPipelineLayout;
  // Renderpass this pipeline is attached to
  pipelineCreateInfo.renderPass = mRenderPass;

  // Construct the different states making up the pipeline

  // Input assembly state describes how primitives are assembled
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  inputAssemblyState.topology = vk::PrimitiveTopology::eTriangleList;

  // Rasterization state
  vk::PipelineRasterizationStateCreateInfo rasterizationState = {};
  rasterizationState.polygonMode = vk::PolygonMode::eFill;
  rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
  rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;
  rasterizationState.depthClampEnable = VK_FALSE;
  rasterizationState.rasterizerDiscardEnable = VK_FALSE;
  rasterizationState.depthBiasEnable = VK_FALSE;
  rasterizationState.lineWidth = 1.0f;

  // Color blend state describes how blend factors are calculated (if used)
  // We need one blend attachment state per color attachment (even if blending
  // is not used)
  vk::PipelineColorBlendAttachmentState blendAttachmentState[1] = {};
  blendAttachmentState[0].blendEnable = VK_FALSE;
  blendAttachmentState[0].colorWriteMask = vk::ColorComponentFlags{0xf};
  vk::PipelineColorBlendStateCreateInfo colorBlendState = {};
  colorBlendState.attachmentCount = 1;
  colorBlendState.pAttachments = blendAttachmentState;

  // Viewport state sets the number of viewports and scissor used in this
  // pipeline Note: This is actually overridden by the dynamic states (see
  // below)
  vk::PipelineViewportStateCreateInfo viewportState = {};
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  // Enable dynamic states
  // Most states are baked into the pipeline, but there are still a few dynamic
  // states that can be changed within a command buffer To be able to change
  // these we need do specify which dynamic states will be changed using this
  // pipeline. Their actual states are set later on in the command buffer. For
  // this example we will set the viewport and scissor using dynamic states
  std::vector<vk::DynamicState> dynamicStateEnables;
  dynamicStateEnables.push_back(vk::DynamicState::eViewport);
  dynamicStateEnables.push_back(vk::DynamicState::eScissor);
  vk::PipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.pDynamicStates = dynamicStateEnables.data();
  dynamicState.dynamicStateCount =
      static_cast<uint32_t>(dynamicStateEnables.size());

  // Depth and stencil state containing depth and stencil compare and test
  // operations We only use depth tests and want depth tests and writes to be
  // enabled and compare with less or equal
  vk::PipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.depthTestEnable = VK_FALSE;
  depthStencilState.depthWriteEnable = VK_FALSE;
  depthStencilState.depthCompareOp = vk::CompareOp::eLessOrEqual;
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.back.failOp = vk::StencilOp::eKeep;
  depthStencilState.back.passOp = vk::StencilOp::eKeep;
  depthStencilState.back.compareOp = vk::CompareOp::eAlways;
  depthStencilState.stencilTestEnable = VK_FALSE;
  depthStencilState.front = depthStencilState.back;

  // Multi sampling state
  // This example does not make use of multi sampling (for anti-aliasing), the
  // state must still be set and passed to the pipeline
  vk::PipelineMultisampleStateCreateInfo multisampleState = {};
  multisampleState.rasterizationSamples = vk::SampleCountFlagBits::e1;
  multisampleState.pSampleMask = nullptr;

  // Vertex input descriptions
  // Specifies the vertex input parameters for a pipeline

  // Vertex input binding
  // This example uses a single vertex input binding at binding point 0 (see
  // vkCmdBindVertexBuffers)
  vk::VertexInputBindingDescription vertexInputBinding = {};
  vertexInputBinding.binding = 0;
  vertexInputBinding.stride = sizeof(VertexUV);
  vertexInputBinding.inputRate = vk::VertexInputRate::eVertex;

  // Input attribute bindings describe shader attribute locations and memory
  // layouts
  std::array<vk::VertexInputAttributeDescription, 2> vertexInputAttributs;
  // These match the following shader layout (see shader_01_triangle.vert):
  //	layout (location = 0) in vec4 inPos;
  //	layout (location = 1) in vec2 inUVPos;
  // Attribute location 0: Position
  vertexInputAttributs[0].binding = 0;
  vertexInputAttributs[0].location = 0;
  // Position attribute is four 32 bit signed (SFLOAT) floats (R32 G32 B32 A32)
  vertexInputAttributs[0].format = vk::Format::eR32G32B32A32Sfloat;
  vertexInputAttributs[0].offset = offsetof(VertexUV, posX);
  // Attribute location 1: Color
  vertexInputAttributs[1].binding = 0;
  vertexInputAttributs[1].location = 1;
  // Color attribute is two 32 bit signed (SFLOAT) floats (R32 G32)
  vertexInputAttributs[1].format = vk::Format::eR32G32Sfloat;
  vertexInputAttributs[1].offset = offsetof(VertexUV, u);

  // Vertex input state used for pipeline creation
  vk::PipelineVertexInputStateCreateInfo vertexInputState = {};
  vertexInputState.vertexBindingDescriptionCount = 1;
  vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
  vertexInputState.vertexAttributeDescriptionCount = 2;
  vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs.data();

  // Shaders
  std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{};

  // Vertex shader
  shaderStages[0] = loadShader(vertFilePath, vk::ShaderStageFlagBits::eVertex);
  // Fragment shader
  shaderStages[1] =
      loadShader(fragFilePath, vk::ShaderStageFlagBits::eFragment);

  // Set pipeline shader stage info
  pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineCreateInfo.pStages = shaderStages.data();

  // Assign the pipeline states to the pipeline creation info structure
  pipelineCreateInfo.pVertexInputState = &vertexInputState;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
  pipelineCreateInfo.pRasterizationState = &rasterizationState;
  pipelineCreateInfo.pColorBlendState = &colorBlendState;
  pipelineCreateInfo.pMultisampleState = &multisampleState;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pDepthStencilState = &depthStencilState;
  pipelineCreateInfo.renderPass = mRenderPass;
  pipelineCreateInfo.pDynamicState = &dynamicState;

  // Create rendering pipeline using the specified states
  CALL_VK(vulkanContext()->device().createGraphicsPipelines(
      vulkanContext()->pipelineCache(), 1, &pipelineCreateInfo, nullptr,
      &mPipeline));

  // Shader modules are no longer needed once the graphics pipeline has been
  // created
  vulkanContext()->device().destroyShaderModule(shaderStages[0].module,
                                                nullptr);
  vulkanContext()->device().destroyShaderModule(shaderStages[1].module,
                                                nullptr);
}

void Engine_CameraHwb::buildCommandBuffers(int i) {
  // Set clear values for all framebuffer attachments with loadOp set to clear
  // We use two attachments (color and depth) that are cleared at the start of
  // the subpass and as such we need to set clear values for both
  vk::ClearValue clearValues[2];
  clearValues[0].color =
      vk::ClearColorValue(std::array<float, 4>({{0.0f, 0.0f, 0.2f, 1.0f}}));
  clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

  vk::RenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderPass = mRenderPass;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = mWindow.windowWidth;
  renderPassBeginInfo.renderArea.extent.height = mWindow.windowHeight;
  renderPassBeginInfo.clearValueCount = 2;
  renderPassBeginInfo.pClearValues = clearValues;

  // Set target frame buffer
  renderPassBeginInfo.framebuffer = frameBuffers[i];

  vk::CommandBufferBeginInfo cmdBufInfo = {};
  cmdBufInfo.pNext = nullptr;
  CALL_VK(drawCmdBuffers[i].begin(&cmdBufInfo));

  // Start the first sub pass specified in our default prepare pass setup by the
  // base class This will clear the color and depth attachment
  drawCmdBuffers[i].beginRenderPass(&renderPassBeginInfo,
                                    vk::SubpassContents::eInline);

  // Update dynamic viewport state
  vk::Viewport viewport = {};
  viewport.height = (float)mWindow.windowHeight;
  viewport.width = (float)mWindow.windowWidth;
  viewport.minDepth = (float)0.0f;
  viewport.maxDepth = (float)1.0f;
  drawCmdBuffers[i].setViewport(0, 1, &viewport);

  // Update dynamic scissor state
  vk::Rect2D scissor = {};
  scissor.extent.width = mWindow.windowWidth;
  scissor.extent.height = mWindow.windowHeight;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  drawCmdBuffers[i].setScissor(0, 1, &scissor);

  // Bind descriptor sets describing shader binding points
  drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       mPipelineLayout, 0, 1, &mDescriptorSet,
                                       0, nullptr);

  // Bind the rendering pipeline
  // The pipeline (state object) contains all states of the rendering pipeline,
  // binding it will set all the states specified at pipeline creation time
  drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);

  // Bind vertex buffer (contains position and colors)
  vk::DeviceSize offsets[1] = {0};
  auto verticesBuf = mVerticesBuffer->getBufferHandle();
  drawCmdBuffers[i].bindVertexBuffers(0, 1, &verticesBuf, offsets);

  // Draw
  drawCmdBuffers[i].draw(sizeof(g_vb_bitmap_texture_Data) /
                             sizeof(g_vb_bitmap_texture_Data[0]),
                         1, 0, 0);

  drawCmdBuffers[i].endRenderPass();

  // Ending the prepare pass will add an implicit barrier transitioning the
  // frame buffer color attachment to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for
  // presenting it to the windowing system

  drawCmdBuffers[i].end();
}

void Engine_CameraHwb::buildCommandBuffers() {
  vk::CommandBufferBeginInfo cmdBufInfo = {};
  cmdBufInfo.pNext = nullptr;

  // Set clear values for all framebuffer attachments with loadOp set to clear
  // We use two attachments (color and depth) that are cleared at the start of
  // the subpass and as such we need to set clear values for both
  vk::ClearValue clearValues[2];
  clearValues[0].color =
      vk::ClearColorValue(std::array<float, 4>({{0.0f, 0.0f, 0.2f, 1.0f}}));
  clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

  vk::RenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderPass = mRenderPass;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = mWindow.windowWidth;
  renderPassBeginInfo.renderArea.extent.height = mWindow.windowHeight;
  renderPassBeginInfo.clearValueCount = 2;
  renderPassBeginInfo.pClearValues = clearValues;

  for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
    // Set target frame buffer
    renderPassBeginInfo.framebuffer = frameBuffers[i];

    CALL_VK(drawCmdBuffers[i].begin(&cmdBufInfo));

    // Start the first sub pass specified in our default prepare pass setup by
    // the base class This will clear the color and depth attachment
    drawCmdBuffers[i].beginRenderPass(&renderPassBeginInfo,
                                      vk::SubpassContents::eInline);

    // Update dynamic viewport state
    vk::Viewport viewport = {};
    viewport.height = (float)mWindow.windowHeight;
    viewport.width = (float)mWindow.windowWidth;
    viewport.minDepth = (float)0.0f;
    viewport.maxDepth = (float)1.0f;
    drawCmdBuffers[i].setViewport(0, 1, &viewport);

    // Update dynamic scissor state
    vk::Rect2D scissor = {};
    scissor.extent.width = mWindow.windowWidth;
    scissor.extent.height = mWindow.windowHeight;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    drawCmdBuffers[i].setScissor(0, 1, &scissor);

    // Bind descriptor sets describing shader binding points
    drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                         mPipelineLayout, 0, 1, &mDescriptorSet,
                                         0, nullptr);

    // Bind the rendering pipeline
    // The pipeline (state object) contains all states of the rendering
    // pipeline, binding it will set all the states specified at pipeline
    // creation time
    drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);

    // Bind vertex buffer
    vk::DeviceSize offsets[1] = {0};
    auto verticesBuf = mVerticesBuffer->getBufferHandle();
    drawCmdBuffers[i].bindVertexBuffers(0, 1, &verticesBuf, offsets);

    // Draw triangle
    drawCmdBuffers[i].draw(sizeof(g_vb_bitmap_texture_Data) /
                               sizeof(g_vb_bitmap_texture_Data[0]),
                           1, 0, 0);

    drawCmdBuffers[i].endRenderPass();

    // Ending the prepare pass will add an implicit barrier transitioning the
    // frame buffer color attachment to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for
    // presenting it to the windowing system

    drawCmdBuffers[i].end();
  }
}

void Engine_CameraHwb::draw() {
  EngineContext::prepareFrame();

  updateTexture();

  // Use a fence to wait until the command buffer has finished execution before
  // using it again
  CALL_VK(vulkanContext()->device().waitForFences(1, &waitFences[currentBuffer],
                                                  VK_TRUE, UINT64_MAX));

  updateDescriptorSets();

  buildCommandBuffers(currentBuffer);

  CALL_VK(vulkanContext()->device().resetFences(1, &waitFences[currentBuffer]));

  // Pipeline stage at which the queue submission will wait (via
  // pWaitSemaphores)
  vk::PipelineStageFlags waitStageMask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  // The submit info structure specifies a command buffer queue submission batch
  vk::SubmitInfo submitInfo = {};
  submitInfo.pWaitDstStageMask =
      &waitStageMask; // Pointer to the list of pipeline stages that
  // the semaphore waits will occur at
  submitInfo.pWaitSemaphores =
      &presentCompleteSemaphore; // Semaphore(s) to wait upon before the
                                 // submitted
  // command buffer starts executing
  submitInfo.waitSemaphoreCount = 1; // One wait semaphore
  submitInfo.pSignalSemaphores =
      &renderCompleteSemaphore;        // Semaphore(s) to be signaled when
                                       // command buffers have completed
  submitInfo.signalSemaphoreCount = 1; // One signal semaphore
  submitInfo.pCommandBuffers =
      &drawCmdBuffers[currentBuffer]; // Command buffers(s) to execute
                                      // in this batch (submission)
  submitInfo.commandBufferCount = 1;  // One command buffer

  // Submit to the graphics queue passing a wait fence
  CALL_VK(vulkanContext()->queue().submit(1, &submitInfo,
                                          waitFences[currentBuffer]));

  EngineContext::submitFrame();
}

Engine_CameraHwb::~Engine_CameraHwb() {
  if (mPipeline) {
    vulkanContext()->device().destroyPipeline(mPipeline);
  }

  if (mDescriptorPool) {
    vulkanContext()->device().destroyDescriptorPool(mDescriptorPool);
  }

  if (mDescriptorSetLayout) {
    vulkanContext()->device().destroyDescriptorSetLayout(mDescriptorSetLayout);
  }
}