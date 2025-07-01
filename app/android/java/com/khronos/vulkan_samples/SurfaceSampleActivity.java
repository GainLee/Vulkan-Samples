package com.khronos.vulkan_samples;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.provider.MediaStore;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Activity for running Vulkan samples with external surface support.
 * This is a standard Android Activity (not GameActivity) that creates a SurfaceView
 * and passes the Surface to the native layer.
 * 
 * Thread Management:
 * - Currently using HandlerThread for render thread management
 * - Alternative: ExecutorService (see commented code below)
 * 
 * HandlerThread advantages:
 * - Better integration with Android's message queue system
 * - Easier to post messages and handle lifecycle
 * - Built-in Looper for message handling
 * 
 * ExecutorService advantages:
 * - More flexible thread pool management
 * - Better for multiple concurrent tasks
 * - Standard Java concurrency API
 */
public class SurfaceSampleActivity extends Activity implements SurfaceHolder.Callback {
    
    private SurfaceView surfaceView;
    private Surface surface;
    private String[] sampleArgs;
    private HandlerThread renderThread;
    private Handler renderHandler;
    private volatile boolean isRunning = false;
    private volatile boolean isInitialized = false;
    private volatile boolean imageSelected = false;
    
    // Image selection
    private static final int PICK_IMAGE_REQUEST = 1;
    private int imageWidth = 0;
    private int imageHeight = 0;
    private boolean isImageSample = false;
    
    // UI elements
    private LinearLayout overlayLayout;
    private LinearLayout bottomInfo;
    private Button toggleOverlayBtn;
    private Button backBtn;
    private Button selectImageBtn;
    private TextView sampleNameText;
    private boolean uiVisible = true;
    
    // Native methods
    public static native void nativeSetSurface(Surface surface);
    public static native void nativeReleaseSurface();
    public static native void nativeRunSample(AssetManager assetManager, String[] args);
    public static native boolean nativeInitSample(AssetManager assetManager, String[] args);
    public static native boolean nativeRenderFrame();
    public static native void nativeTerminateSample();
    public static native void nativeSetImageData(Bitmap bitmap);
    
    static {
        System.loadLibrary("vulkan_samples");
    }
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String dataPath = getExternalFilesDir(null).getAbsolutePath();
        Log.i("Sample", "dataPath:"+dataPath);

        String cacheDir = getCacheDir().getAbsolutePath();
        Log.i("Sample", "cacheDir:"+cacheDir);

        // Get sample arguments from intent
        Intent intent = getIntent();
        sampleArgs = intent.getStringArrayExtra("sample_args");
        if (sampleArgs == null) {
            // Try to get single sample parameter and convert to args array
            String sampleName = intent.getStringExtra("sample");
            if (sampleName != null) {
                sampleArgs = new String[]{"sample", sampleName};
            } else {
                sampleArgs = new String[]{"vulkan_samples"};
            }
        }
        
        // Check if this is camera_preview sample
        isImageSample = sampleArgs != null && sampleArgs.length >= 2 && 
                       "camera_preview".equals(sampleArgs[1]);
        
        // Set up full screen mode but keep title bar for our custom UI
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                           WindowManager.LayoutParams.FLAG_FULLSCREEN);
        
        // Set the layout
        setContentView(R.layout.activity_surface_sample);
        
        // Initialize UI elements
        initializeUI();
        
        // Configure SurfaceView
        surfaceView = findViewById(R.id.surface_view);
        surfaceView.getHolder().addCallback(this);
        
        // Add touch listener to toggle UI
        surfaceView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    if (!uiVisible) {
                        toggleUI();
                        return true;
                    }
                }
                return false;
            }
        });
        
        // Update sample name
        updateSampleInfo();
        
        // For camera_preview sample, prompt user to select image after UI is ready
        if (isImageSample) {
            surfaceView.post(new Runnable() {
                @Override
                public void run() {
                    showStatus("Please select an image to begin");
                    Toast.makeText(SurfaceSampleActivity.this, 
                                 "Please select an image to start the camera preview sample", 
                                 Toast.LENGTH_LONG).show();
                }
            });
        }
    }
    
    private void initializeUI() {
        overlayLayout = findViewById(R.id.overlay_layout);
        bottomInfo = findViewById(R.id.bottom_info);
        toggleOverlayBtn = findViewById(R.id.toggle_overlay_btn);
        backBtn = findViewById(R.id.back_btn);
        sampleNameText = findViewById(R.id.sample_name_text);
        
        // Add select image button for camera_preview sample
        selectImageBtn = new Button(this);
        selectImageBtn.setText("Select Image");
        selectImageBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                selectImageFromGallery();
            }
        });
        
        // Only show select image button for camera_preview sample
        if (isImageSample) {
            bottomInfo.addView(selectImageBtn);
        }
        
        // Set button click listeners
        toggleOverlayBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleUI();
            }
        });
        
        backBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onBackPressed();
            }
        });
    }
    
    private void selectImageFromGallery() {
        Intent intent = new Intent(Intent.ACTION_PICK, MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
        intent.setType("image/*");
        startActivityForResult(intent, PICK_IMAGE_REQUEST);
    }
    
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        
        if (requestCode == PICK_IMAGE_REQUEST && resultCode == RESULT_OK && data != null) {
            Uri imageUri = data.getData();
            if (imageUri != null) {
                loadImageFromUri(imageUri);
                renderFrame();
            }
        }
    }
    
    private void loadImageFromUri(Uri uri) {
        try {
            InputStream inputStream = getContentResolver().openInputStream(uri);
            if (inputStream != null) {
                loadImageFromInputStream(inputStream, "Selected image");
            }
        } catch (IOException e) {
            Log.e("SurfaceSampleActivity", "Failed to load image from URI", e);
            Toast.makeText(this, "Failed to load image", Toast.LENGTH_SHORT).show();
        }
    }
    
    private void loadImageFromInputStream(InputStream inputStream, String description) {
        try {
            // Try to load as a bitmap
            Bitmap bitmap = BitmapFactory.decodeStream(inputStream);
            if (bitmap != null) {
                convertBitmapToImageData(bitmap, description);
                bitmap.recycle();
            } else {
                Log.w("SurfaceSampleActivity", "Could not decode image as bitmap: " + description);
            }
        } catch (Exception e) {
            Log.e("SurfaceSampleActivity", "Failed to load image from stream", e);
        } finally {
            try {
                inputStream.close();
            } catch (IOException e) {
                // Ignore
            }
        }
    }
    
    private void convertBitmapToImageData(Bitmap bitmap, String description) {
        // Convert bitmap to RGBA8888 format for consistency
        Bitmap rgbaBitmap = bitmap.copy(Bitmap.Config.ARGB_8888, false);
        
        imageWidth = rgbaBitmap.getWidth();
        imageHeight = rgbaBitmap.getHeight();
        
        Log.i("SurfaceSampleActivity", "Passing bitmap to native: " + description + 
              " (" + imageWidth + "x" + imageHeight + ")");
        
        // Send bitmap directly to native if render thread is running
        if (isRunning) {
            nativeSetImageData(rgbaBitmap);
        }
        
        showStatus("Loaded: " + description + " (" + imageWidth + "x" + imageHeight + ")");
        
        rgbaBitmap.recycle();
        
        // Mark image as selected and initialize engine if needed
        imageSelected = true;
        if (isImageSample && !isInitialized && isRunning) {
            initializeEngine();
        }
    }
    
    private void updateSampleInfo() {
        if (sampleArgs != null && sampleArgs.length >= 2) {
            String sampleName = sampleArgs[1]; // args[0] is "sample", args[1] is sample name
            sampleNameText.setText("Sample: " + sampleName);
        } else {
            sampleNameText.setText("Sample: Unknown");
        }
    }
    
    private void showStatus(String status) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                sampleNameText.setText(status);
            }
        });
    }
    
    private void toggleUI() {
        uiVisible = !uiVisible;
        int visibility = uiVisible ? View.VISIBLE : View.GONE;
        
        overlayLayout.setVisibility(visibility);
        bottomInfo.setVisibility(visibility);
        
        toggleOverlayBtn.setText(uiVisible ? "Hide UI" : "Show UI");
        
        if (!uiVisible) {
            // Hide system UI for full immersive experience
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }
    
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Surface is created, but we wait for surfaceChanged to get the correct size
    }
    
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // Surface is ready with correct size
        surface = holder.getSurface();
        
        if (surface != null && surface.isValid()) {
            // Set the surface in native code
            nativeSetSurface(surface);
            
            // Start rendering thread
            startRenderThread();
        }
    }
    
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        LOGI("surfaceDestroyed called");
        
        // Stop rendering asynchronously to avoid blocking UI thread
        stopRenderThread();
        
        if (surface != null) {
            LOGI("Releasing native surface");
            nativeReleaseSurface();
            surface = null;
        }
        
        LOGI("Surface destroyed and cleaned up");
    }
    
    private void startRenderThread() {
        synchronized (this) {
            if (renderThread != null && renderThread.isAlive()) {
                return; // Already running
            }
        }
        
        isRunning = true;
        renderThread = new HandlerThread("RenderThread");
        renderThread.start();
        renderHandler = new Handler(renderThread.getLooper());
        
        if (isImageSample) {
            // For camera_preview sample, wait for image selection
            showStatus("Please select an image to begin");
            // Don't initialize engine yet, wait for image selection
        } else {
            // For other samples, initialize immediately
            showStatus("Surface ready, starting sample...");
            initializeEngine();
        }
        
        // Auto-hide UI after 5 seconds once sample is running
        surfaceView.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (uiVisible && isRunning) {
                    toggleUI();
                }
            }
        }, 5000);
    }
    
    private void renderFrame() {
        renderHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isRunning && isInitialized) {
                    try {
                        // Render one frame
                        boolean renderSuccess = nativeRenderFrame();
                        if (!renderSuccess) {
                            LOGW("Frame render failed, stopping render loop");
                            isRunning = false;
                            showStatus("Render failed");
                            return;
                        }
                    } catch (Exception e) {
                        LOGW("Exception in render loop: " + e.getMessage());
                        isRunning = false;
                        showStatus("Render error: " + e.getMessage());
                    }
                }
            }
        });
    }
    
    private void stopRenderThread() {
        LOGI("Stopping render thread synchronously...");
        isRunning = false;
        
        synchronized (this) {
            if (renderThread != null && renderThread.isAlive()) {
                // Terminate the native sample first
                try {
                    nativeTerminateSample();
                    showStatus("Sample terminated");
                } catch (Exception e) {
                    LOGW("Error terminating sample: " + e.getMessage());
                }
                
                renderThread.quit();
                renderThread = null;
                renderHandler = null;
            }
        }
        LOGI("Render thread stopped");
    }
    
    @Override
    protected void onPause() {
        super.onPause();
        // Note: We don't stop the render thread on pause to maintain state
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        // Restore immersive mode
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    // Helper methods for logging
    private static final String TAG = "SurfaceSampleActivity";
    
    private void LOGI(String message) {
        Log.i(TAG, message);
    }
    
    private void LOGW(String message) {
        Log.w(TAG, message);
    }
    
    private void initializeEngine() {
        if (isInitialized) {
            return; // Already initialized
        }
        
        renderHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    showStatus("Initializing sample...");
                    
                    // Initialize the native sample
                    boolean initSuccess = nativeInitSample(SurfaceSampleActivity.this.getAssets(), sampleArgs);
                    if (!initSuccess) {
                        showStatus("Sample initialization failed");
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                Toast.makeText(SurfaceSampleActivity.this, 
                                             "Sample initialization failed", 
                                             Toast.LENGTH_LONG).show();
                                finish();
                            }
                        });
                        return;
                    }
                    
                    isInitialized = true;
                    showStatus("Sample initialized, starting render loop...");
                    
                    // Start render loop
                    renderFrame();
                    
                } catch (Exception e) {
                    showStatus("Error: " + e.getMessage());
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            Toast.makeText(SurfaceSampleActivity.this, 
                                         "Sample execution failed: " + e.getMessage(), 
                                         Toast.LENGTH_LONG).show();
                            finish();
                        }
                    });
                } finally {
                    // Cleanup will be handled by stopRenderThread
                }
            }
        });
    }
} 