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

#include "android_surface_platform.h"

#include <chrono>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <android/configuration.h>

#include "apps.h"
#include "common/logging.h"
#include "platform/android/android_window.h"

// Forward declaration of global plugins namespace
namespace plugins {
extern std::vector<vkb::Plugin *> get_all();
}

namespace vkb
{
// Global platform instance for JNI callbacks
AndroidSurfacePlatform *g_surface_platform = nullptr;

// Static member to store JVM for JNI callbacks
static JavaVM *s_java_vm = nullptr;

extern "C"
{
	/**
	 * JNI Implementation for VulkanSurfaceActivity
	 */

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeInit(JNIEnv *env, jobject thiz,
	                                                                  jstring external_dir, jstring temp_dir)
	{
		const char *external_dir_cstr = env->GetStringUTFChars(external_dir, 0);
		Platform::set_external_storage_directory(std::string(external_dir_cstr) + "/");
		env->ReleaseStringUTFChars(external_dir, external_dir_cstr);

		const char *temp_dir_cstr = env->GetStringUTFChars(temp_dir, 0);
		Platform::set_temp_directory(std::string(temp_dir_cstr) + "/");
		env->ReleaseStringUTFChars(temp_dir, temp_dir_cstr);

		// Create global platform instance
		if (!g_surface_platform)
		{
			g_surface_platform = new AndroidSurfacePlatform();
		}

		// Store JVM for later use
		env->GetJavaVM(&s_java_vm);
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeSetSurface(JNIEnv *env, jobject thiz, jobject surface)
	{
		if (g_surface_platform)
		{
			g_surface_platform->set_surface(env, surface);
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeSurfaceCreated(JNIEnv *env, jobject thiz,
	                                                                              jint width, jint height)
	{
		if (g_surface_platform)
		{
			g_surface_platform->on_surface_created(width, height);
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeSurfaceChanged(JNIEnv *env, jobject thiz,
	                                                                              jint width, jint height)
	{
		if (g_surface_platform)
		{
			g_surface_platform->on_surface_changed(width, height);
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeSurfaceDestroyed(JNIEnv *env, jobject thiz)
	{
		if (g_surface_platform)
		{
			g_surface_platform->on_surface_destroyed();
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeRun(JNIEnv *env, jobject thiz, jobjectArray arg_strings)
	{
		std::vector<std::string> args;

		if (arg_strings)
		{
			jsize len = env->GetArrayLength(arg_strings);
			for (jsize i = 0; i < len; i++)
			{
				jstring arg_string = (jstring)(env->GetObjectArrayElement(arg_strings, i));
				const char *arg = env->GetStringUTFChars(arg_string, 0);
				args.push_back(std::string(arg));
				env->ReleaseStringUTFChars(arg_string, arg);
				env->DeleteLocalRef(arg_string);
			}
		}

		if (g_surface_platform)
		{
			// Get plugins from the plugins registry
			std::vector<Plugin *> plugins = ::plugins::get_all();
			g_surface_platform->run(args, plugins);
		}
	}

	JNIEXPORT void JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_nativeCleanup(JNIEnv *env, jobject thiz)
	{
		if (g_surface_platform)
		{
			g_surface_platform->terminate(ExitCode::Success);
			delete g_surface_platform;
			g_surface_platform = nullptr;
		}
	}

	JNIEXPORT jobjectArray JNICALL
	Java_com_khronos_vulkan_1samples_VulkanSurfaceActivity_getSamples(JNIEnv *env, jobject thiz)
	{
		auto sample_list = apps::get_samples();

		jclass c = env->FindClass("com/khronos/vulkan_samples/model/Sample");
		jmethodID constructor = env->GetMethodID(c, "<init>",
		                                         "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V");
		jobjectArray j_sample_list = env->NewObjectArray(sample_list.size(), c, 0);

		for (int sample_index = 0; sample_index < sample_list.size(); sample_index++)
		{
			const apps::SampleInfo *sample_info = reinterpret_cast<const apps::SampleInfo *>(sample_list[sample_index]);

			jstring id = env->NewStringUTF(sample_info->id.c_str());
			jstring category = env->NewStringUTF(sample_info->category.c_str());
			jstring author = env->NewStringUTF(sample_info->author.c_str());
			jstring name = env->NewStringUTF(sample_info->name.c_str());
			jstring desc = env->NewStringUTF(sample_info->description.c_str());

			jobjectArray j_tag_list = env->NewObjectArray(
			    sample_info->tags.size(), env->FindClass("java/lang/String"), env->NewStringUTF(""));
			for (int tag_index = 0; tag_index < sample_info->tags.size(); ++tag_index)
			{
				env->SetObjectArrayElement(j_tag_list, tag_index,
				                           env->NewStringUTF(sample_info->tags[tag_index].c_str()));
			}

			env->SetObjectArrayElement(j_sample_list, sample_index,
			                           env->NewObject(c, constructor, id, category, author, name, desc, j_tag_list));
		}

		return j_sample_list;
	}
}

AndroidSurfacePlatform::AndroidSurfacePlatform() :
    Platform(),
    native_window(nullptr),
    surface_ready(false),
    running(false),
    jni_env(nullptr),
    java_surface(nullptr),
    java_vm(nullptr)
{
}

ExitCode AndroidSurfacePlatform::initialize(const std::vector<Plugin *> &plugins)
{
	// Wait for surface to be ready before initializing
	LOGI("Waiting for Android Surface to be ready...");
	wait_for_surface();

	auto code = Platform::initialize(plugins);
	if (code != ExitCode::Success)
	{
		return code;
	}

	return ExitCode::Success;
}

void AndroidSurfacePlatform::create_window(const Window::Properties &properties)
{
	// Create AndroidWindow with the native window
	window = std::make_unique<AndroidWindow>(this, native_window, properties);
}

void AndroidSurfacePlatform::set_surface(JNIEnv *env, jobject surface)
{
	std::lock_guard<std::mutex> lock(surface_mutex);

	if (native_window)
	{
		// Release previous window
		ANativeWindow_release(native_window);
		native_window = nullptr;
	}

	if (surface)
	{
		// Create native window from Java Surface
		native_window = ANativeWindow_fromSurface(env, surface);
		if (native_window)
		{
			LOGI("Created ANativeWindow from Surface");
		}
		else
		{
			LOGE("Failed to create ANativeWindow from Surface");
		}
	}
}

void AndroidSurfacePlatform::on_surface_created(int width, int height)
{
	{
		std::lock_guard<std::mutex> lock(surface_mutex);
		surface_ready = true;
	}

	resize(width, height);
	surface_cv.notify_all();

	LOGI("Surface created: {}x{}", width, height);
}

void AndroidSurfacePlatform::on_surface_changed(int width, int height)
{
	resize(width, height);
	LOGI("Surface changed: {}x{}", width, height);
}

void AndroidSurfacePlatform::on_surface_destroyed()
{
	{
		std::lock_guard<std::mutex> lock(surface_mutex);
		surface_ready = false;

		if (native_window)
		{
			ANativeWindow_release(native_window);
			native_window = nullptr;
		}
	}

	LOGI("Surface destroyed");
}

void AndroidSurfacePlatform::run(const std::vector<std::string> &args, const std::vector<Plugin *> &plugins)
{
	set_arguments(args);

	// Use provided plugins or empty list
	std::vector<Plugin *> plugins_to_use = plugins;

	// Initialize platform
	ExitCode code = initialize(plugins_to_use);
	if (code == ExitCode::Success)
	{
		running = true;
		main_loop();
	}
	else
	{
		LOGE("Platform initialization failed");
	}
}

ANativeWindow *AndroidSurfacePlatform::get_native_window() const
{
	return native_window;
}

bool AndroidSurfacePlatform::is_surface_ready() const
{
	std::lock_guard<std::mutex> lock(surface_mutex);
	return surface_ready && native_window != nullptr;
}

void AndroidSurfacePlatform::wait_for_surface()
{
	std::unique_lock<std::mutex> lock(surface_mutex);
	surface_cv.wait(lock, [this] { return surface_ready && native_window != nullptr; });
}

void AndroidSurfacePlatform::terminate(ExitCode code)
{
	running = false;

	// Clean up native window
	{
		std::lock_guard<std::mutex> lock(surface_mutex);
		if (native_window)
		{
			ANativeWindow_release(native_window);
			native_window = nullptr;
		}
		surface_ready = false;
	}

	Platform::terminate(code);
}

const char *AndroidSurfacePlatform::get_surface_extension()
{
	return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
}

std::vector<spdlog::sink_ptr> AndroidSurfacePlatform::get_platform_sinks()
{
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>("VulkanSurface"));

	// Add file sink for debugging
	char timestamp[80];
	std::time_t time = std::time(0);
	std::tm now = *std::localtime(&time);
	std::strftime(timestamp, 80, "%Y-%m-%d_%H-%M-%S_log.txt", &now);
	log_output = Platform::get_external_storage_directory() + std::string(timestamp);
	sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_output, true));

	return sinks;
}

} // namespace vkb
