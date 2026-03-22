# Android Surface-based Vulkan Rendering

This modification allows the Vulkan Samples framework to work with regular Android Activities that pass a Surface via JNI, instead of using NativeActivity.

## Overview

The original Vulkan Samples framework relies on `android_native_app_glue` and `NativeActivity` to manage the Android window lifecycle. This modification adds support for:

1. **VulkanSurfaceActivity** - A regular AppCompatActivity with a SurfaceView that passes the Surface to native code
2. **AndroidSurfacePlatform** - A C++ platform implementation that receives Surface from Java and creates ANativeWindow
3. **VulkanCameraActivity** - Example showing how to use Vulkan to render Camera preview

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Java Layer                               │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐    ┌─────────────────────┐             │
│  │ VulkanSurfaceActivity│    │ VulkanCameraActivity│             │
│  │  (AppCompatActivity) │    │  (Camera + Vulkan)  │             │
│  └──────────┬──────────┘    └──────────┬──────────┘             │
│             │                          │                        │
│             ▼                          ▼                        │
│       ┌──────────┐               ┌──────────┐                   │
│       │ Surface  │◄─────────────│ Surface  │                   │
│       │  View    │               │  (Camera)│                   │
│       └────┬─────┘               └────┬─────┘                   │
│            │                          │                         │
└────────────┼──────────────────────────┼─────────────────────────┘
             │                          │
             │ JNI (Surface pointer)    │
             │                          │
├────────────┼──────────────────────────┼─────────────────────────┤
│            ▼                          ▼                        │
│  ┌──────────────────────────────────────────────┐              │
│  │     AndroidSurfacePlatform (C++)              │              │
│  │  - Receives Surface from Java via JNI         │              │
│  │  - Creates ANativeWindow from Surface         │              │
│  │  - Manages Vulkan Surface lifecycle           │              │
│  └──────────────┬───────────────────────────────┘              │
│                 │                                               │
│                 ▼                                               │
│  ┌──────────────────────────────────────────────┐              │
│  │        Existing Vulkan Framework              │              │
│  │  - Instance, Device, Swapchain, etc.          │              │
│  └──────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────────┘
```

## Files Added

### Java Files
- `app/android/java/com/khronos/vulkan_samples/VulkanSurfaceActivity.java` - Base activity for Surface-based rendering
- `app/android/java/com/khronos/vulkan_samples/VulkanCameraActivity.java` - Camera preview example

### C++ Files
- `framework/platform/android/android_surface_platform.h` - Header for Surface-based platform
- `framework/platform/android/android_surface_platform.cpp` - Implementation of Surface-based platform

### Modified Files
- `framework/platform/android/android_window.h` - Added support for AndroidSurfacePlatform
- `framework/platform/android/android_window.cpp` - Modified to handle both platform types

## Usage

### 1. Basic VulkanSurfaceActivity

```java
// In your AndroidManifest.xml
<activity
    android:name=".VulkanSurfaceActivity"
    android:theme="@style/Theme.AppCompat.NoActionBar"
    android:screenOrientation="landscape">
</activity>

// Launch from another activity
Intent intent = new Intent(context, VulkanSurfaceActivity.class);
startActivity(intent);
```

### 2. Custom Activity with Arguments

```java
public class MyVulkanActivity extends VulkanSurfaceActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Set sample arguments
        setArguments(new String[]{"sample", "hello_triangle"});
    }
}
```

### 3. Camera Rendering Activity

```java
// Launch camera activity
Intent intent = new Intent(context, VulkanCameraActivity.class);
startActivity(intent);
```

The `VulkanCameraActivity`:
- Sets up Camera2 API
- Creates a Surface from SurfaceView
- Passes the Surface to native Vulkan code
- Native code receives camera frames and renders using Vulkan

## JNI Methods

The following JNI methods bridge Java Surface to native ANativeWindow:

```cpp
// Initialize native platform
void nativeInit(String externalDir, String tempDir);

// Set the Surface from Java
void nativeSetSurface(Surface surface);

// Surface lifecycle callbacks
void nativeSurfaceCreated(int width, int height);
void nativeSurfaceChanged(int width, int height);
void nativeSurfaceDestroyed();

// Start rendering
void nativeRun(String[] args);

// Cleanup
void nativeCleanup();
```

## CMake Integration

Add to your CMakeLists.txt:

```cmake
# Include Android Surface Platform sources
list(APPEND SOURCES
    framework/platform/android/android_surface_platform.cpp
)

# Link against required libraries
target_link_libraries(vulkan_samples
    android
    log
)
```

## Build Requirements

- Android SDK with Camera2 support (API 21+)
- Vulkan SDK
- CMake 3.10+
- Android NDK with Vulkan support

## AndroidManifest.xml Requirements

```xml
<!-- Permissions -->
<uses-permission android:name="android.permission.CAMERA" />
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />

<!-- Features (optional but recommended) -->
<uses-feature android:name="android.hardware.camera" android:required="false" />
<uses-feature android:name="android.hardware.vulkan.level" android:required="true" />

<!-- Activities -->
<activity
    android:name=".VulkanSurfaceActivity"
    android:theme="@style/Theme.AppCompat.NoActionBar"
    android:screenOrientation="landscape"
    android:configChanges="orientation|screenSize|keyboardHidden">
</activity>
```

## Camera Integration Notes

The `VulkanCameraActivity` demonstrates how to:
1. Use Android Camera2 API to capture frames
2. Create a Surface that can be used by both Camera and Vulkan
3. Pass camera frames to native code for Vulkan processing

For actual camera frame processing in Vulkan, you would need to:
1. Create an `ImageReader` to receive camera frames
2. Convert camera frames (YUV) to a format Vulkan can use (RGB/RGBA)
3. Upload to Vulkan texture
4. Render the texture to the Surface

## Troubleshooting

### Surface not created
- Ensure SurfaceView is attached to window
- Check that SurfaceHolder.Callback is registered
- Verify native library is loaded before setting Surface

### ANativeWindow is null
- Check Surface validity in Java before passing to native
- Ensure Surface is not destroyed when passed to native
- Verify JNI method signatures match

### Camera preview not showing
- Check camera permissions are granted
- Verify camera is opened after Surface is created
- Check that Surface size matches camera preview size

## Differences from NativeActivity

| Feature | NativeActivity | VulkanSurfaceActivity |
|---------|---------------|----------------------|
| Activity Type | NativeActivity | AppCompatActivity |
| Input Events | Native (AInputEvent) | Java callbacks |
| Window Management | android_app | SurfaceHolder |
| Lifecycle | android_app_cmd | Activity lifecycle |
| JNI Bridge | Minimal | Required |

## Benefits

1. **Flexibility** - Can use any Android UI components alongside Vulkan
2. **Camera Integration** - Easy integration with Camera2 API
3. **Standard Android** - Follows standard Activity lifecycle
4. **Interoperability** - Can mix Vulkan with other Android APIs
5. **No NativeActivity Limitations** - Full control over window and lifecycle

## Limitations

1. Requires JNI bridge for event handling
2. Slightly more complex setup than NativeActivity
3. Need to manage Surface lifecycle manually
4. Input events need custom handling

## Future Enhancements

1. Add support for multiple Surfaces (PIP)
2. Hardware buffer import for zero-copy camera rendering
3. Support for SurfaceTexture (OpenGL interop)
4. Jetpack Compose integration
