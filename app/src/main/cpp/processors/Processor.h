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

#ifndef GAINVULKANSAMPLE_SAMPLE_H
#define GAINVULKANSAMPLE_SAMPLE_H

#include "../engine/VulkanContext.h"
#include "../engine/VulkanImageWrapper.h"
#include "EngineContext.h"
#include <android/native_window_jni.h>
#include <glm/vec2.hpp>
#include <memory>
#include <vulkan/vulkan.hpp>

using namespace gain;

// 与NativeVulkan.java中EngineType保持一致
enum EngineType { CAMERA_HARDWAREBUFFER, HWB_TO_NV21, LUT };

class Processor {
public:
  explicit Processor();

  static std::unique_ptr<Processor> create(AAssetManager *assetManager);

  void configEngine(uint32_t type);

  void initialize(AAssetManager *assetManager);

  void unInit(JNIEnv *env);

  void prepareHardwareBuffer(JNIEnv *env, AHardwareBuffer *buffer,
                             int orientation);

  void getNV21FromHardwareBuffer(JNIEnv *env, AHardwareBuffer *buffer,
                                 void *outputData);

  void render(bool loop);

  void stopLoopRender();

  void setWindow(ANativeWindow *window, uint32_t w, uint32_t h);

  void onWindowSizeChanged(ANativeWindow *window, uint32_t w, uint32_t h);

private:
  std::shared_ptr<VulkanContext> mVulkanContext;

  std::unique_ptr<EngineContext> mEngineContext;

  bool mLoopDraw;
};

#endif // GAINVULKANSAMPLE_SAMPLE_H
