/* Copyright (c) 2018-2023, Arm Limited and Contributors
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

#include "common/vk_common.h"
#include "platform/window.h"

namespace vkb
{
class ExternalSurfaceAndroidPlatform;

/**
 * @brief Android window abstraction supporting both native_app_glue and external surface modes
 */
class ExternalSurfaceAndroidWindow : public Window
{
  public:
  /**
	 * @brief Constructor
	 * @param platform The platform this window is created for
	 * @param window A reference to the location of the Android native window
	 * @param properties Window configuration
	 */
	ExternalSurfaceAndroidWindow(ExternalSurfaceAndroidPlatform *platform, ANativeWindow *window, const Window::Properties &properties);

	virtual ~ExternalSurfaceAndroidWindow() = default;

	/**
	 * @brief Creates a Vulkan surface to the native window
	 *        If headless, this will return VK_NULL_HANDLE
	 */
	virtual VkSurfaceKHR create_surface(Instance &instance) override;

	/**
	 * @brief Creates a Vulkan surface to the native window
	 *        If headless, this will return nullptr
	 */
	virtual VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice physical_device) override;

	virtual void process_events() override;

	virtual bool should_close() override;

	virtual void close() override;

	virtual float get_dpi_factor() const override;

	std::vector<const char *> get_required_surface_extensions() const override;

  private:
	ANativeWindow *handle;
	ExternalSurfaceAndroidPlatform *platform;
	bool finish_called{false};
};
}        // namespace vkb
