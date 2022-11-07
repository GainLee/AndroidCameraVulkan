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

#include "engine/util/LogUtil.h"
#include "jni.h"
#include "processors/Processor.h"
#include <android/native_window_jni.h>
#include <stdexcept>

#define JCMCPRV(rettype, name)                                                 \
  extern "C" JNIEXPORT rettype JNICALL                                         \
      Java_com_gain_android_1camera_1vulkan_NativeVulkan_##name

Processor *castToProcessor(jlong handle) {
  return reinterpret_cast<Processor *>(static_cast<uintptr_t>(handle));
}

JCMCPRV(jlong, nativeInit)
(JNIEnv *env, jobject thiz, jobject asset_manager) {
  auto *assetManager = AAssetManager_fromJava(env, asset_manager);
  assert(assetManager != nullptr);
  auto sample = Processor::create(assetManager);
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(sample.release()));
}

JCMCPRV(void, nativeUnInit)
(JNIEnv *env, jobject thiz, jlong handle) {
  if (handle == 0L) {
    return;
  }
  auto sample = castToProcessor(handle);
  sample->unInit(env);
  delete sample;
}

JCMCPRV(void, nativeConfigEngine)
(JNIEnv *env, jobject thiz, jlong handle, jint type) {
  castToProcessor(handle)->configEngine(type);
}

JCMCPRV(void, nativeStartRender)
(JNIEnv *env, jobject thiz, jlong handle, jboolean loop) {
  castToProcessor(handle)->render(loop);
}

JCMCPRV(void, nativeSetWindow)
(JNIEnv *env, jobject thiz, jlong handle, jobject surface, jint width,
 jint height) {
  ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
  castToProcessor(handle)->setWindow(window, width, height);
}

JCMCPRV(void, nativeStopLoopRender)
(JNIEnv *env, jobject thiz, jlong handle) {
  castToProcessor(handle)->stopLoopRender();
}

JCMCPRV(void, nativePrepareHardwareBuffer)
(JNIEnv *env, jobject thiz, jlong handle, jobject buffer, jint orientation) {
  AHardwareBuffer *nativeBuffer =
      AHardwareBuffer_fromHardwareBuffer(env, buffer);
  if (!nativeBuffer) {
    __android_log_print(ANDROID_LOG_INFO, "Vulkan",
                        "Unable to obtain native HardwareBuffer.");
    return;
  }

  castToProcessor(handle)->prepareHardwareBuffer(env, nativeBuffer,
                                                 orientation);
}

JCMCPRV(void, nativeOnWindowSizeChanged)
(JNIEnv *env, jobject thiz, jlong handle, jobject surface, jint width,
 jint height) {
  ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
  castToProcessor(handle)->onWindowSizeChanged(window, width, height);
}