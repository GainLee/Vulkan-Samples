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

#include "external_surface_android_window.h"

#include "platform/android/external_surface_android_platform.h"

namespace vkb
{
ExternalSurfaceAndroidWindow::ExternalSurfaceAndroidWindow(ExternalSurfaceAndroidPlatform *platform, ANativeWindow *window, const Window::Properties &properties) :
    Window(properties),
    handle{window},
    platform{platform}
{
}

VkSurfaceKHR ExternalSurfaceAndroidWindow::create_surface(Instance &instance)
{
	return create_surface(instance.get_handle(), VK_NULL_HANDLE);
}

VkSurfaceKHR ExternalSurfaceAndroidWindow::create_surface(VkInstance instance, VkPhysicalDevice)
{
	if (instance == VK_NULL_HANDLE || !handle || properties.mode == Mode::Headless)
	{
		return VK_NULL_HANDLE;
	}

	VkSurfaceKHR surface{};

	VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};

	info.window = handle;

	VK_CHECK(vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface));

	return surface;
}

void ExternalSurfaceAndroidWindow::process_events()
{
	// In external surface mode, we don't need to process events like native_app_glue
	platform->process_android_input_events();
}

bool ExternalSurfaceAndroidWindow::should_close()
{
	// Check if finish was called OR if the surface handle is null/invalid
	if (finish_called)
	{
		return true;
	}
	
	// Check if the surface is still valid
	if (!handle)
	{
		LOGI("ExternalSurfaceAndroidWindow: Surface handle is null, should close");
		return true;
	}
	
	return false;
}

void ExternalSurfaceAndroidWindow::close()
{
	LOGI("ExternalSurfaceAndroidWindow::close() called");
	finish_called = true;
	
	// Clear the surface handle to prevent further use
	handle = nullptr;
}

float ExternalSurfaceAndroidWindow::get_dpi_factor() const
{
    // TODO: Use AAssetManager to get the density
	return 1.0f;
}

std::vector<const char *> ExternalSurfaceAndroidWindow::get_required_surface_extensions() const
{
	return {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
}
}        // namespace vkb
