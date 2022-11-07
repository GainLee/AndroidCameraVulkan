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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
package com.gain.android_camera_vulkan;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.hardware.HardwareBuffer;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;

import java.nio.ByteBuffer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;
import kotlin.Pair;

public class NativeVulkan {
    public enum EngineType {
        CAMERA_HARDWAREBUFFER,
        HARDWAREBUFFER_TO_NV21,
        LUT
    }

    // Used to load the 'vulkan' library on application startup.
    static {
        System.loadLibrary("vulkanSample");
    }

    private long mVulkanHandle;

    private HandlerThread mRenderThread;
    private Handler mRenderHandler;

    private boolean mDrawing = false;

    // Return a non-zero handle on success, and 0L if failed.
    private native long nativeInit(AssetManager assetManager);

    private native void nativeConfigEngine(long handle, int sampleType);

    // Frees up any underlying native resources. After calling this method, the NativeVulkan
    // must not be used in any way.
    private native void nativeUnInit(long handle);

    private native void nativePrepareHardwareBuffer(long handle, HardwareBuffer buffer, int orientation);

    private native void nativeStartRender(long handle, boolean loop);

    private native void nativeStopLoopRender(long handle);

    private native void nativeSetWindow(long handle, Surface surface, int width, int height);

    private native void nativeOnWindowSizeChanged(long handle, Surface surface, int width, int height);

    public void init(AssetManager assetManager) {
        if (mRenderThread != null) {
            mRenderThread.quitSafely();
            mRenderThread = null;
            mRenderHandler = null;
        }

        mRenderThread = new HandlerThread("VK_RenderThread");
        mRenderThread.start();
        mRenderHandler = new Handler(mRenderThread.getLooper());

        mVulkanHandle = nativeInit(assetManager);
    }

    public void unInit() {
        if (mVulkanHandle != 0L) {
            nativeUnInit(mVulkanHandle);
            mVulkanHandle = 0L;
        }

        if (mRenderThread != null) {
            mRenderThread.quitSafely();
            mRenderThread = null;
            mRenderHandler = null;
        }

        mDrawing = false;
    }

    public void configEngine(EngineType engineType) {
        if (mVulkanHandle != 0L) {
            nativeConfigEngine(mVulkanHandle, engineType.ordinal());
        }
    }

    public void setWindow(@Nullable Surface surface, int width, int height) {
        nativeSetWindow(mVulkanHandle, surface, width, height);
    }

    public void onWindowSizeChanged(@Nullable Surface surface, int width, int height) {
        nativeOnWindowSizeChanged(mVulkanHandle, surface, width, height);
    }

    public void prepareHardwareBuffer(HardwareBuffer hardwareBuffer, int orientation) {
        nativePrepareHardwareBuffer(mVulkanHandle, hardwareBuffer, orientation);
    }

    public void startRender(boolean loop) {
        if (mDrawing) {
            return;
        }

        mRenderHandler.post(() -> {
            if(loop) {
                mDrawing = true;
            }
            nativeStartRender(mVulkanHandle, loop);
        });

//        nativeStartRender(mVulkanHandle, loop);
    }

    public void stopLoopRender() {
        nativeStopLoopRender(mVulkanHandle);

        mRenderThread.quitSafely();
        mRenderThread = null;
    }
}
