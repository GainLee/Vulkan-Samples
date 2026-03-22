# CMake configuration for Android Surface-based Vulkan rendering
# Add these to your existing CMakeLists.txt

# Source files for Android Surface Platform
set(ANDROID_SURFACE_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/platform/android/android_surface_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/platform/android/android_window.cpp
)

# Add to your target sources
# target_sources(vulkan_samples PRIVATE ${ANDROID_SURFACE_PLATFORM_SOURCES})

# Java files for Android Surface Activity
set(ANDROID_SURFACE_JAVA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/android/java/com/khronos/vulkan_samples/VulkanSurfaceActivity.java
    ${CMAKE_CURRENT_SOURCE_DIR}/app/android/java/com/khronos/vulkan_samples/VulkanCameraActivity.java
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/platform/android
)

# Required libraries
find_library(ANDROID_LIBRARY android)
find_library(LOG_LIBRARY log)
find_library(GLESv2_LIBRARY GLESv2)

target_link_libraries(vulkan_samples
    ${ANDROID_LIBRARY}
    ${LOG_LIBRARY}
    # Note: camera2 is a Java API, link via Java not native
)

# Android manifest requirements for VulkanSurfaceActivity:
# Add these to your AndroidManifest.xml:
#
# <activity
#     android:name=".VulkanSurfaceActivity"
#     android:theme="@style/Theme.AppCompat.NoActionBar"
#     android:screenOrientation="landscape"
#     android:configChanges="orientation|screenSize">
# </activity>
#
# <activity
#     android:name=".VulkanCameraActivity"
#     android:theme="@style/Theme.AppCompat.NoActionBar"
#     android:screenOrientation="landscape"
#     android:configChanges="orientation|screenSize">
# </activity>
#
# Required permissions:
# <uses-permission android:name="android.permission.CAMERA" />
# <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
# <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
#
# Required features:
# <uses-feature android:name="android.hardware.camera" android:required="false" />
# <uses-feature android:name="android.hardware.camera2" android:required="false" />
