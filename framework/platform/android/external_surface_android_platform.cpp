/* Copyright (c) 2019-2025, Arm Limited and Contributors
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

#include "external_surface_android_platform.h"

#include <chrono>
#include <unistd.h>
#include <unordered_map>

#include <android/external_surface_context.hpp>

#include "common/error.h"

#include <fmt/format.h>
#include <imgui.h>
#include <jni.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "apps.h"
#include "common/strings.h"
#include "core/util/logging.hpp"
#include "platform/android/external_surface_android_window.h"
#include "platform/input_events.h"

#ifdef EXTERNAL_SURFACE
extern "C"
{
	JNIEXPORT jobjectArray JNICALL
	    Java_com_khronos_vulkan_1samples_SampleLauncherActivity_getSamples(JNIEnv *env, jobject thiz)
	{
		auto sample_list = apps::get_samples();

		jclass       c             = env->FindClass("com/khronos/vulkan_samples/model/Sample");
		jmethodID    constructor   = env->GetMethodID(c, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V");
		jobjectArray j_sample_list = env->NewObjectArray(sample_list.size(), c, 0);

		for (int sample_index = 0; sample_index < sample_list.size(); sample_index++)
		{
			const apps::SampleInfo *sample_info = reinterpret_cast<apps::SampleInfo *>(sample_list[sample_index]);

			jstring id       = env->NewStringUTF(sample_info->id.c_str());
			jstring category = env->NewStringUTF(sample_info->category.c_str());
			jstring author   = env->NewStringUTF(sample_info->author.c_str());
			jstring name     = env->NewStringUTF(sample_info->name.c_str());
			jstring desc     = env->NewStringUTF(sample_info->description.c_str());

			jobjectArray j_tag_list = env->NewObjectArray(sample_info->tags.size(), env->FindClass("java/lang/String"), env->NewStringUTF(""));
			for (int tag_index = 0; tag_index < sample_info->tags.size(); ++tag_index)
			{
				env->SetObjectArrayElement(j_tag_list, tag_index, env->NewStringUTF(sample_info->tags[tag_index].c_str()));
			}

			env->SetObjectArrayElement(j_sample_list, sample_index, env->NewObject(c, constructor, id, category, author, name, desc, j_tag_list));
		}

		return j_sample_list;
	}
}
#endif

namespace vkb
{
namespace
{
inline std::tm thread_safe_time(const std::time_t time)
{
	std::tm                     result;
	std::mutex                  mtx;
	std::lock_guard<std::mutex> lock(mtx);
	result = *std::localtime(&time);
	return result;
}

}        // namespace

ExternalSurfaceAndroidPlatform::ExternalSurfaceAndroidPlatform(const PlatformContext &context) :
    Platform{context}
{
	if (auto *android = dynamic_cast<const ExternalSurfacePlatformContext *>(&context))
	{
		asset_manager = android->asset_manager;
	}
}

ExitCode ExternalSurfaceAndroidPlatform::initialize(const std::vector<Plugin *> &plugins)
{
	for (auto plugin : plugins)
	{
		plugin->clear_platform();
	}

	auto code = Platform::initialize(plugins);
	if (code != ExitCode::Success)
	{
		return code;
	}

	if (!process_android_events(asset_manager))
	{
		// Android requested for the app to close
		LOGI("Android app has been destroyed by the OS");
		return ExitCode::Close;
	}

	return ExitCode::Success;
}

void ExternalSurfaceAndroidPlatform::set_external_surface(ANativeWindow *surface)
{
	external_surface = surface;
	
	// If surface is being cleared and we have a window, notify it to close
	if (!surface && window)
	{
		LOGI("ExternalSurfaceAndroidPlatform: Surface cleared, requesting window close");
		window->close();
	}
}

ANativeWindow *ExternalSurfaceAndroidPlatform::get_external_surface()
{
	return external_surface;
}

void ExternalSurfaceAndroidPlatform::create_window(const Window::Properties &properties)
{
	// TODO: Use ANativeWindow passed in from the Java side
	window = std::make_unique<ExternalSurfaceAndroidWindow>(this, external_surface, properties);
}

void ExternalSurfaceAndroidPlatform::process_android_input_events(void)
{
}

void ExternalSurfaceAndroidPlatform::terminate(ExitCode code)
{
	switch (code)
	{
		case ExitCode::Success:
		case ExitCode::Close:
			log_output.clear();
			break;
		case ExitCode::FatalError:
		{
			const std::string error_message = "Error! Could not launch selected sample:" + get_last_error();
			send_notification(error_message);
			break;
		}
		default:
			break;
	}

	while (process_android_events(asset_manager))
	{
		// Process events until app->destroyRequested is set
	}

	plugins.clear();
	Platform::terminate(code);
}

void ExternalSurfaceAndroidPlatform::send_notification(const std::string &message)
{
}

void ExternalSurfaceAndroidPlatform::set_surface_ready()
{
	LOGI("ExternalSurfaceAndroidPlatform::set_surface_ready");
	surface_ready = true;
}

AAssetManager *ExternalSurfaceAndroidPlatform::get_asset_manager()
{
	return asset_manager;
}

std::vector<spdlog::sink_ptr> ExternalSurfaceAndroidPlatform::get_platform_sinks()
{
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>(PROJECT_NAME));

	try {
		char        timestamp[80];	
		std::time_t time = std::time(0);
		std::tm     now  = thread_safe_time(time);
		std::strftime(timestamp, 80, "%G-%m-%d_%H-%M-%S_log.txt", &now);
		log_output = vkb::fs::path::get(vkb::fs::path::Logs) + std::string(timestamp);

		LOGI("ExternalSurfaceAndroidPlatform: log_output: {}", log_output);

		sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_output, true));
		LOGI("File logging enabled: {}", log_output);
	} catch (const std::exception& e) {
		// If we can't create file logger due to permissions, just use Android log
		LOGW("Could not create file logger: {}. Using Android log only.", e.what());
		log_output.clear();
	}

	return sinks;
}

// Static member definition
ExternalSurfaceAndroidPlatform *ExternalSurfaceAndroidPlatform::jni_platform_instance = nullptr;

void ExternalSurfaceAndroidPlatform::set_jni_platform_instance(ExternalSurfaceAndroidPlatform *platform)
{
	jni_platform_instance = platform;
}

ExternalSurfaceAndroidPlatform *ExternalSurfaceAndroidPlatform::get_jni_platform_instance()
{
	return jni_platform_instance;
}

}        // namespace vkb
