/* Copyright (c) 2019-2025, Arm Limited and Contributors
 * Copyright (c) 2025, NVIDIA CORPORATION. All rights reserved.
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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <core/platform/context.hpp>

#include "apps.h"
#include "common/optional.h"
#include "common/utils.h"
#include "common/vk_common.h"
#include "platform/application.h"
#include "platform/plugins/plugin.h"
#include "platform/window.h"
#include "rendering/render_context.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#	undef Success
#endif

namespace vkb
{
enum class ExitCode
{
	Success = 0, /* App executed as expected */
	NoSample,    /* App should show help how to run a sample */
	Help,        /* App should show help */
	Close,       /* App has been requested to close at initialization */
	FatalError   /* App encountered an unexpected error */
};

class Platform
{
  public:
	Platform(const PlatformContext &context);

	virtual ~Platform() = default;

	/**
	 * @brief Initialize the platform
	 * @param plugins plugins available to the platform
	 * @return An exit code representing the outcome of initialization
	 */
	virtual ExitCode initialize(const std::vector<Plugin *> &plugins);

	/**
	 * @brief Handles the main update and render loop
	 * @return An exit code representing the outcome of the loop
	 */
	virtual ExitCode main_loop();

	/**
	 * @brief Handles the update and render of a frame.
	 * Called either from main_loop(), or from a platform-specific
	 * frame-looping mechanism, typically tied to platform screen refeshes.
	 * @return An exit code representing the outcome of the loop
	 */
	ExitCode main_loop_frame();

	/**
	 * @brief Runs the application for one frame
	 */
	void update();

	/**
	 * @brief Terminates the platform and the application
	 * @param code Determines how the platform should exit
	 */
	virtual void terminate(ExitCode code);

	/**
	 * @brief Requests to close the platform at the next available point
	 */
	virtual void close();

	std::string &get_last_error();

	virtual void resize(uint32_t width, uint32_t height);

	virtual void input_event(const InputEvent &input_event);

	Window &get_window();

	Application &get_app() const;

	Application &get_app();

	void set_last_error(const std::string &error);

	template <class T>
	T *get_plugin() const;

	template <class T>
	bool using_plugin() const;

	void set_focus(bool focused);

	void request_application(const apps::AppInfo *app);

	bool app_requested();

	bool start_app();

	void force_simulation_fps(float fps);

	// Force the application to always render even if it is not in focus
	void force_render(bool should_always_render);

	void disable_input_processing();

	void set_window_properties(const Window::OptionalProperties &properties);

	void on_post_draw(RenderContext &context);

	static const uint32_t MIN_WINDOW_WIDTH;
	static const uint32_t MIN_WINDOW_HEIGHT;

  protected:
	std::vector<Plugin *> active_plugins;

	std::unordered_map<Hook, std::vector<Plugin *>> hooks;

	std::unique_ptr<Window> window{nullptr};

	std::unique_ptr<Application> active_app{nullptr};

	virtual std::vector<spdlog::sink_ptr> get_platform_sinks();

	/**
	 * @brief Handles the creation of the window
	 *
	 * @param properties Preferred window configuration
	 */
	virtual void create_window(const Window::Properties &properties) = 0;

	void register_hooks(Plugin *plugin);

	void on_update(float delta_time);
	void on_app_error(const std::string &app_id);
	void on_app_start(const std::string &app_id);
	void on_app_close(const std::string &app_id);
	void on_platform_close();
	void on_update_ui_overlay(vkb::Drawer &drawer);

	Window::Properties window_properties;              /* Source of truth for window state */
	bool               fixed_simulation_fps{false};    /* Delta time should be fixed with a fabricated value */
	bool               always_render{false};           /* App should always render even if not in focus */
	float              simulation_frame_time = 0.016f; /* A fabricated delta time */
	bool               process_input_events{true};     /* App should continue processing input events */
	bool               focused{true};                  /* App is currently in focus at an operating system level */
	bool               close_requested{false};         /* Close requested */

  protected:
	std::vector<Plugin *> plugins;

  private:
	Timer timer;

	const apps::AppInfo *requested_app{nullptr};

	std::vector<std::string> arguments;

	std::string last_error;

	std::map<std::string, Plugin *> command_map;
	std::map<std::string, Plugin *> option_map;
};

template <class T>
bool Platform::using_plugin() const
{
	return !plugins::with_tags<T>(active_plugins).empty();
}

template <class T>
T *Platform::get_plugin() const
{
	assert(using_plugin<T>() && "Plugin is not enabled but was requested");
	const auto plugins = plugins::with_tags<T>(active_plugins);
	return dynamic_cast<T *>(plugins[0]);
}
}        // namespace vkb
