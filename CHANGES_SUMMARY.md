## Summary of Changes

This modification allows the Vulkan Samples framework to work with regular Android Activities by passing a Surface from Java to native code, instead of relying on NativeActivity.

### New Files Created:

1. **Java Activities:**
   - `VulkanSurfaceActivity.java` - Base activity for Surface-based rendering
   - `VulkanCameraActivity.java` - Camera preview example with Vulkan

2. **C++ Platform:**
   - `android_surface_platform.h` - Header for Surface-based platform
   - `android_surface_platform.cpp` - JNI implementation and platform logic

3. **Modified Files:**
   - `android_window.h` - Added support for AndroidSurfacePlatform
   - `android_window.cpp` - Modified to handle both platform types

4. **Documentation & Examples:**
   - `ANDROID_SURFACE_PLATFORM.md` - Complete usage documentation
   - `camera_vulkan.cpp` - Example native sample for camera rendering
   - `camera_texture.glsl` - Shader code for camera texture rendering

### Key Changes:

1. **Surface-based ANativeWindow creation:**
   ```cpp
   native_window = ANativeWindow_fromSurface(env, surface);
   ```

2. **JNI bridge between Java and Native:**
   - `nativeSetSurface()` - Pass Surface from Java to native
   - `nativeSurfaceCreated()` - Called when Surface is ready
   - `nativeRun()` - Start Vulkan rendering

3. **Activity inheritance:**
   - Old: `extends NativeActivity`
   - New: `extends AppCompatActivity`

### How to Use:

```java
// Basic usage
public class MyActivity extends VulkanSurfaceActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setArguments(new String[]{"sample", "hello_triangle"});
    }
}

// Camera usage
Intent intent = new Intent(context, VulkanCameraActivity.class);
startActivity(intent);
```

### Build Instructions:

1. Add new source files to CMakeLists.txt
2. Update AndroidManifest.xml with new activities
3. Ensure Vulkan and Camera2 permissions are declared
4. Build and run on Android device with Vulkan support
