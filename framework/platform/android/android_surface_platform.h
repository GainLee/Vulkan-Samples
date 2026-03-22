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

#pragma once

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <mutex>
#include <condition_variable>

#include "platform/platform.h"

namespace vkb
{
/**
 * @brief Android platform that receives Surface from Java instead of using NativeActivity
 *
 * This platform is designed to work with a regular Android Activity that passes
 * a Surface via JNI. This is useful for integrating Vulkan rendering into
 * existing Android apps without using NativeActivity.
 */
class AndroidSurfacePlatform : public Platform
{
  public:
	AndroidSurfacePlatform();

	virtual ~AndroidSurfacePlatform() = default;

	/**
	 * @brief Initialize the platform
	 * @param plugins plugins available to the platform
	 * @return An exit code representing the outcome of initialization
	 */
	virtual ExitCode initialize(const std::vector<Plugin *> &plugins) override;

	/**
	 * @brief Terminates the platform and the application
	 * @param code Determines how the platform should exit
	 */
	virtual void terminate(ExitCode code) override;

	/**
	 * @brief Get the Vulkan surface extension name for Android
	 * @return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
	 */
	virtual const char *get_surface_extension() override;

	/**
	 * @brief Set the Android Surface from Java
	 * @param env JNI environment
	 * @param surface Java Surface object
	 */
	void set_surface(JNIEnv *env, jobject surface);

	/**
	 * @brief Called when surface is created
	 * @param width Surface width
	 * @param height Surface height
	 */
	void on_surface_created(int width, int height);

	/**
	 * @brief Called when surface size changes
	 * @param width New width
	 * @param height New height
	 */
	void on_surface_changed(int width, int height);

	/**
	 * @brief Called when surface is destroyed
	 */
	void on_surface_destroyed();

	/**
	 * @brief Run the application with given arguments
	 * @param args Command line arguments
	 * @param plugins Plugins to initialize (defaults to empty for backward compatibility)
	 */
	void run(const std::vector<std::string> &args, const std::vector<Plugin *> &plugins = {});

	/**
	 * @brief Get the native window
	 * @return ANativeWindow pointer
	 */
	ANativeWindow *get_native_window() const;

	/**
	 * @brief Check if surface is ready
	 * @return true if surface is available
	 */
	bool is_surface_ready() const;

	/**
	 * @brief Wait for surface to be ready
	 */
	void wait_for_surface();

  protected:
	/**
	 * @brief Create the window
	 * @param properties Window properties
	 */
	virtual void create_window(const Window::Properties &properties) override;

	/**
	 * @brief Get platform-specific log sinks
	 * @return Vector of spdlog sinks
	 */
	virtual std::vector<spdlog::sink_ptr> get_platform_sinks() override;

  private:
	ANativeWindow *native_window{nullptr};
	mutable std::mutex surface_mutex;
	std::condition_variable surface_cv;
	bool surface_ready{false};
	bool running{false};
	std::string log_output;

	JNIEnv *jni_env{nullptr};
	jobject java_surface{nullptr};
	JavaVM *java_vm{nullptr};
};

/**
 * @brief Global instance of the platform for JNI callbacks
 */
extern AndroidSurfacePlatform *g_surface_platform;

} // namespace vkb
