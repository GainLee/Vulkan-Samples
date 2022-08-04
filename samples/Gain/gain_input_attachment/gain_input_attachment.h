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

#include "rendering/render_pipeline.h"
#include "scene_graph/components/camera.h"
#include <api_vulkan_sample.h>
#include <ktx.h>

class Gain_InputAttachment : public ApiVulkanSample
{
  public:
	VkPipeline pipeline;

	struct Texture
	{
		VkSampler      sampler;
		VkImage        image;
		VkImageLayout  image_layout;
		VkDeviceMemory device_memory;
		VkImageView    view;
		uint32_t       width, height;
		uint32_t       mip_levels;
	} input_Texture;
	VkDescriptorSetLayout descriptor_set_layout;        // Particle system rendering shader binding layout
	VkDescriptorSet       descriptor_set;               // Particle system rendering shader bindings
	VkPipelineLayout      pipeline_layout;

	Gain_InputAttachment();
	~Gain_InputAttachment();
	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;
	void         setup_descriptor_pool();
	void         setup_descriptor_set_layout();
	void         setup_descriptor_set();
	void         build_command_buffers() override;
	void         draw();
	void         prepare_input_attachment();
	void         prepare_pipelines();
	bool         prepare(vkb::Platform &platform) override;
	void         destroy_texture(Texture texture);
	virtual void setup_render_pass() override;
	virtual void setup_framebuffer() override;
	virtual void render(float delta_time) override;
	virtual void view_changed() override;
	virtual void on_update_ui_overlay(vkb::Drawer &drawer) override;
};

std::unique_ptr<vkb::VulkanSample> create_gain_input_attachment();
