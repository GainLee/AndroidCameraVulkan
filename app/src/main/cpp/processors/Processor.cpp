/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Gain
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

#include "Processor.h"
#include "../util/LogUtil.h"
#include "Engine_CameraHwb.h"
#include "includes/cube_data.h"
#include "jni.h"
#include <VulkanContext.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <chrono>
#include <thread>

std::unique_ptr<Processor> Processor::create(AAssetManager *assetManager) {
  auto sample = std::make_unique<Processor>();
  sample->initialize(assetManager);
  return std::move(sample);
}

Processor::Processor() {}

void Processor::configEngine(uint32_t type) {
  switch (type) {
  case EngineType::CAMERA_HARDWAREBUFFER: {
    mEngineContext = std::make_unique<Engine_CameraHwb>(mVulkanContext);
    break;
  }
  case EngineType::HWB_TO_NV21: {
    //			mEngineContext =
    //std::make_unique<Engine_HwbToNV21>(mVulkanContext);
    break;
  }
  }
}

void Processor::initialize(AAssetManager *assetManager) {
  mVulkanContext = std::make_shared<VulkanContext>();

  const bool success = mVulkanContext->create(assetManager);
  assert(success);
}

void Processor::setWindow(ANativeWindow *window, uint32_t w, uint32_t h) {
  // init swapchain
  mEngineContext->connectSwapChain();
  mEngineContext->setNativeWindow(window, w, h);
}

void Processor::onWindowSizeChanged(ANativeWindow *window, uint32_t w,
                                    uint32_t h) {
  // Recreate swap chain
  mEngineContext->setNativeWindow(window, w, h);
  mEngineContext->windowResize();
}

void Processor::prepareHardwareBuffer(JNIEnv *env, AHardwareBuffer *buffer,
                                      int orientation) {
  Engine_CameraHwb *context =
      dynamic_cast<Engine_CameraHwb *>(mEngineContext.get());
  context->setHdwImage(buffer, orientation);

  mEngineContext->prepare(env);
}

void Processor::getNV21FromHardwareBuffer(JNIEnv *env, AHardwareBuffer *buffer,
                                          void *outputData) {
  //	Engine_HwbToNV21 *context = dynamic_cast<Engine_HwbToNV21
  //*>(mEngineContext.get()); 	context->getNV21FromHardwareBuffer(buffer,
  //outputData);
}

void Processor::render(bool loop) {
  mLoopDraw = loop;
  do {
    mEngineContext->draw();
  } while (mLoopDraw);
}

void Processor::stopLoopRender() { mLoopDraw = false; }

void Processor::unInit(JNIEnv *env) {}