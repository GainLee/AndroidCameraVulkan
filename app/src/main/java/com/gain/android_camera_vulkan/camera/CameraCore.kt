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

package com.gain.vulkan.camera

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.SurfaceTexture
import android.hardware.HardwareBuffer
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.util.Range
import android.util.Size
import android.view.Display
import android.view.Surface
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.coroutines.suspendCoroutine

class CameraCore constructor(private var mContext: Context) {

    /** Readers used as buffers for camera still shots */
    private lateinit var imageReader: ImageReader

    /** [HandlerThread] where all camera operations run */
    private lateinit var cameraThread: HandlerThread

    /** [Handler] corresponding to [cameraThread] */
    private lateinit var cameraHandler: Handler

    /** [HandlerThread] where all buffer reading operations run */
    private lateinit var imageReaderThread: HandlerThread

    /** [Handler] corresponding to [imageReaderThread] */
    private lateinit var imageReaderHandler: Handler

    /** The [CameraDevice] that will be opened in this fragment */
    private lateinit var camera: CameraDevice

    /** Internal reference to the ongoing [CameraCaptureSession] configured with our parameters */
    private lateinit var session: CameraCaptureSession

    private lateinit var targets: List<Surface>

    private lateinit var mExpTimeRange: Range<Long>
    private lateinit var mISORange: Range<Int>
    private var mOrientation: Int = 0

    fun getOrientation():Int { return mOrientation }

    fun init() {
        cameraThread = HandlerThread("CameraThread").apply { start() }
        cameraHandler = Handler(cameraThread.looper)
        imageReaderThread = HandlerThread("imageReaderThread").apply { start() }
        imageReaderHandler = Handler(imageReaderThread.looper)
    }

    fun unInit() {
        cameraThread.quitSafely()
        imageReaderThread.quitSafely()
    }

    /**
     * Begin all camera operations in a coroutine in the main thread. This function:
     * - Opens the camera
     * - Configures the camera session
     * - Starts the preview by dispatching a repeating capture request
     * - Sets up the still image capture listeners
     */
    public suspend fun initializeCamera(displaySize:Size, configPreview:(prevSize:Size)->Unit,
    onImageAvailable:(imageReader:ImageReader)->Unit, previewSurface: Surface? = null) {
        withContext(Dispatchers.Main) {
            // Open the selected camera
            val context = mContext.applicationContext
            var cameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
            camera = openCamera(cameraManager, "0", cameraHandler)

            var characteristics = cameraManager.getCameraCharacteristics("0")

            mExpTimeRange = characteristics.get(CameraCharacteristics.SENSOR_INFO_EXPOSURE_TIME_RANGE)!!
            mISORange = characteristics.get(CameraCharacteristics.SENSOR_INFO_SENSITIVITY_RANGE)!!
            mOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION)!!

            val previewSize = getPreviewOutputSize(
                displaySize, characteristics, SurfaceTexture::class.java)

            Log.i("Vulkan", "previewSize:${previewSize.width}x${previewSize.height}")
            configPreview(previewSize)

            imageReader = ImageReader.newInstance(
                previewSize.width, previewSize.height, ImageFormat.PRIVATE,
                IMAGE_BUFFER_SIZE,
                HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE
            )

            imageReader.setOnImageAvailableListener(ImageReader.OnImageAvailableListener {
                onImageAvailable(it)
            }, imageReaderHandler)

            targets = if(previewSurface==null) {
                listOf(imageReader.surface)
            } else {
                listOf(imageReader.surface, previewSurface)
            }

            // Start a capture session using our open camera and list of Surfaces where frames will go
            session = createCaptureSession(camera, targets, cameraHandler)

            startPreview()
        }
    }

    /** Opens the camera and returns the opened device (as the result of the suspend coroutine) */
    @SuppressLint("MissingPermission")
    private suspend fun openCamera(
        manager: CameraManager,
        cameraId: String,
        handler: Handler? = null
    ): CameraDevice = suspendCancellableCoroutine { cont ->
        manager.openCamera(cameraId, object : CameraDevice.StateCallback() {
            override fun onOpened(device: CameraDevice) = cont.resume(device)

            override fun onDisconnected(device: CameraDevice) {
                Log.w(TAG, "Camera $cameraId has been disconnected")
            }

            override fun onError(device: CameraDevice, error: Int) {
                val msg = when (error) {
                    ERROR_CAMERA_DEVICE -> "Fatal (device)"
                    ERROR_CAMERA_DISABLED -> "Device policy"
                    ERROR_CAMERA_IN_USE -> "Camera in use"
                    ERROR_CAMERA_SERVICE -> "Fatal (service)"
                    ERROR_MAX_CAMERAS_IN_USE -> "Maximum cameras in use"
                    else -> "Unknown"
                }
                val exc = RuntimeException("Camera $cameraId error: ($error) $msg")
                Log.e(TAG, exc.message, exc)
                if (cont.isActive) cont.resumeWithException(exc)
            }
        }, handler)
    }

    /**
     * Starts a [CameraCaptureSession] and returns the configured session (as the result of the
     * suspend coroutine
     */
    private suspend fun createCaptureSession(
        device: CameraDevice,
        targets: List<Surface>,
        handler: Handler? = null
    ): CameraCaptureSession = suspendCoroutine { cont ->

        // Create a capture session using the predefined targets; this also involves defining the
        // session state callback to be notified of when the session is ready
        device.createCaptureSession(targets, object : CameraCaptureSession.StateCallback() {

            override fun onConfigured(session: CameraCaptureSession) = cont.resume(session)

            override fun onConfigureFailed(session: CameraCaptureSession) {
                val exc = RuntimeException("Camera ${device.id} session configuration failed")
                Log.e(TAG, exc.message, exc)
                cont.resumeWithException(exc)
            }
        }, handler)
    }

    private fun startPreview() {
        val captureRequest = session.device.createCaptureRequest(
            CameraDevice.TEMPLATE_PREVIEW
        ).apply {
            for (item in targets) {
                addTarget(item)
            }
        }
        session.setRepeatingRequest(captureRequest.build(), null, cameraHandler)
    }

    public fun close() {
        camera.close()
    }

    public fun runInImageReaderThread(runnable: Runnable) {
        imageReaderHandler.post(runnable)
    }

    companion object {
        private val TAG = CameraCore::class.java.simpleName

        /** Maximum number of images that will be held in the reader's buffer */
        private const val IMAGE_BUFFER_SIZE: Int = 3

        /** Maximum time allowed to wait for the result of an image capture */
        private const val IMAGE_CAPTURE_TIMEOUT_MILLIS: Long = 5000
    }
}