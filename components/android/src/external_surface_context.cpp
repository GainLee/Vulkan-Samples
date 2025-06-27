/* Copyright (c) 2023-2024, Thomas Atkinson
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

#include "android/external_surface_context.hpp"

#include <core/util/logging.hpp>
#include <jni.h>

#ifdef EXTERNAL_SURFACE
extern "C"
{
	// TODO: Arguments can be parsed from the bundle
	JNIEXPORT void JNICALL
	    Java_com_khronos_vulkan_1samples_SampleLauncherActivity_sendArgumentsToPlatform(JNIEnv *env, jobject thiz, jobjectArray arg_strings)
	{
		std::vector<std::string> args;

		for (int i = 0; i < env->GetArrayLength(arg_strings); i++)
		{
			jstring arg_string = (jstring) (env->GetObjectArrayElement(arg_strings, i));

			const char *arg = env->GetStringUTFChars(arg_string, 0);

			args.push_back(std::string(arg));

			env->ReleaseStringUTFChars(arg_string, arg);
		}

		LOGI("JNI: Arguments:");
		for (const auto &arg : args)
		{
			LOGI("  {}", arg);
		}

		vkb::ExternalSurfacePlatformContext::android_arguments = args;
	}
}
#endif

namespace details
{
std::string get_external_storage_directory(AAssetManager* asset_manager) {
    // 返回外部存储路径，与CMake同步脚本保持一致
    return "/sdcard/Android/data/com.khronos.vulkan_samples/files";
}

std::string get_external_cache_directory(AAssetManager* asset_manager) {
    // 简化实现：直接返回内部缓存路径
    return "/sdcard/Android/data/com.khronos.vulkan_samples/cache";
}
}        // namespace details

namespace vkb
{
std::vector<std::string> ExternalSurfacePlatformContext::android_arguments = {};

ExternalSurfacePlatformContext::ExternalSurfacePlatformContext(AAssetManager *asset_manager) :
    PlatformContext{}, asset_manager{asset_manager}
{
	_external_storage_directory = details::get_external_storage_directory(asset_manager);
	_temp_directory             = details::get_external_cache_directory(asset_manager);
	_arguments                  = android_arguments;
}
}        // namespace vkb