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
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.util.Log;
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

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

import com.khronos.vulkan_samples.model.Sample;
import com.khronos.vulkan_samples.model.SampleStore;

/**
 * VulkanSurfaceActivity - A regular Android Activity that hosts a SurfaceView
 * and passes the Surface to native Vulkan code for rendering.
 *
 * This allows Vulkan rendering without using NativeActivity.
 */
public class VulkanSurfaceActivity extends AppCompatActivity implements SurfaceHolder.Callback {

    private static final String TAG = "VulkanSurface";
    private static final int PERMISSION_REQUEST_CODE = 100;

    private SurfaceView surfaceView;
    private FrameLayout containerLayout;

    private volatile boolean nativeInitialized = false;
    private volatile boolean surfaceCreated = false;
    private Surface currentSurface;

    // Native methods
    private native void nativeInit(String externalDir, String tempDir);
    private native void nativeSetSurface(Surface surface);
    private native void nativeSurfaceCreated(int width, int height);
    private native void nativeSurfaceChanged(int width, int height);
    private native void nativeSurfaceDestroyed();
    private native void nativeRun(String[] args);
    private native void nativeCleanup();
    private native Sample[] getSamples();

    private String[] pendingArgs = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load native library
        if (!loadNativeLibrary("vulkan_samples")) {
            Toast.makeText(this, "Failed to load Vulkan native library", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // Initialize file paths
        initFilePaths();

        // Create UI
        containerLayout = new FrameLayout(this);
        containerLayout.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        surfaceView = new SurfaceView(this);
        surfaceView.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        // Set pixel format to RGBA_8888 to match Vulkan swapchain format
        surfaceView.getHolder().setFormat(PixelFormat.RGBA_8888);
        surfaceView.getHolder().addCallback(this);
        containerLayout.addView(surfaceView);

        setContentView(containerLayout);

        // Hide system UI for fullscreen experience
        hideSystemUI();

        // Check permissions
        checkPermissions();
    }

    private void initFilePaths() {
        File externalFilesDir = getExternalFilesDir("");
        File tempFilesDir = getCacheDir();

        if (externalFilesDir != null && tempFilesDir != null) {
            String sharedStorage = externalFilesDir.getPath().split(Pattern.quote("Android"))[0];
            File externalDir = new File(sharedStorage, getPackageName());

            Log.i(TAG, "External: " + externalDir.toString());
            Log.i(TAG, "Temp: " + tempFilesDir.toString());

            nativeInit(externalDir.toString(), tempFilesDir.toString());
            nativeInitialized = true;
        }
    }

    private boolean loadNativeLibrary(String libName) {
        try {
            System.loadLibrary(libName);
            return true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
            return false;
        }
    }

    private void checkPermissions() {
        List<String> permissionsNeeded = new ArrayList<>();

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            permissionsNeeded.add(Manifest.permission.CAMERA);
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            permissionsNeeded.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
        }

        if (!permissionsNeeded.isEmpty()) {
            ActivityCompat.requestPermissions(this,
                    permissionsNeeded.toArray(new String[0]),
                    PERMISSION_REQUEST_CODE);
        } else {
            onPermissionsGranted();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            boolean allGranted = true;
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }
            if (allGranted) {
                onPermissionsGranted();
            } else {
                Toast.makeText(this, "Permissions required for Vulkan rendering", Toast.LENGTH_LONG).show();
            }
        }
    }

    private void onPermissionsGranted() {
        // Permissions granted, wait for surface to be created
        Log.i(TAG, "Permissions granted, waiting for surface...");
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "Surface created: " + holder.getSurfaceFrame().width() + "x" + holder.getSurfaceFrame().height());
        currentSurface = holder.getSurface();
        surfaceCreated = true;

        // Pass surface to native
        nativeSetSurface(currentSurface);
        nativeSurfaceCreated(holder.getSurfaceFrame().width(), holder.getSurfaceFrame().height());

        // Start native rendering if args are pending
        if (pendingArgs != null) {
            startNativeRendering(pendingArgs);
            pendingArgs = null;
        } else {
            // Default args - run a sample
            startNativeRendering(new String[]{"sample", "hello_triangle"});
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "Surface changed: " + width + "x" + height);
        nativeSurfaceChanged(width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "Surface destroyed");
        surfaceCreated = false;
        currentSurface = null;
        nativeSurfaceDestroyed();
    }

    public void setArguments(String[] args) {
        if (surfaceCreated && nativeInitialized) {
            startNativeRendering(args);
        } else {
            pendingArgs = args;
        }
    }

    private void startNativeRendering(String[] args) {
        // Run on separate thread to avoid blocking UI
        final String[] threadArgs = args;
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    nativeRun(threadArgs);
                } catch (Exception e) {
                    Log.e(TAG, "Native rendering error: " + e.getMessage());
                }
            }
        }, "VulkanRenderThread").start();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
    }

    @Override
    protected void onPause() {
        super.onPause();
        // Signal native to pause rendering
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        nativeCleanup();
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
