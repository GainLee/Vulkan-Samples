/* Copyright (c) 2019, Arm Limited and Contributors
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

#include <core/buffer.h>
#include "common/vk_common.h"
#include "core/instance.h"
#include "platform/application.h"
#include <api_vulkan_sample.h>

class TestGeo : public ApiVulkanSample
{
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;

	std::unique_ptr<vkb::core::Buffer> vertices_buffer;
	std::unique_ptr<vkb::core::Buffer> vertices_index_buffer;

  public:
	TestGeo();

	virtual ~TestGeo();

	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;
	void         build_command_buffers() override;
	void         prepare_pipelines();
	void         draw();
	bool         prepare(vkb::Platform &platform) override;
	void         prepare_vertices_buffers();
	void         setup_descriptor_set_layout();
	virtual void render(float delta_time) override;
	virtual void on_update_ui_overlay(vkb::Drawer &drawer) override;
	virtual bool resize(const uint32_t width, const uint32_t height) override;
};

std::unique_ptr<vkb::Application> create_test_geo();
