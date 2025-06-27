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

#pragma once

#include "platform/platform.h"
#include <android/asset_manager.h>

namespace vkb
{
class ExternalSurfaceAndroidPlatform : public Platform
{
  public:
	ExternalSurfaceAndroidPlatform(const PlatformContext &context);

	virtual ~ExternalSurfaceAndroidPlatform() = default;

	virtual ExitCode initialize(const std::vector<Plugin *> &plugins) override;

	virtual void terminate(ExitCode code) override;

	/**
	 * @brief Sends a notification in the task bar
	 * @param message The message to display
	 */
	void send_notification(const std::string &message);

	/**
	 * @brief Sends an error notification in the task bar
	 * @param message The message to display
	 */
	void send_error_notification(const std::string &message);

	void set_external_surface(ANativeWindow *surface);

	ANativeWindow *get_external_surface();

	AAssetManager *get_asset_manager();

	void set_surface_ready();

	void process_android_input_events(void);

	/**
	 * @brief Set the global JNI platform instance for external surface mode
	 * @param platform Pointer to the platform instance
	 */
	static void set_jni_platform_instance(ExternalSurfaceAndroidPlatform *platform);

	/**
	 * @brief Get the global JNI platform instance for external surface mode
	 * @return Pointer to the platform instance, or nullptr if not set
	 */
	static ExternalSurfaceAndroidPlatform *get_jni_platform_instance();

  private:
	virtual void create_window(const Window::Properties &properties) override;

  private:
	AAssetManager *asset_manager{nullptr};
	ANativeWindow *external_surface{nullptr};

	std::string log_output;

	virtual std::vector<spdlog::sink_ptr> get_platform_sinks() override;

	bool surface_ready{false};

	/**
	 * @brief Global platform instance for JNI access
	 */
	static ExternalSurfaceAndroidPlatform *jni_platform_instance;
};

/**
 * @brief Process android lifecycle events
 *
 * @param app Android app context
 * @return true Events processed
 * @return false Program should close
 */
inline bool process_android_events(AAssetManager *asset_manager)
{
	// TODO: Implement
	return true;
}
}        // namespace vkb
