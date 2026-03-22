/* Copyright (c) 2018-2022, Arm Limited and Contributors
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

#include "android_window.h"

#include "platform/android/android_platform.h"
#include "platform/android/android_surface_platform.h"

namespace vkb
{
AndroidWindow::AndroidWindow(AndroidPlatform *platform, ANativeWindow *&window, const Window::Properties &properties) :
    Window(properties),
    native_activity_platform{platform},
    handle{window}
{
}

AndroidWindow::AndroidWindow(AndroidSurfacePlatform *platform, ANativeWindow *&window, const Window::Properties &properties) :
    Window(properties),
    surface_platform{platform},
    handle{window}
{
}

VkSurfaceKHR AndroidWindow::create_surface(Instance &instance)
{
	return create_surface(instance.get_handle(), VK_NULL_HANDLE);
}

VkSurfaceKHR AndroidWindow::create_surface(VkInstance instance, VkPhysicalDevice)
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

void AndroidWindow::process_events()
{
	if (native_activity_platform)
	{
		process_android_events(native_activity_platform->get_android_app());
	}
	// For surface_platform, events are handled differently (via Java callbacks)
}

bool AndroidWindow::should_close()
{
	return finish_called ? true : handle == nullptr;
}

void AndroidWindow::close()
{
	if (native_activity_platform)
	{
		ANativeActivity_finish(native_activity_platform->get_activity());
	}
	// For surface_platform, activity finishing is handled in Java
	finish_called = true;
}

float AndroidWindow::get_dpi_factor() const
{
	if (native_activity_platform)
	{
		return AConfiguration_getDensity(native_activity_platform->get_android_app()->config) /
		       static_cast<float>(ACONFIGURATION_DENSITY_MEDIUM);
	}
	// For surface_platform, return a default DPI factor
	// Can be improved by getting density from Java
	return 2.0f;
}
}        // namespace vkb
