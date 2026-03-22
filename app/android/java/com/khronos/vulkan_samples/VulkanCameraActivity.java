/* Copyright (c) 2019-2025, User Modified
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.khronos.vulkan_samples;

import android.Manifest;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * VulkanCameraActivity - Example of using Vulkan to render Camera preview
 *
 * This activity demonstrates:
 * 1. Setting up a SurfaceView for Vulkan rendering
 * 2. Passing the Surface to native Vulkan code
 * 3. Creating a Camera preview session with Vulkan as the renderer
 *
 * The native Vulkan code receives camera frames via an Android ImageReader
 * and renders them to the SurfaceView using Vulkan.
 */
public class VulkanCameraActivity extends AppCompatActivity implements SurfaceHolder.Callback {

    private static final String TAG = "VulkanCamera";
    private static final int REQUEST_PERMISSIONS = 200;

    // Camera
    private CameraManager cameraManager;
    private CameraDevice cameraDevice;
    private CameraCaptureSession cameraSession;
    private CaptureRequest.Builder captureRequestBuilder;
    private String cameraId;
    private HandlerThread cameraThread;
    private Handler cameraHandler;

    // Surface
    private SurfaceView surfaceView;
    private Surface previewSurface;
    private Size previewSize;

    // Native
    private volatile boolean nativeInitialized = false;
    private static final String[] REQUIRED_PERMISSIONS = {
            Manifest.permission.CAMERA,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
    };

    // Native methods
    private native void nativeInit(String externalDir, String tempDir);
    private native void nativeSetSurface(Surface surface);
    private native void nativeSurfaceCreated(int width, int height);
    private native void nativeSurfaceChanged(int width, int height);
    private native void nativeSurfaceDestroyed();
    private native void nativeCameraFrameAvailable(long timestamp);
    private native void nativeRun(String[] args);
    private native void nativeCleanup();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load native library
        if (!loadNativeLibrary()) {
            Toast.makeText(this, "Failed to load Vulkan native library", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // Check permissions
        if (!checkPermissions()) {
            return;
        }

        setupUI();
        initCamera();
        initNative();
    }

    private boolean loadNativeLibrary() {
        try {
            System.loadLibrary("vulkan_samples");
            return true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
            return false;
        }
    }

    private boolean checkPermissions() {
        List<String> permissionsNeeded = new ArrayList<>();
        for (String permission : REQUIRED_PERMISSIONS) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                permissionsNeeded.add(permission);
            }
        }

        if (!permissionsNeeded.isEmpty()) {
            ActivityCompat.requestPermissions(this,
                    permissionsNeeded.toArray(new String[0]),
                    REQUEST_PERMISSIONS);
            return false;
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_PERMISSIONS) {
            boolean allGranted = true;
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }
            if (allGranted) {
                setupUI();
                initCamera();
                initNative();
            } else {
                Toast.makeText(this, "Camera permissions required", Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    private void setupUI() {
        // Create FrameLayout as container
        FrameLayout container = new FrameLayout(this);
        container.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        // Create SurfaceView for Vulkan rendering
        surfaceView = new SurfaceView(this);
        surfaceView.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        surfaceView.getHolder().addCallback(this);

        container.addView(surfaceView);
        setContentView(container);

        hideSystemUI();
    }

    private void initCamera() {
        cameraManager = (CameraManager) getSystemService(CAMERA_SERVICE);
        cameraThread = new HandlerThread("CameraThread");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());
    }

    private void initNative() {
        // Initialize native file paths
        java.io.File externalDir = getExternalFilesDir("");
        java.io.File tempDir = getCacheDir();
        if (externalDir != null && tempDir != null) {
            nativeInit(externalDir.getAbsolutePath(), tempDir.getAbsolutePath());
            nativeInitialized = true;
        }
    }

    private void openCamera() {
        try {
            // Find back camera
            for (String id : cameraManager.getCameraIdList()) {
                CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(id);
                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == CameraCharacteristics.LENS_FACING_BACK) {
                    cameraId = id;
                    break;
                }
            }

            if (cameraId == null) {
                Log.e(TAG, "No back camera found");
                return;
            }

            // Get optimal preview size
            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(cameraId);
            StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            if (map == null) {
                Log.e(TAG, "StreamConfigurationMap is null");
                return;
            }

            // Choose preview size
            Size[] sizes = map.getOutputSizes(SurfaceHolder.class);
            previewSize = chooseOptimalSize(sizes, surfaceView.getWidth(), surfaceView.getHeight());
            Log.i(TAG, "Selected preview size: " + previewSize.getWidth() + "x" + previewSize.getHeight());

            // Open camera
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
                cameraManager.openCamera(cameraId, cameraStateCallback, cameraHandler);
            }
        } catch (CameraAccessException e) {
            Log.e(TAG, "Camera access error: " + e.getMessage());
        }
    }

    private Size chooseOptimalSize(Size[] choices, int width, int height) {
        if (choices == null || choices.length == 0) {
            return new Size(1920, 1080);
        }

        // Collect supported sizes that are at least as big as the preview Surface
        List<Size> bigEnough = new ArrayList<>();
        for (Size option : choices) {
            if (option.getWidth() >= width && option.getHeight() >= height) {
                bigEnough.add(option);
            }
        }

        // Pick the smallest of those, assuming we have a choice
        if (bigEnough.size() > 0) {
            return Collections.min(bigEnough, new Comparator<Size>() {
                @Override
                public int compare(Size lhs, Size rhs) {
                    return Long.signum((long) lhs.getWidth() * lhs.getHeight() -
                            (long) rhs.getWidth() * rhs.getHeight());
                }
            });
        }

        // If no size is big enough, pick the largest
        return choices[0];
    }

    private final CameraDevice.StateCallback cameraStateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(@NonNull CameraDevice camera) {
            Log.i(TAG, "Camera opened");
            cameraDevice = camera;
            createCameraSession();
        }

        @Override
        public void onDisconnected(@NonNull CameraDevice camera) {
            Log.w(TAG, "Camera disconnected");
            camera.close();
            cameraDevice = null;
        }

        @Override
        public void onError(@NonNull CameraDevice camera, int error) {
            Log.e(TAG, "Camera error: " + error);
            camera.close();
            cameraDevice = null;
        }
    };

    private void createCameraSession() {
        try {
            if (cameraDevice == null || previewSurface == null) {
                Log.e(TAG, "Cannot create session: camera or surface is null");
                return;
            }

            // Create capture request builder
            captureRequestBuilder = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            captureRequestBuilder.addTarget(previewSurface);

            // Create session
            List<Surface> surfaces = new ArrayList<>();
            surfaces.add(previewSurface);

            cameraDevice.createCaptureSession(surfaces, new CameraCaptureSession.StateCallback() {
                @Override
                public void onConfigured(@NonNull CameraCaptureSession session) {
                    Log.i(TAG, "Camera session configured");
                    cameraSession = session;
                    try {
                        captureRequestBuilder.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_AUTO);
                        cameraSession.setRepeatingRequest(captureRequestBuilder.build(), null, cameraHandler);
                    } catch (CameraAccessException e) {
                        Log.e(TAG, "Failed to start preview: " + e.getMessage());
                    }
                }

                @Override
                public void onConfigureFailed(@NonNull CameraCaptureSession session) {
                    Log.e(TAG, "Camera session configuration failed");
                }
            }, cameraHandler);
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to create camera session: " + e.getMessage());
        }
    }

    // SurfaceHolder.Callback implementation
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "Surface created: " + holder.getSurfaceFrame().width() + "x" + holder.getSurfaceFrame().height());
        previewSurface = holder.getSurface();

        if (nativeInitialized) {
            nativeSetSurface(previewSurface);
            nativeSurfaceCreated(holder.getSurfaceFrame().width(), holder.getSurfaceFrame().height());

            // Start native Vulkan rendering
            new Thread(new Runnable() {
                @Override
                public void run() {
                    nativeRun(new String[]{"sample", "camera_vulkan"}); // Use custom sample
                }
            }, "VulkanRenderThread").start();
        }

        // Open camera after surface is ready
        openCamera();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "Surface changed: " + width + "x" + height);
        nativeSurfaceChanged(width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "Surface destroyed");
        closeCamera();
        nativeSurfaceDestroyed();
    }

    private void closeCamera() {
        if (cameraSession != null) {
            cameraSession.close();
            cameraSession = null;
        }
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        closeCamera();
        nativeCleanup();

        if (cameraThread != null) {
            cameraThread.quitSafely();
            try {
                cameraThread.join();
                cameraThread = null;
            } catch (InterruptedException e) {
                Log.e(TAG, "Camera thread join interrupted");
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    private void hideSystemUI() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN);
    }
}
