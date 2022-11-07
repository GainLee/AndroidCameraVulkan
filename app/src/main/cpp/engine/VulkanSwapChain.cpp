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

#include "VulkanSwapChain.h"

void VulkanSwapChain::initSurface(ANativeWindow *window) {
  vk::Result err = vk::Result::eSuccess;

  // Create the os-specific surface
  vk::AndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
  surfaceCreateInfo.window = window;
  err = instance.createAndroidSurfaceKHR(&surfaceCreateInfo, NULL, &surface);

  if (err != vk::Result::eSuccess) {
    LOGCATE("Could not create surface!", err);
  }

  // Get available queue family properties
  uint32_t queueCount;
  physicalDevice.getQueueFamilyProperties(&queueCount, NULL);
  assert(queueCount >= 1);

  std::vector<vk::QueueFamilyProperties> queueProps(queueCount);
  physicalDevice.getQueueFamilyProperties(&queueCount, queueProps.data());

  // Iterate over each queue to learn whether it supports presenting:
  // Find a queue with present support
  // Will be used to present the swap chain images to the windowing system
  std::vector<vk::Bool32> supportsPresent(queueCount);
  for (uint32_t i = 0; i < queueCount; i++) {
    physicalDevice.getSurfaceSupportKHR(i, surface, &supportsPresent[i]);
  }

  // Search for a graphics and a present queue in the array of queue
  // families, try to find one that supports both
  uint32_t graphicsQueueNodeIndex = UINT32_MAX;
  uint32_t presentQueueNodeIndex = UINT32_MAX;
  for (uint32_t i = 0; i < queueCount; i++) {
    if (queueProps[i].queueFlags & vk::QueueFlagBits::eGraphics) {
      if (graphicsQueueNodeIndex == UINT32_MAX) {
        graphicsQueueNodeIndex = i;
      }

      if (supportsPresent[i] == VK_TRUE) {
        graphicsQueueNodeIndex = i;
        presentQueueNodeIndex = i;
        break;
      }
    }
  }

  if (presentQueueNodeIndex == UINT32_MAX) {
    // If there's no queue that supports both present and graphics
    // try to find a separate present queue
    for (uint32_t i = 0; i < queueCount; ++i) {
      if (supportsPresent[i] == VK_TRUE) {
        presentQueueNodeIndex = i;
        break;
      }
    }
  }

  // Exit if either a graphics or a presenting queue hasn't been found
  if (graphicsQueueNodeIndex == UINT32_MAX ||
      presentQueueNodeIndex == UINT32_MAX) {
    LOGCATE("Could not find a graphics and/or presenting queue!", -1);
  }

  // todo : Add support for separate graphics and presenting queue
  if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
    LOGCATE("Separate graphics and presenting queues are not supported yet!",
            -1);
  }

  queueNodeIndex = graphicsQueueNodeIndex;

  // Get list of supported surface formats
  uint32_t formatCount;
  CALL_VK(physicalDevice.getSurfaceFormatsKHR(surface, &formatCount, NULL));
  assert(formatCount > 0);

  std::vector<vk::SurfaceFormatKHR> surfaceFormats(formatCount);
  CALL_VK(physicalDevice.getSurfaceFormatsKHR(surface, &formatCount,
                                              surfaceFormats.data()));

  // If the surface format list only includes one entry with
  // VK_FORMAT_UNDEFINED, there is no preferred format, so we assume
  // VK_FORMAT_R8G8B8A8_UNORM
  if ((formatCount == 1) &&
      (surfaceFormats[0].format == vk::Format::eUndefined)) {
    colorFormat = vk::Format::eR8G8B8A8Unorm;
    colorSpace = surfaceFormats[0].colorSpace;
  } else {
    // iterate over the list of available surface format and
    // check for the presence of VK_FORMAT_R8G8B8A8_UNORM
    bool found_R8G8B8A8_UNORM = false;
    for (auto &&surfaceFormat : surfaceFormats) {
      if (surfaceFormat.format == vk::Format::eR8G8B8A8Unorm) {
        colorFormat = surfaceFormat.format;
        colorSpace = surfaceFormat.colorSpace;
        found_R8G8B8A8_UNORM = true;
        break;
      }
    }

    // in case VK_FORMAT_R8G8B8A8_UNORM is not available
    // select the first available color format
    if (!found_R8G8B8A8_UNORM) {
      colorFormat = surfaceFormats[0].format;
      colorSpace = surfaceFormats[0].colorSpace;
    }
  }
}

/**
 * Set instance, physical and logical device to use for the swapchain
 *
 * @param instance Vulkan instance to use
 * @param physicalDevice Physical device used to query properties and formats
 * relevant to the swapchain
 * @param device Logical representation of the device to create the swapchain
 * for
 *
 */
void VulkanSwapChain::connect(vk::Instance instance,
                              vk::PhysicalDevice physicalDevice,
                              vk::Device device) {
  this->instance = instance;
  this->physicalDevice = physicalDevice;
  this->device = device;
}

/**
 * Create the swapchain and get its images with given width and height
 *
 * @param width Pointer to the width of the swapchain (may be adjusted to fit
 * the requirements of the swapchain)
 * @param height Pointer to the height of the swapchain (may be adjusted to fit
 * the requirements of the swapchain)
 * @param vsync (Optional) Can be used to force vsync-ed rendering (by using
 * VK_PRESENT_MODE_FIFO_KHR as presentation mode)
 */
void VulkanSwapChain::create(int32_t *width, int32_t *height, bool vsync) {
  // Store the current swap chain handle so we can use it later on to ease up
  // recreation
  vk::SwapchainKHR oldSwapchain = swapChain;

  // Get physical device surface properties and formats
  vk::SurfaceCapabilitiesKHR surfCaps;
  CALL_VK(physicalDevice.getSurfaceCapabilitiesKHR(surface, &surfCaps));

  // Get available present modes
  uint32_t presentModeCount;
  CALL_VK(physicalDevice.getSurfacePresentModesKHR(surface, &presentModeCount,
                                                   NULL));
  assert(presentModeCount > 0);

  std::vector<vk::PresentModeKHR> presentModes(presentModeCount);
  CALL_VK(physicalDevice.getSurfacePresentModesKHR(surface, &presentModeCount,
                                                   presentModes.data()));

  vk::Extent2D swapchainExtent = {};
  // If width (and height) equals the special value 0xFFFFFFFF, the size of the
  // surface will be set by the swapchain
  if (surfCaps.currentExtent.width == (uint32_t)-1) {
    // If the surface size is undefined, the size is set to
    // the size of the images requested.
    swapchainExtent.width = *width;
    swapchainExtent.height = *height;
  } else {
    // If the surface size is defined, the swap chain size must match
    swapchainExtent = surfCaps.currentExtent;
    *width = surfCaps.currentExtent.width;
    *height = surfCaps.currentExtent.height;
  }

  // Select a present mode for the swapchain

  // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
  // This mode waits for the vertical blank ("v-sync")
  vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

  // If v-sync is not requested, try to find a mailbox mode
  // It's the lowest latency non-tearing present mode available
  if (!vsync) {
    for (size_t i = 0; i < presentModeCount; i++) {
      if (presentModes[i] == vk::PresentModeKHR::eMailbox) {
        swapchainPresentMode = vk::PresentModeKHR::eMailbox;
        break;
      }
      if (presentModes[i] == vk::PresentModeKHR::eImmediate) {
        swapchainPresentMode = vk::PresentModeKHR::eImmediate;
      }
    }
  }

  // Determine the number of images
  uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
  if ((surfCaps.maxImageCount > 0) &&
      (desiredNumberOfSwapchainImages > surfCaps.maxImageCount)) {
    desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
  }

  // Find the transformation of the surface
  vk::SurfaceTransformFlagBitsKHR preTransform;
  if (surfCaps.supportedTransforms &
      vk::SurfaceTransformFlagBitsKHR::eIdentity) {
    // We prefer a non-rotated transform
    preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
  } else {
    preTransform = surfCaps.currentTransform;
  }

  // Find a supported composite alpha format (not all devices support alpha
  // opaque)
  vk::CompositeAlphaFlagBitsKHR compositeAlpha =
      vk::CompositeAlphaFlagBitsKHR::eOpaque;
  // Simply select the first composite alpha format available
  std::vector<vk::CompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
      vk::CompositeAlphaFlagBitsKHR::eOpaque,
      vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
      vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
      vk::CompositeAlphaFlagBitsKHR::eInherit,
  };
  for (auto &compositeAlphaFlag : compositeAlphaFlags) {
    if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
      compositeAlpha = compositeAlphaFlag;
      break;
    };
  }

  vk::Extent2D imageExtent = {swapchainExtent.width, swapchainExtent.height};
  vk::SwapchainCreateInfoKHR swapchainCI = {};
  swapchainCI.surface = surface;
  swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
  swapchainCI.imageFormat = colorFormat;
  swapchainCI.imageColorSpace = colorSpace;
  swapchainCI.imageExtent = imageExtent;
  swapchainCI.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  swapchainCI.preTransform = preTransform;
  swapchainCI.imageArrayLayers = 1;
  swapchainCI.imageSharingMode = vk::SharingMode::eExclusive;
  swapchainCI.queueFamilyIndexCount = 0;
  swapchainCI.presentMode = swapchainPresentMode;
  // Setting oldSwapChain to the saved handle of the previous swapchain aids in
  // resource reuse and makes sure that we can still present already acquired
  // images
  swapchainCI.oldSwapchain = oldSwapchain;
  // Setting clipped to VK_TRUE allows the implementation to discard rendering
  // outside of the surface area
  swapchainCI.clipped = VK_TRUE;
  swapchainCI.compositeAlpha = compositeAlpha;

  // Enable transfer source on swap chain images if supported
  if (surfCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) {
    swapchainCI.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
  }

  // Enable transfer destination on swap chain images if supported
  if (surfCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst) {
    swapchainCI.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
  }

  CALL_VK(device.createSwapchainKHR(&swapchainCI, nullptr, &swapChain));

  // If an existing swap chain is re-created, destroy the old swap chain
  // This also cleans up all the presentable images
  if (oldSwapchain) {
    for (uint32_t i = 0; i < imageCount; i++) {
      device.destroyImageView(buffers[i].view, nullptr);
    }
    device.destroySwapchainKHR(oldSwapchain, nullptr);
  }
  CALL_VK(device.getSwapchainImagesKHR(swapChain, &imageCount, NULL));

  // Get the swap chain images
  images.resize(imageCount);
  CALL_VK(device.getSwapchainImagesKHR(swapChain, &imageCount, images.data()));

  // Get the swap chain buffers containing the image and imageview
  buffers.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; i++) {
    vk::ImageViewCreateInfo colorAttachmentView = {};
    colorAttachmentView.pNext = NULL;
    colorAttachmentView.format = colorFormat;
    colorAttachmentView.components = {
        vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
        vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    colorAttachmentView.subresourceRange.aspectMask =
        vk::ImageAspectFlagBits::eColor;
    colorAttachmentView.subresourceRange.baseMipLevel = 0;
    colorAttachmentView.subresourceRange.levelCount = 1;
    colorAttachmentView.subresourceRange.baseArrayLayer = 0;
    colorAttachmentView.subresourceRange.layerCount = 1;
    colorAttachmentView.viewType = vk::ImageViewType::e2D;

    buffers[i].image = images[i];

    colorAttachmentView.image = buffers[i].image;

    CALL_VK(device.createImageView(&colorAttachmentView, nullptr,
                                   &buffers[i].view));
  }
}

/**
 * Acquires the next image in the swap chain
 *
 * @param presentCompleteSemaphore (Optional) Semaphore that is signaled when
 * the image is ready for use
 * @param imageIndex Pointer to the image index that will be increased if the
 * next image could be acquired
 *
 * @note The function will always wait until the next image has been acquired by
 * setting timeout to UINT64_MAX
 *
 * @return vk::Result of the image acquisition
 */
vk::Result
VulkanSwapChain::acquireNextImage(vk::Semaphore presentCompleteSemaphore,
                                  uint32_t *imageIndex) {
  // By setting timeout to UINT64_MAX we will always wait until the next image
  // has been acquired or an actual error is thrown With that we don't have to
  // handle VK_NOT_READY
  return device.acquireNextImageKHR(swapChain, UINT64_MAX,
                                    presentCompleteSemaphore,
                                    (vk::Fence) nullptr, imageIndex);
}

/**
 * Queue an image for presentation
 *
 * @param queue Presentation queue for presenting the image
 * @param imageIndex Index of the swapchain image to queue for presentation
 * @param waitSemaphore (Optional) Semaphore that is waited on before the image
 * is presented (only used if != VK_NULL_HANDLE)
 *
 * @return vk::Result of the queue presentation
 */
vk::Result VulkanSwapChain::queuePresent(vk::Queue queue, uint32_t imageIndex,
                                         vk::Semaphore waitSemaphore) {
  vk::PresentInfoKHR presentInfo = {};
  presentInfo.pNext = NULL;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapChain;
  presentInfo.pImageIndices = &imageIndex;
  // Check if a wait semaphore has been specified to wait for before presenting
  // the image
  if (waitSemaphore) {
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
  }
  return queue.presentKHR(&presentInfo);
}

/**
 * Destroy and free Vulkan resources used for the swapchain
 */
void VulkanSwapChain::cleanup() {
  if (swapChain) {
    for (uint32_t i = 0; i < imageCount; i++) {
      device.destroyImageView(buffers[i].view, nullptr);
    }
  }
  if (surface) {
    device.destroySwapchainKHR(swapChain, nullptr);
    instance.destroySurfaceKHR(surface, nullptr);
  }
  surface = nullptr;
  swapChain = nullptr;
}

#if defined(_DIRECT2DISPLAY)
/**
 * Create direct to display surface
 */
void VulkanSwapChain::createDirect2DisplaySurface(uint32_t width,
                                                  uint32_t height) {
  uint32_t displayPropertyCount;

  // Get display property
  vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropertyCount,
                                          NULL);
  vk::DisplayPropertiesKHR *pDisplayProperties =
      new vk::DisplayPropertiesKHR[displayPropertyCount];
  vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropertyCount,
                                          pDisplayProperties);

  // Get plane property
  uint32_t planePropertyCount;
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice,
                                               &planePropertyCount, NULL);
  vk::DisplayPlanePropertiesKHR *pPlaneProperties =
      new vk::DisplayPlanePropertiesKHR[planePropertyCount];
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
      physicalDevice, &planePropertyCount, pPlaneProperties);

  vk::DisplayKHR display = VK_NULL_HANDLE;
  vk::DisplayModeKHR displayMode;
  vk::DisplayModePropertiesKHR *pModeProperties;
  bool foundMode = false;

  for (uint32_t i = 0; i < displayPropertyCount; ++i) {
    display = pDisplayProperties[i].display;
    uint32_t modeCount;
    vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, NULL);
    pModeProperties = new vk::DisplayModePropertiesKHR[modeCount];
    vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount,
                                  pModeProperties);

    for (uint32_t j = 0; j < modeCount; ++j) {
      const vk::DisplayModePropertiesKHR *mode = &pModeProperties[j];

      if (mode->parameters.visibleRegion.width == width &&
          mode->parameters.visibleRegion.height == height) {
        displayMode = mode->displayMode;
        foundMode = true;
        break;
      }
    }
    if (foundMode) {
      break;
    }
    delete[] pModeProperties;
  }

  if (!foundMode) {
    vks::tools::exitFatal("Can't find a display and a display mode!", -1);
    return;
  }

  // Search for a best plane we can use
  uint32_t bestPlaneIndex = UINT32_MAX;
  vk::DisplayKHR *pDisplays = NULL;
  for (uint32_t i = 0; i < planePropertyCount; i++) {
    uint32_t planeIndex = i;
    uint32_t displayCount;
    vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex,
                                          &displayCount, NULL);
    if (pDisplays) {
      delete[] pDisplays;
    }
    pDisplays = new vk::DisplayKHR[displayCount];
    vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex,
                                          &displayCount, pDisplays);

    // Find a display that matches the current plane
    bestPlaneIndex = UINT32_MAX;
    for (uint32_t j = 0; j < displayCount; j++) {
      if (display == pDisplays[j]) {
        bestPlaneIndex = i;
        break;
      }
    }
    if (bestPlaneIndex != UINT32_MAX) {
      break;
    }
  }

  if (bestPlaneIndex == UINT32_MAX) {
    vks::tools::exitFatal("Can't find a plane for displaying!", -1);
    return;
  }

  vk::DisplayPlaneCapabilitiesKHR planeCap;
  vkGetDisplayPlaneCapabilitiesKHR(physicalDevice, displayMode, bestPlaneIndex,
                                   &planeCap);
  vk::DisplayPlaneAlphaFlagBitsKHR alphaMode =
      (vk::DisplayPlaneAlphaFlagBitsKHR)0;

  if (planeCap.supportedAlpha &
      VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
  } else if (planeCap.supportedAlpha &
             VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
  } else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
  } else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
  }

  vk::DisplaySurfaceCreateInfoKHR surfaceInfo{};
  surfaceInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
  surfaceInfo.pNext = NULL;
  surfaceInfo.flags = 0;
  surfaceInfo.displayMode = displayMode;
  surfaceInfo.planeIndex = bestPlaneIndex;
  surfaceInfo.planeStackIndex =
      pPlaneProperties[bestPlaneIndex].currentStackIndex;
  surfaceInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  surfaceInfo.globalAlpha = 1.0;
  surfaceInfo.alphaMode = alphaMode;
  surfaceInfo.imageExtent.width = width;
  surfaceInfo.imageExtent.height = height;

  vk::Result result =
      vkCreateDisplayPlaneSurfaceKHR(instance, &surfaceInfo, NULL, &surface);
  if (result != vk::Result::eSuccess) {
    vks::tools::exitFatal("Failed to create surface!", result);
  }

  delete[] pDisplays;
  delete[] pModeProperties;
  delete[] pDisplayProperties;
  delete[] pPlaneProperties;
}
#endif
