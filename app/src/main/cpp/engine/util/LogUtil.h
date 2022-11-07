/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 by Gain
 * Copyright (C) 2022 by Google Inc
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
#ifndef YUVCROP_LOGUTIL_H
#define YUVCROP_LOGUTIL_H

#include <android/log.h>
#include <cassert>
#include <sys/time.h>

#define LOG_TAG "Vulkan"

#define LOGCATE(...)                                                           \
  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGCATV(...)                                                           \
  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGCATD(...)                                                           \
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGCATI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define FUN_BEGIN_TIME(FUN)                                                    \
  {                                                                            \
    LOGCATE("%s:%s func start", __FILE__, FUN);                                \
    long long t0 = GetSysCurrentTime();

#define FUN_END_TIME(FUN)                                                      \
  long long t1 = GetSysCurrentTime();                                          \
  LOGCATE("%s:%s func cost time %ldms", __FILE__, FUN, (long)(t1 - t0));       \
  }

#define BEGIN_TIME(FUN)                                                        \
  {                                                                            \
    LOGCATE("%s func start", FUN);                                             \
    long long t0 = GetSysCurrentTime();

#define END_TIME(FUN)                                                          \
  long long t1 = GetSysCurrentTime();                                          \
  LOGCATE("%s func cost time %ldms", FUN, (long)(t1 - t0));                    \
  }

static long long GetSysCurrentTime() {
  struct timeval time;
  gettimeofday(&time, NULL);
  long long curTime = ((long long)(time.tv_sec)) * 1000 + time.tv_usec / 1000;
  return curTime;
}

#define GO_CHECK_GL_ERROR(...)                                                 \
  LOGCATE("CHECK_GL_ERROR %s glGetError = %d, line = %d, ", __FUNCTION__,      \
          glGetError(), __LINE__)

#define DEBUG_LOGCATE(...)                                                     \
  LOGCATE("DEBUG_LOGCATE %s line = %d", __FUNCTION__, __LINE__)

// Vulkan call wrapper
#define CALL_VK(func)                                                          \
  if (vk::Result::eSuccess != (func)) {                                        \
    LOGCATE("Vulkan error. File[%s], line[%d]", __FILE__, __LINE__);           \
    assert(false);                                                             \
  }

#endif // YUVCROP_LOGUTIL_H
