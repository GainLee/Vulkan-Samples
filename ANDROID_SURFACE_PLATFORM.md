# Android Surface Platform for Vulkan Samples

This document describes how to use the Vulkan Samples framework with regular Android Activities (AppCompatActivity) instead of NativeActivity. This is particularly useful for integrating Vulkan rendering into existing Android apps or when you need to combine Vulkan with other Android features like the Camera2 API.

## Overview

The Android Surface Platform allows you to:
- Use Vulkan from a regular `AppCompatActivity` instead of `NativeActivity`
- Pass a `Surface` from Java to native code via JNI
- Integrate Vulkan with Android UI components (buttons, overlays, etc.)
- Combine Vulkan rendering with Camera preview and other Android features

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Java Layer                               │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │ VulkanSurfaceAct │  │ VulkanCameraAct  │                     │
│  │   (Base Class)   │  │ (Camera Example) │                     │
│  └────────┬─────────┘  └────────┬─────────┘                     │
│           │                     │                                │
│           └─────────────────────┘                                │
│                     │                                            │
│                     ▼                                            │
│           ┌─────────────────┐                                    │
│           │   SurfaceView   │                                    │
│           └────────┬────────┘                                    │
│                    │                                             │
└────────────────────┼─────────────────────────────────────────────┘
                     │ JNI
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Native Layer                              │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │AndroidSurfacePlat│  │  AndroidWindow   │                     │
│  │   (Platform)     │──│   (Window)       │                     │
│  └────────┬─────────┘  └────────┬─────────┘                     │
│           │                     │                                │
│           ▼                     ▼                                │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │ ANativeWindow    │  │   Vulkan Swap    │                     │
│  │  (from Surface)  │──│    Chain         │                     │
│  └──────────────────┘  └──────────────────┘                     │
└─────────────────────────────────────────────────────────────────┘
```

## Key Components

### 1. VulkanSurfaceActivity.java
Base activity class that handles SurfaceView lifecycle and JNI communication.

**Key Features:**
- Extends `AppCompatActivity`, not `NativeActivity`
- Implements `SurfaceHolder.Callback` for surface lifecycle
- JNI methods: `nativeInit`, `nativeSetSurface`, `nativeSurfaceCreated`, `nativeSurfaceChanged`, `nativeSurfaceDestroyed`, `nativeRun`, `nativeCleanup`

### 2. AndroidSurfacePlatform
C++ platform class that receives the Surface from Java and creates an `ANativeWindow`.

**Key Methods:**
- `set_surface(JNIEnv *env, jobject surface)` - Creates `ANativeWindow` from Java Surface
- `on_surface_created(int width, int height)` - Called when surface is ready
- `run(const std::vector<std::string> &args)` - Starts the Vulkan sample

### 3. AndroidWindow (Modified)
Extended to support both `AndroidPlatform` (NativeActivity) and `AndroidSurfacePlatform` (Surface-based).

## Usage

### Basic Vulkan Rendering

```java
// Create a simple Vulkan activity
public class MyVulkanActivity extends VulkanSurfaceActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Optional: Set which sample to run
        setArguments(new String[]{"sample", "hello_triangle"});
    }
}
```

### Custom UI with Vulkan

```java
public class VulkanWithUIActivity extends VulkanSurfaceActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Base class creates SurfaceView, add UI overlay
        FrameLayout container = findViewById(android.R.id.content);

        // Add buttons, controls, etc.
        Button captureButton = new Button(this);
        captureButton.setText("Capture");
        captureButton.setOnClickListener(v -> {
            // Handle capture
        });

        container.addView(captureButton);
    }
}
```

### Camera + Vulkan Integration

```java
public class CameraVulkanActivity extends VulkanCameraActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Camera setup is handled by base class
        // Vulkan receives camera frames via ImageReader
        setArguments(new String[]{"sample", "camera_vulkan"});
    }
}
```

## Build Configuration

### Update CMakeLists.txt

Add the new source files to `framework/CMakeLists.txt`:

```cmake
set(ANDROID_FILES
    # Header Files
    platform/android/android_platform.h
    platform/android/android_window.h
    platform/android/android_surface_platform.h    # NEW
    # Source Files
    platform/android/android_platform.cpp
    platform/android/android_window.cpp
    platform/android/android_surface_platform.cpp  # NEW
)
```

### Update AndroidManifest.xml

Add the new activities to `app/android/AndroidManifest.xml`:

```xml
<manifest>
    <!-- Add permissions -->
    <uses-permission android:name="android.permission.CAMERA" />

    <application>
        <!-- NativeActivity (existing) -->
        <activity android:name="com.khronos.vulkan_samples.NativeSampleActivity"
            android:configChanges="orientation|keyboardHidden|screenSize">
            <meta-data android:name="android.app.lib_name" android:value="vulkan_samples" />
        </activity>

        <!-- NEW: Surface-based activities -->
        <activity android:name="com.khronos.vulkan_samples.VulkanSurfaceActivity"
            android:configChanges="orientation|keyboardHidden|screenSize"
            android:theme="@style/AppTheme">
        </activity>

        <activity android:name="com.khronos.vulkan_samples.VulkanCameraActivity"
            android:configChanges="orientation|keyboardHidden|screenSize"
            android:theme="@style/AppTheme">
        </activity>
    </application>
</manifest>
```

## JNI Method Signatures

| Java Method | Native Function | Purpose |
|-------------|-----------------|---------|
| `nativeInit(String, String)` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeInit` | Initialize file paths |
| `nativeSetSurface(Surface)` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeSetSurface` | Pass Surface to native |
| `nativeSurfaceCreated(int, int)` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeSurfaceCreated` | Surface ready |
| `nativeSurfaceChanged(int, int)` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeSurfaceChanged` | Size changed |
| `nativeSurfaceDestroyed()` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeSurfaceDestroyed` | Surface destroyed |
| `nativeRun(String[])` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeRun` | Start rendering |
| `nativeCleanup()` | `Java_com_khronos_vulkan_samples_VulkanSurfaceActivity_nativeCleanup` | Cleanup resources |

## Implementation Details

### Surface to ANativeWindow

The key step is converting a Java Surface to a native ANativeWindow:

```cpp
void AndroidSurfacePlatform::set_surface(JNIEnv *env, jobject surface)
{
    native_window = ANativeWindow_fromSurface(env, surface);
}
```

### Thread Safety

The platform uses mutex and condition variable to synchronize surface state between Java UI thread and native render thread:

```cpp
std::mutex surface_mutex;
std::condition_variable surface_cv;
bool surface_ready{false};
```

### Lifecycle Management

1. **Activity.onCreate()** - Initialize native library, create SurfaceView
2. **surfaceCreated()** - Pass Surface to native, create ANativeWindow
3. **nativeRun()** - Start render thread
4. **surfaceDestroyed()** - Stop rendering, release ANativeWindow
5. **Activity.onDestroy()** - Cleanup native resources

## Differences from NativeActivity

| Feature | NativeActivity | Surface Platform |
|---------|---------------|------------------|
| Base Class | `NativeActivity` | `AppCompatActivity` |
| Window Access | `app->window` | `ANativeWindow_fromSurface()` |
| Event Loop | `android_app` event loop | Java callbacks via JNI |
| UI Integration | Limited | Full Android UI support |
| Lifecycle | `APP_CMD_*` commands | `SurfaceHolder.Callback` |

## Troubleshooting

### Surface not ready
- Ensure SurfaceView is attached to window hierarchy
- Check that SurfaceHolder.Callback is registered

### Black screen
- Verify native library is loaded (`System.loadLibrary()`)
- Check that Surface is passed before calling `nativeRun()`
- Validate Vulkan surface creation

### Permission issues
- Camera permission must be granted before opening camera
- Storage permissions needed for assets

## Example Samples

### Basic Triangle
```java
Intent intent = new Intent(context, VulkanSurfaceActivity.class);
intent.putExtra("sample", "hello_triangle");
startActivity(intent);
```

### Camera Preview
```java
Intent intent = new Intent(context, VulkanCameraActivity.class);
startActivity(intent);
```

## See Also

- `framework/platform/android/android_surface_platform.h`
- `framework/platform/android/android_surface_platform.cpp`
- `app/android/java/com/khronos/vulkan_samples/VulkanSurfaceActivity.java`
- `app/android/java/com/khronos/vulkan_samples/VulkanCameraActivity.java`
