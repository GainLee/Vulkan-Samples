/* Copyright (c) 2019-2024, Arm Limited and Contributors
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

#include <platform/android/external_surface_android_platform.h>

#include <core/util/logging.hpp>
#include <core/platform/entrypoint.hpp>
#include <filesystem/filesystem.hpp>

#include "../plugins/plugins.h"
#include "../apps/apps.h"

#include <android/context.hpp>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <jni.h>

#include <chrono>
#include <thread>
#include <memory>
#include <mutex>
#include <filesystem>

// spdlog includes
#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

// Global state for JNI mode
static ANativeWindow* g_pending_surface = nullptr;
static std::mutex g_surface_mutex;

// Initialize logging system safely (avoid duplicate initialization)
void init_jni_logging()
{
	static std::once_flag logging_init_flag;
	std::call_once(logging_init_flag, []() {
		// Create Android logcat sink with thread safety
		auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>("vulkan_samples");
		
		// Create logger with the sink
		auto logger = std::make_shared<spdlog::logger>("vkb", android_sink);
		logger->set_level(spdlog::level::debug);
		logger->set_pattern("[%L] %v");
		
		// Register the logger globally
		spdlog::register_logger(logger);
		spdlog::set_default_logger(logger);
		
		// Ensure the logger is flushed regularly
		spdlog::flush_every(std::chrono::seconds(1));
	});
}

extern "C"
{
	JNIEXPORT jstring JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeGetExternalStoragePath(JNIEnv *env, jobject /* thiz */)
	{
		init_jni_logging();
		LOGD("JNI: nativeGetExternalStoragePath called");
		return env->NewStringUTF("/data/data/com.khronos.vulkan_samples/files");
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeSetSurface(JNIEnv *env, jobject /* thiz */, jobject surface)
	{
		init_jni_logging();
		
		if (!env) {
			LOGE("JNI: Invalid JNI environment");
			return;
		}
		
		LOGI("JNI: nativeSetSurface called with surface: {}", surface ? "valid" : "null");
		
		std::lock_guard<std::mutex> lock(g_surface_mutex);
		
		// Release previous surface if any
		if (g_pending_surface)
		{
			ANativeWindow_release(g_pending_surface);
			g_pending_surface = nullptr;
			LOGD("JNI: Released previous pending surface");
		}
		
		if (surface)
		{
			ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
			if (native_window)
			{
				// Validate surface dimensions
				int32_t width = ANativeWindow_getWidth(native_window);
				int32_t height = ANativeWindow_getHeight(native_window);
				int32_t format = ANativeWindow_getFormat(native_window);
				
				if (width <= 0 || height <= 0) {
					LOGE("JNI: Invalid surface dimensions: {}x{}", width, height);
					ANativeWindow_release(native_window);
					return;
				}
				
				// Store the surface for later use
				g_pending_surface = native_window;
				ANativeWindow_acquire(g_pending_surface);
				
				LOGI("JNI: Surface stored - dimensions: {}x{}, format: {}", width, height, format);
				
				// If platform is already created, set it immediately
				auto platform = vkb::ExternalSurfaceAndroidPlatform::get_jni_platform_instance();
				if (platform)
				{
					platform->set_external_surface(g_pending_surface);
					LOGI("JNI: Surface applied to existing platform instance");
				}
				else
				{
					LOGD("JNI: Platform not yet created, surface will be applied later");
				}
			}
			else
			{
				LOGE("JNI: Failed to get native window from surface");
			}
		}
		else
		{
			LOGI("JNI: Surface cleared (null surface passed)");
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeReleaseSurface(JNIEnv *env, jobject /* thiz */)
	{
		init_jni_logging();
		LOGI("JNI: nativeReleaseSurface called");
		
		std::lock_guard<std::mutex> lock(g_surface_mutex);
		
		// Clear from platform if exists
		auto platform = vkb::ExternalSurfaceAndroidPlatform::get_jni_platform_instance();
		if (platform)
		{
			platform->set_external_surface(nullptr);
			platform->request_application(nullptr);
			LOGI("JNI: Surface released from platform instance");
		}
		else
		{
			LOGD("JNI: No platform instance found to release surface from");
		}
		
		// Release pending surface
		if (g_pending_surface)
		{
			ANativeWindow_release(g_pending_surface);
			g_pending_surface = nullptr;
			LOGI("JNI: Pending surface released and cleared");
		}
		else
		{
			LOGD("JNI: No pending surface to release");
		}
	}

	// Global variables for image data
	static std::vector<uint8_t> g_image_data;
	static uint32_t g_image_width = 0;
	static uint32_t g_image_height = 0;
	static std::mutex g_image_mutex;

	// Global flag to indicate texture update is needed
	static bool g_texture_update_needed = false;
	static std::mutex g_texture_update_mutex;

	// Forward declaration of texture update function
	void notify_texture_update_needed();

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeSetImageData(JNIEnv *env, jobject /* thiz */, jobject bitmap)
	{
		init_jni_logging();
		
		if (!bitmap) {
			LOGE("JNI: nativeSetImageData called with null bitmap");
			return;
		}
		
		// Get bitmap info
		AndroidBitmapInfo bitmapInfo;
		int result = AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);
		if (result != ANDROID_BITMAP_RESULT_SUCCESS) {
			LOGE("JNI: Failed to get bitmap info, result: {}", result);
			return;
		}
		
		// Check bitmap format
		if (bitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
			LOGE("JNI: Unsupported bitmap format: {}, expected RGBA_8888", bitmapInfo.format);
			return;
		}
		
		LOGI("JNI: nativeSetImageData called - width: {}, height: {}, format: {}", 
			 bitmapInfo.width, bitmapInfo.height, bitmapInfo.format);
		
		// Lock bitmap pixels
		void* bitmapPixels;
		result = AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels);
		if (result != ANDROID_BITMAP_RESULT_SUCCESS) {
			LOGE("JNI: Failed to lock bitmap pixels, result: {}", result);
			return;
		}
		
		std::lock_guard<std::mutex> lock(g_image_mutex);
		
		// Calculate data size
		size_t dataSize = bitmapInfo.width * bitmapInfo.height * 4; // RGBA
		
		// Copy the bitmap data to our global buffer
		g_image_data.clear();
		g_image_data.resize(dataSize);
		std::memcpy(g_image_data.data(), bitmapPixels, dataSize);
		g_image_width = bitmapInfo.width;
		g_image_height = bitmapInfo.height;
		
		// Unlock bitmap pixels
		AndroidBitmap_unlockPixels(env, bitmap);
		
		LOGI("JNI: Bitmap data stored successfully - {}x{}, {} bytes", g_image_width, g_image_height, g_image_data.size());
		
		// Notify that texture needs update
		notify_texture_update_needed();
	}

	// Global platform instance for split init/render
	static std::unique_ptr<vkb::ExternalSurfaceAndroidPlatform> g_platform_instance = nullptr;
	static std::mutex g_platform_mutex;

	JNIEXPORT jboolean JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeInitSample(JNIEnv *env, jobject /* thiz */, jobject jAsert, jobjectArray args)
	{
		// Initialize logging system first (safe from multiple calls)
		init_jni_logging();
		
		// Validate JNI environment
		if (!env) {
			LOGE("JNI: Invalid JNI environment in nativeInitSample");
			return JNI_FALSE;
		}
		
		LOGI("JNI: nativeInitSample called");
		
		std::lock_guard<std::mutex> lock(g_platform_mutex);
		
		// Clean up any existing platform instance
		if (g_platform_instance) {
			LOGW("JNI: Cleaning up existing platform instance");
			try {
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
				g_platform_instance->terminate(vkb::ExitCode::Success);
				g_platform_instance.reset();
			} catch (...) {
				LOGE("JNI: Error during platform cleanup");
			}
		}
		
		// Convert jobjectArray to std::vector<std::string>
		std::vector<std::string> arg_strings;
		
		if (args != nullptr)
		{
			jsize length = env->GetArrayLength(args);
			LOGD("JNI: Processing {} arguments", length);
			
			for (jsize i = 0; i < length; ++i)
			{
				jstring j_string = (jstring) env->GetObjectArrayElement(args, i);
				if (j_string != nullptr)
				{
					const char* c_string = env->GetStringUTFChars(j_string, nullptr);
					if (c_string != nullptr) {
						arg_strings.push_back(std::string(c_string));
						env->ReleaseStringUTFChars(j_string, c_string);
					}
					env->DeleteLocalRef(j_string);
				}
			}
		}
		
		// If no args provided, use default
		if (arg_strings.empty())
		{
			arg_strings.push_back("vulkan_samples");
			LOGD("JNI: No arguments provided, using default");
		}

		LOGI("JNI: Initializing sample with {} arguments", arg_strings.size());
		for (size_t i = 0; i < arg_strings.size(); ++i)
		{
			LOGI("JNI: arg[{}] = '{}'", i, arg_strings[i]);
		}
		
		// Validate that we have a surface before proceeding
		bool has_surface = false;
		{
			std::lock_guard<std::mutex> surface_lock(g_surface_mutex);
			has_surface = (g_pending_surface != nullptr);
			if (has_surface)
			{
				LOGI("JNI: Using pending surface: {}", static_cast<void*>(g_pending_surface));
			}
			else
			{
				LOGE("JNI: No pending surface available - cannot proceed");
				return JNI_FALSE;
			}
		}
		
		try
		{
			// Create external surface platform context
			LOGD("JNI: Creating platform context");
			AAssetManager *asset_manager = AAssetManager_fromJava(env, jAsert);
			auto context = create_platform_context(asset_manager);
			
			// Initialize filesystem
			LOGD("JNI: Initializing filesystem");
			vkb::filesystem::init_with_context(*context);
			
			// Create platform
			LOGD("JNI: Creating platform");
			g_platform_instance = std::make_unique<vkb::ExternalSurfaceAndroidPlatform>(*context);
			
			// Apply pending surface if available
			{
				std::lock_guard<std::mutex> surface_lock(g_surface_mutex);
				if (g_pending_surface)
				{
					g_platform_instance->set_external_surface(g_pending_surface);
					LOGI("JNI: Applied pending surface to platform");
				}
				else
				{
					LOGE("JNI: Surface was lost during platform creation");
					g_platform_instance.reset();
					return JNI_FALSE;
				}
			}
			
			// Set global platform instance for JNI callbacks
			vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(g_platform_instance.get());
			
			std::string sample_name = arg_strings.empty() ? "vulkan_samples" : arg_strings[0];
			LOGI("JNI: Initializing sample: {}", sample_name);
			
			// Initialize platform with plugins (but don't start main loop)
			LOGD("JNI: Initializing platform with plugins");
			auto code = g_platform_instance->initialize(plugins::get_all());
			if (code == vkb::ExitCode::Success)
			{
				// Request the application to run
				std::string sample_name = arg_strings.size() > 1 ? arg_strings[1] : "hello_triangle";
				LOGI("JNI: Requesting application: {}", sample_name);
				
				auto *app_info = apps::get_app(sample_name);
				if (app_info)
				{
					g_platform_instance->request_application(app_info);
					LOGI("JNI: Platform initialized successfully with app: {}", sample_name);
					return JNI_TRUE;
				}
				else
				{
					LOGE("JNI: Failed to find application: {}", sample_name);
					g_platform_instance->terminate(code);
					g_platform_instance.reset();
					vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
					return JNI_FALSE;
				}
			}
			else
			{
				LOGE("JNI: Platform initialization failed with code: {}", static_cast<int>(code));
				g_platform_instance->terminate(code);
				g_platform_instance.reset();
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
				return JNI_FALSE;
			}
		}
		catch (const std::runtime_error& e)
		{
			LOGE("JNI: Runtime error: {}", e.what());
			if (g_platform_instance) {
				g_platform_instance.reset();
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
			}
			return JNI_FALSE;
		}
		catch (const std::exception& e)
		{
			LOGE("JNI: Exception: {}", e.what());
			if (g_platform_instance) {
				g_platform_instance.reset();
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
			}
			return JNI_FALSE;
		}
		catch (...)
		{
			LOGE("JNI: Unknown exception occurred");
			if (g_platform_instance) {
				g_platform_instance.reset();
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
			}
			return JNI_FALSE;
		}
	}

	JNIEXPORT jboolean JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeRenderFrame(JNIEnv *env, jobject /* thiz */)
	{
		std::lock_guard<std::mutex> lock(g_platform_mutex);
		
		if (!g_platform_instance) {
			LOGE("JNI: No platform instance available for rendering");
			return JNI_FALSE;
		}
		
		try {
			LOGI("JNI: Rendering frame");
			// Render one frame
			vkb::ExitCode code = g_platform_instance->main_loop();
			if (code != vkb::ExitCode::Success) {
				LOGE("JNI: Frame update failed with code: {}", static_cast<int>(code));
				return JNI_FALSE;
			}
			return JNI_TRUE;
		}
		catch (const std::exception& e) {
			LOGE("JNI: Exception during frame render: {}", e.what());
			return JNI_FALSE;
		}
		catch (...) {
			LOGE("JNI: Unknown exception during frame render");
			return JNI_FALSE;
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeTerminateSample(JNIEnv *env, jobject /* thiz */)
	{
		LOGI("JNI: nativeTerminateSample called");
		
		std::lock_guard<std::mutex> lock(g_platform_mutex);
		
		if (g_platform_instance) {
			try {
				LOGD("JNI: Terminating platform");
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
				g_platform_instance->terminate(vkb::ExitCode::Success);
				g_platform_instance.reset();
				LOGI("JNI: Platform terminated successfully");
			} catch (const std::exception& e) {
				LOGE("JNI: Exception during platform termination: {}", e.what());
				g_platform_instance.reset();
			} catch (...) {
				LOGE("JNI: Unknown exception during platform termination");
				g_platform_instance.reset();
			}
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_SurfaceSampleActivity_nativeRunSample(JNIEnv *env, jobject /* thiz */, jobject jAsert, jobjectArray args)
	{
		// Initialize logging system first (safe from multiple calls)
		init_jni_logging();
		
		// Validate JNI environment
		if (!env) {
			LOGE("JNI: Invalid JNI environment in nativeRunSample");
			return;
		}
		
		LOGI("JNI: nativeRunSample called");
		
		// Convert jobjectArray to std::vector<std::string>
		std::vector<std::string> arg_strings;
		
		if (args != nullptr)
		{
			jsize length = env->GetArrayLength(args);
			LOGD("JNI: Processing {} arguments", length);
			
			for (jsize i = 0; i < length; ++i)
			{
				jstring j_string = (jstring) env->GetObjectArrayElement(args, i);
				if (j_string != nullptr)
				{
					const char* c_string = env->GetStringUTFChars(j_string, nullptr);
					if (c_string != nullptr) {
						arg_strings.push_back(std::string(c_string));
						env->ReleaseStringUTFChars(j_string, c_string);
					}
					env->DeleteLocalRef(j_string);
				}
			}
		}
		
		// If no args provided, use default
		if (arg_strings.empty())
		{
			arg_strings.push_back("vulkan_samples");
			LOGD("JNI: No arguments provided, using default");
		}

		LOGI("JNI: Starting sample with {} arguments", arg_strings.size());
		for (size_t i = 0; i < arg_strings.size(); ++i)
		{
			LOGI("JNI: arg[{}] = '{}'", i, arg_strings[i]);
		}
		
		// Validate that we have a surface before proceeding
		bool has_surface = false;
		{
			std::lock_guard<std::mutex> lock(g_surface_mutex);
			has_surface = (g_pending_surface != nullptr);
			if (has_surface)
			{
				LOGI("JNI: Using pending surface: {}", static_cast<void*>(g_pending_surface));
			}
			else
			{
				LOGE("JNI: No pending surface available - cannot proceed");
				return;
			}
		}
		
		std::unique_ptr<vkb::ExternalSurfaceAndroidPlatform> platform;
		
		try
		{
			// Create external surface platform context
			LOGD("JNI: Creating platform context");
			AAssetManager *asset_manager = AAssetManager_fromJava(env, jAsert);
			auto context = create_platform_context(asset_manager);
			
			// Initialize filesystem
			LOGD("JNI: Initializing filesystem");
			vkb::filesystem::init_with_context(*context);
			
			// Create platform
			LOGD("JNI: Creating platform");
			platform = std::make_unique<vkb::ExternalSurfaceAndroidPlatform>(*context);
			
			// Apply pending surface if available
			{
				std::lock_guard<std::mutex> lock(g_surface_mutex);
				if (g_pending_surface)
				{
					platform->set_external_surface(g_pending_surface);
					LOGI("JNI: Applied pending surface to platform");
				}
				else
				{
					LOGE("JNI: Surface was lost during platform creation");
					return;
				}
			}
			
			// Set global platform instance for JNI callbacks
			vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(platform.get());
			
			std::string sample_name = arg_strings.empty() ? "vulkan_samples" : arg_strings[0];
			LOGI("JNI: Running sample: {}", sample_name);
			
			// Initialize platform with plugins
			LOGD("JNI: Initializing platform with plugins");
			auto code = platform->initialize(plugins::get_all());
			if (code == vkb::ExitCode::Success)
			{
				LOGI("JNI: Platform initialized successfully, starting main loop");
				// Run the main loop
				code = platform->main_loop();
				LOGI("JNI: Main loop finished with code: {}", static_cast<int>(code));
			}
			else
			{
				LOGE("JNI: Platform initialization failed with code: {}", static_cast<int>(code));
			}
			
			// Terminate platform
			LOGD("JNI: Terminating platform");
			platform->terminate(code);
			
			LOGI("JNI: Sample finished with exit code: {}", static_cast<int>(code));
		}
		catch (const std::runtime_error& e)
		{
			LOGE("JNI: Runtime error: {}", e.what());
		}
		catch (const std::exception& e)
		{
			LOGE("JNI: Exception: {}", e.what());
		}
		catch (...)
		{
			LOGE("JNI: Unknown exception occurred");
		}
		
		// Clean up - ensure this always happens
		try
		{
			if (platform)
			{
				vkb::ExternalSurfaceAndroidPlatform::set_jni_platform_instance(nullptr);
				LOGD("JNI: Platform instance cleared");
			}
		}
		catch (...)
		{
			LOGE("JNI: Error during cleanup");
		}
		
		LOGI("JNI: nativeRunSample completed");
	}
}

extern "C" {
	// Function to get image data from native code
	bool get_jni_image_data(std::vector<uint8_t>& data, uint32_t& width, uint32_t& height)
	{
		std::lock_guard<std::mutex> lock(g_image_mutex);
		
		if (g_image_data.empty()) {
			return false;
		}
		
		data = g_image_data;
		width = g_image_width;
		height = g_image_height;
		
		return true;
	}

	// Function to notify texture update needed
	void notify_texture_update_needed()
	{
		std::lock_guard<std::mutex> lock(g_texture_update_mutex);
		g_texture_update_needed = true;
		LOGI("JNI: Texture update notification sent");
	}

	// Function to check and reset texture update flag
	bool check_texture_update_needed()
	{
		std::lock_guard<std::mutex> lock(g_texture_update_mutex);
		bool needed = g_texture_update_needed;
		g_texture_update_needed = false;
		return needed;
	}
}