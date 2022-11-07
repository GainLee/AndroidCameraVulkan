//
// Created by 李根 on 2022/10/10.
//

#include "EngineContext.h"

void EngineContext::initSyncObjects() {
  presentCompleteSemaphore =
      vulkanContext()->device().createSemaphore(vk::SemaphoreCreateInfo{});
  renderCompleteSemaphore =
      vulkanContext()->device().createSemaphore(vk::SemaphoreCreateInfo{});
}

bool EngineContext::createSemaphore(vk::Semaphore *semaphore) const {
  if (semaphore == nullptr)
    return false;
  const vk::SemaphoreCreateInfo semaphoreCreateInfo{};
  CALL_VK(vulkanContext()->device().createSemaphore(&semaphoreCreateInfo,
                                                    nullptr, semaphore));
  return true;
}

void EngineContext::connectSwapChain() {
  mSwapChain.connect(vulkanContext()->instance(),
                     vulkanContext()->physicalDevice(),
                     vulkanContext()->device());
}

void EngineContext::setNativeWindow(ANativeWindow *window, uint32_t width,
                                    uint32_t height) {
  mWindow.nativeWindow = window;
  mWindow.windowWidth = width;
  mWindow.windowHeight = height;

  mCamera.setPerspective(
      45.0f, (float)mWindow.windowWidth / (float)mWindow.windowHeight, 0.1f,
      256.0f);
}

void EngineContext::windowResize() {
  if (!mPrepared) {
    return;
  }
  mPrepared = false;

  // Ensure all operations on the device have been finished before destroying
  // resources
  vulkanContext()->device().waitIdle();

  // Recreate swap chain
  setupSwapChain();

  // Recreate the frame buffers
  if (settings.uesDepth) {
    vulkanContext()->device().destroyImageView(depthStencil.view, nullptr);
    vulkanContext()->device().destroyImage(depthStencil.image, nullptr);
    vulkanContext()->device().freeMemory(depthStencil.mem, nullptr);
    setupDepthStencil();
  }

  for (uint32_t i = 0; i < frameBuffers.size(); i++) {
    vulkanContext()->device().destroyFramebuffer(frameBuffers[i], nullptr);
  }
  setupFrameBuffer();

  // Command buffers need to be recreated as they may store
  // references to the recreated frame buffer
  vulkanContext()->device().freeCommandBuffers(vulkanContext()->commandPool(),
                                               drawCmdBuffers.size(),
                                               drawCmdBuffers.data());
  drawCmdBuffers.clear();

  createCommandBuffers();

  vulkanContext()->device().waitIdle();

  mPrepared = true;
}

void EngineContext::createPipelines() {}

void EngineContext::prepareUniformBuffers() {}

void EngineContext::createDescriptorSet() {}

void EngineContext::buildCommandBuffers() {}

void EngineContext::draw() {}

void EngineContext::prepareVertices(bool useStagingBuffers, const void *data,
                                    size_t bufSize) {
  // Setup vertices

  uint32_t vertexBufferSize = bufSize;

  if (useStagingBuffers) {
    // Static data like vertex and index buffer should be stored on the device
    // memory for optimal (and fastest) access by the GPU
    //
    // To achieve this we use so-called "staging buffers" :
    // - Create a buffer that's visible to the host (and can be mapped)
    // - Copy the data to this buffer
    // - Create another buffer that's local on the device (VRAM) with the same
    // size
    // - Copy the data from the host to the device using a command buffer
    // - Delete the host visible (staging) buffer
    // - Use the device local buffers for rendering

    auto stagingBuffers =
        vks::Buffer::create(vulkanContext()->deviceWrapper(), vertexBufferSize,
                            vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible |
                                vk::MemoryPropertyFlagBits::eHostCoherent);
    stagingBuffers->map();
    stagingBuffers->copyFrom(data, vertexBufferSize);
    stagingBuffers->unmap();

    vks::debug::setDeviceMemoryName(
        vulkanContext()->device(), stagingBuffers->getMemoryHandle(),
        "EngineContext-prepareVertices-stagingBuffers");

    mVerticesBuffer =
        vks::Buffer::create(vulkanContext()->deviceWrapper(), vertexBufferSize,
                            vk::BufferUsageFlagBits::eVertexBuffer |
                                vk::BufferUsageFlagBits::eTransferDst,
                            vk::MemoryPropertyFlagBits::eDeviceLocal);

    vks::debug::setDeviceMemoryName(
        vulkanContext()->device(), mVerticesBuffer->getMemoryHandle(),
        "EngineContext-prepareVertices-mVerticesBuffer");

    // Buffer copies have to be submitted to a queue, so we need a command
    // buffer for them Note: Some devices offer a dedicated transfer queue (with
    // only the transfer bit set) that may be faster when doing lots of copies
    vk::CommandBuffer copyCmd;
    assert(vulkanContext()->deviceWrapper()->beginSingleTimeCommand(&copyCmd));

    // Put buffer region copies into command buffer
    vk::BufferCopy copyRegion = {};

    // Vertex buffer
    copyRegion.size = vertexBufferSize;
    copyCmd.copyBuffer(stagingBuffers->getBufferHandle(),
                       mVerticesBuffer->getBufferHandle(), 1, &copyRegion);

    vulkanContext()->deviceWrapper()->endAndSubmitSingleTimeCommand(
        copyCmd, vulkanContext()->queue());
  } else {
    // Don't use staging
    // Create host-visible buffers only and use these for rendering. This is not
    // advised and will usually result in lower rendering performance

    mVerticesBuffer =
        vks::Buffer::create(vulkanContext()->deviceWrapper(), vertexBufferSize,
                            vk::BufferUsageFlagBits::eVertexBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible |
                                vk::MemoryPropertyFlagBits::eHostCoherent);
    mVerticesBuffer->map();
    mVerticesBuffer->copyFrom(data, vertexBufferSize);
    mVerticesBuffer->unmap();

    vks::debug::setDeviceMemoryName(
        vulkanContext()->device(), mVerticesBuffer->getMemoryHandle(),
        "EngineContext-prepareVertices-mVerticesBuffer-no-staging");
  }
}

void EngineContext::setupRenderPass() {
  std::vector<vk::AttachmentDescription> attachments;
  if (!settings.uesDepth) {
    attachments.resize(1);
    // Color attachment
    attachments[0].format = mSwapChain.colorFormat;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
  } else {
    attachments.resize(2);
    // Color attachment
    attachments[0].format = mSwapChain.colorFormat;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eStore;
    attachments[1].initialLayout = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout =
        vk::ImageLayout::eDepthStencilAttachmentOptimal;
  }

  vk::AttachmentReference colorReference = {};
  colorReference.attachment = 0;
  colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::SubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorReference;
  subpassDescription.pDepthStencilAttachment = nullptr;
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pInputAttachments = nullptr;
  subpassDescription.preserveAttachmentCount = 0;
  subpassDescription.pPreserveAttachments = nullptr;
  subpassDescription.pResolveAttachments = nullptr;

  if (settings.uesDepth) {
    vk::AttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    subpassDescription.pDepthStencilAttachment = &depthReference;
  }

  vk::SubpassDependency subpass_dependency = {};
  subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependency.dstSubpass = 0;
  subpass_dependency.srcStageMask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpass_dependency.dstStageMask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpass_dependency.srcAccessMask = vk::AccessFlagBits::eNone;
  subpass_dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  vk::RenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &subpass_dependency;

  CALL_VK(vulkanContext()->device().createRenderPass(&renderPassInfo, nullptr,
                                                     &mRenderPass));
}

vk::PipelineShaderStageCreateInfo
EngineContext::loadShader(const char *shaderFilePath,
                          vk::ShaderStageFlagBits stage) {
  // Read shader file from asset.
  AAsset *shaderFile = AAssetManager_open(vulkanContext()->_AAssetManager(),
                                          shaderFilePath, AASSET_MODE_BUFFER);
  assert(shaderFile != nullptr);
  const size_t shaderSize = AAsset_getLength(shaderFile);
  std::vector<char> shader(shaderSize);
  int status = AAsset_read(shaderFile, shader.data(), shaderSize);
  AAsset_close(shaderFile);
  assert(status >= 0);

  // Create shader module.
  const vk::ShaderModuleCreateInfo shaderDesc{
      {}, shaderSize, reinterpret_cast<const uint32_t *>(shader.data())};
  vk::ShaderModule shaderModule;
  CALL_VK(vulkanContext()->device().createShaderModule(&shaderDesc, nullptr,
                                                       &shaderModule));

  vk::PipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.stage = stage;
  shaderStage.pName = "main";
  shaderStage.module = shaderModule;
  return shaderStage;
}

void EngineContext::initSwapchain() {
#if defined(_WIN32)
  swapChain.initSurface(windowInstance, window);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
  mSwapChain.initSurface(mWindow.nativeWindow);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
  swapChain.initSurface(view);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
  swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
  swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
  swapChain.initSurface(connection, window);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
  swapChain.initSurface(width, height);
#endif
}

void EngineContext::setupSwapChain() {
  mSwapChain.create(&mWindow.windowWidth, &mWindow.windowHeight);
}

void EngineContext::createCommandBuffers() {
  // Create one command buffer for each swap chain image and reuse for rendering
  for (int i = 0; i < mSwapChain.imageCount; ++i) {
    vk::CommandBuffer cmdBuffer;
    vk::CommandBufferAllocateInfo cmdBufAllocateInfo{};
    cmdBufAllocateInfo.commandPool =
        vulkanContext()->deviceWrapper()->commandPool;
    cmdBufAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    cmdBufAllocateInfo.commandBufferCount = 1;
    CALL_VK(vulkanContext()->device().allocateCommandBuffers(
        &cmdBufAllocateInfo, &cmdBuffer));

    drawCmdBuffers.push_back(cmdBuffer);
  }
}

void EngineContext::createSynchronizationPrimitives() {
  // Wait fences to sync command buffer access
  vk::FenceCreateInfo fenceCreateInfo{};
  fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

  waitFences.resize(drawCmdBuffers.size());
  for (auto &fence : waitFences) {
    CALL_VK(vulkanContext()->device().createFence(&fenceCreateInfo, nullptr,
                                                  &fence));
  }
}

void EngineContext::setupDepthStencil() {
  vk::ImageCreateInfo imageCI{};
  imageCI.imageType = vk::ImageType::e2D;
  imageCI.format = depthFormat;
  imageCI.extent.width = mWindow.windowWidth;
  imageCI.extent.height = mWindow.windowHeight;
  imageCI.extent.depth = 1;
  imageCI.mipLevels = 1;
  imageCI.arrayLayers = 1;
  imageCI.samples = vk::SampleCountFlagBits::e1;
  imageCI.tiling = vk::ImageTiling::eOptimal;
  imageCI.initialLayout = vk::ImageLayout::eUndefined;
  imageCI.queueFamilyIndexCount = 0;
  imageCI.pQueueFamilyIndices = NULL;
  imageCI.sharingMode = vk::SharingMode::eExclusive;
  imageCI.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

  CALL_VK(vulkanContext()->device().createImage(&imageCI, nullptr,
                                                &depthStencil.image));
  vk::MemoryRequirements memReqs{};
  vulkanContext()->device().getImageMemoryRequirements(depthStencil.image,
                                                       &memReqs);

  vk::MemoryAllocateInfo memAllloc{};
  memAllloc.allocationSize = memReqs.size;
  uint32_t memTypeIdx = vulkanContext()->deviceWrapper()->getMemoryType(
      memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
  memAllloc.memoryTypeIndex = memTypeIdx;
  CALL_VK(vulkanContext()->device().allocateMemory(&memAllloc, nullptr,
                                                   &depthStencil.mem));
  vulkanContext()->device().bindImageMemory(depthStencil.image,
                                            depthStencil.mem, 0);

  vk::ImageViewCreateInfo imageViewCI{};
  imageViewCI.viewType = vk::ImageViewType::e2D;
  imageViewCI.image = depthStencil.image;
  imageViewCI.format = depthFormat;
  imageViewCI.subresourceRange.baseMipLevel = 0;
  imageViewCI.subresourceRange.levelCount = 1;
  imageViewCI.subresourceRange.baseArrayLayer = 0;
  imageViewCI.subresourceRange.layerCount = 1;
  imageViewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
  imageViewCI.components.r = vk::ComponentSwizzle::eR;
  imageViewCI.components.g = vk::ComponentSwizzle::eG;
  imageViewCI.components.b = vk::ComponentSwizzle::eB;
  imageViewCI.components.a = vk::ComponentSwizzle::eA;
  // Stencil aspect should only be set on depth + stencil formats
  // (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
  if (depthFormat >= vk::Format::eD16UnormS8Uint) {
    imageViewCI.subresourceRange.aspectMask |=
        vk::ImageAspectFlagBits::eStencil;
  }
  CALL_VK(vulkanContext()->device().createImageView(&imageViewCI, nullptr,
                                                    &depthStencil.view));
}

void EngineContext::setupFrameBuffer() {
  std::vector<vk::ImageView> attachments;

  if (settings.uesDepth) {
    attachments.resize(2);
    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencil.view;
  } else {
    attachments.resize(1);
  }

  vk::FramebufferCreateInfo frameBufferCreateInfo = {};
  frameBufferCreateInfo.pNext = NULL;
  frameBufferCreateInfo.renderPass = mRenderPass;
  frameBufferCreateInfo.attachmentCount = attachments.size();
  frameBufferCreateInfo.pAttachments = attachments.data();
  frameBufferCreateInfo.width = mWindow.windowWidth;
  frameBufferCreateInfo.height = mWindow.windowHeight;
  frameBufferCreateInfo.layers = 1;

  // Create frame buffers for every swap chain image
  frameBuffers.resize(mSwapChain.imageCount);
  for (uint32_t i = 0; i < frameBuffers.size(); i++) {
    attachments[0] = mSwapChain.buffers[i].view;
    CALL_VK(vulkanContext()->device().createFramebuffer(
        &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
  }
}

void EngineContext::prepare(JNIEnv *env) {
  if (settings.uesDepth) {
    vulkanContext()->deviceWrapper()->getDepthFormat(depthFormat);
  }

  initSwapchain();
  setupSwapChain();
  createCommandBuffers();
  createSynchronizationPrimitives();

  if (settings.uesDepth) {
    setupDepthStencil();
  }

  setupRenderPass();
  setupFrameBuffer();
}

void EngineContext::prepareFrame() {
  CALL_VK(
      mSwapChain.acquireNextImage(presentCompleteSemaphore, &currentBuffer));
}

void EngineContext::submitFrame() {
  // Present the current buffer to the swap chain
  // Pass the semaphore signaled by the command buffer submission from the
  // submit info as the wait semaphore for swap chain presentation This ensures
  // that the image is not presented to the windowing system until all commands
  // have been submitted
  vk::Result present = mSwapChain.queuePresent(
      vulkanContext()->queue(), currentBuffer, renderCompleteSemaphore);
  if (!((present == vk::Result::eSuccess) ||
        (present == vk::Result::eSuboptimalKHR))) {
    CALL_VK(present);
  }
}

EngineContext::~EngineContext() {
  vulkanContext()->device().waitIdle();

  if (presentCompleteSemaphore) {
    vulkanContext()->device().destroySemaphore(presentCompleteSemaphore);
  }

  if (renderCompleteSemaphore) {
    vulkanContext()->device().destroySemaphore(renderCompleteSemaphore);
  }

  for (auto &fence : waitFences) {
    vulkanContext()->device().destroyFence(fence);
  }

  vulkanContext()->device().freeCommandBuffers(vulkanContext()->commandPool(),
                                               drawCmdBuffers.size(),
                                               drawCmdBuffers.data());

  mSwapChain.cleanup();
}