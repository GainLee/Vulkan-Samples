/* Copyright (c) 2019-2020, Arm Limited and Contributors
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

#include <common/vk_initializers.h>
#include "test_geo.h"

#include "common/logging.h"
#include "common/vk_common.h"
#include "glsl_compiler.h"
#include "platform/filesystem.h"
#include "platform/platform.h"

#define PI 3.1415926
#define POINTS 12


TestGeo::TestGeo()
{
	title = "Instanced mesh rendering";
}

TestGeo::~TestGeo()
{
	if (device)
	{
		vkDestroyPipeline(get_device().get_handle(), pipeline, nullptr);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
	}
}

void TestGeo::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	auto &requested_features = gpu.get_mutable_requested_features();

	if (gpu.get_features().geometryShader)
	{
		requested_features.geometryShader = VK_TRUE;
	}
};

void TestGeo::build_command_buffers()
{
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VkClearValue clear_values[2];
	clear_values[0].color        = {{0.0f, 0.0f, 0.033f, 0.0f}};
	clear_values[1].depthStencil = {0.0f, 0};

	VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
	render_pass_begin_info.renderPass               = render_pass;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount          = 2;
	render_pass_begin_info.pClearValues             = clear_values;

	for (int32_t i = 0; i < draw_cmd_buffers.size(); ++i)
	{
		// Set target frame buffer
		render_pass_begin_info.framebuffer = framebuffers[i];

		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &command_buffer_begin_info));

		vkCmdBeginRenderPass(draw_cmd_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkb::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[1] = {0};

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, vertices_buffer->get(), offsets);
		vkCmdBindIndexBuffer(draw_cmd_buffers[i], vertices_index_buffer->get_handle(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(draw_cmd_buffers[i], (POINTS -1) * 6, 1, 0, 0, 0);

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void TestGeo::prepare_pipelines()
{
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
			vkb::initializers::pipeline_input_assembly_state_create_info(
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					0,
					VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
			vkb::initializers::pipeline_rasterization_state_create_info(
					VK_POLYGON_MODE_FILL,
					VK_CULL_MODE_BACK_BIT,
					VK_FRONT_FACE_COUNTER_CLOCKWISE,
					0);

	VkPipelineColorBlendAttachmentState blend_attachment_state =
			vkb::initializers::pipeline_color_blend_attachment_state(
					0xf,
					VK_FALSE);

	VkPipelineColorBlendStateCreateInfo color_blend_state =
			vkb::initializers::pipeline_color_blend_state_create_info(
					1,
					&blend_attachment_state);

	// Note: Using Reversed depth-buffer for increased precision, so Greater depth values are kept
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
			vkb::initializers::pipeline_depth_stencil_state_create_info(
					VK_FALSE,
					VK_FALSE,
					VK_COMPARE_OP_GREATER);

	VkPipelineViewportStateCreateInfo viewport_state =
			vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
			vkb::initializers::pipeline_multisample_state_create_info(
					VK_SAMPLE_COUNT_1_BIT,
					0);

	std::vector<VkDynamicState> dynamic_state_enables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamic_state =
			vkb::initializers::pipeline_dynamic_state_create_info(
					dynamic_state_enables.data(),
					vkb::to_u32(dynamic_state_enables.size()),
					0);

	// Load shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;
	shader_stages[0] = load_shader("test_geo/test_geo.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("test_geo/test_geo.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipeline_create_info =
			vkb::initializers::pipeline_create_info(
					pipeline_layout,
					render_pass,
					0);

	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState    = &color_blend_state;
	pipeline_create_info.pMultisampleState   = &multisample_state;
	pipeline_create_info.pViewportState      = &viewport_state;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_state;
	pipeline_create_info.pDynamicState       = &dynamic_state;
	pipeline_create_info.stageCount          = vkb::to_u32(shader_stages.size());
	pipeline_create_info.pStages             = shader_stages.data();

	// This example uses two different input states, one for the instanced part and one for non-instanced rendering
	VkPipelineVertexInputStateCreateInfo           input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	std::vector<VkVertexInputBindingDescription>   binding_descriptions;
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

	// Vertex input bindings
	// The instancing pipeline uses a vertex input state with two bindings
	binding_descriptions = {
			// Binding point 0: Mesh vertex layout description at per-vertex rate
			vkb::initializers::vertex_input_binding_description(0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX)};

	// Vertex attribute bindings
	attribute_descriptions = {
			// Per-vertex attributees
			// These are advanced for each vertex fetched by the vertex shader
			vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),                        // Location 0: Position
	};
	input_state.pVertexBindingDescriptions   = binding_descriptions.data();
	input_state.pVertexAttributeDescriptions = attribute_descriptions.data();
	input_state.vertexBindingDescriptionCount   = static_cast<uint32_t>(binding_descriptions.size());
	input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());

	pipeline_create_info.pVertexInputState = &input_state;

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline));
}


void TestGeo::prepare_vertices_buffers()
{
	vertices_buffer = std::make_unique<vkb::core::Buffer>(get_device(),
																sizeof(glm::vec3) * POINTS * 2,
																VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
																VMA_MEMORY_USAGE_CPU_TO_GPU);

	vertices_index_buffer = std::make_unique<vkb::core::Buffer>(get_device(),
														  sizeof(uint32_t) * (POINTS -1) * 6,
														  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
														  VMA_MEMORY_USAGE_CPU_TO_GPU);

	std::array<glm::vec3, POINTS * 2> vertices;
	std::array<uint32_t, (POINTS -1) * 6> vertices_index;
	for (int i = 0; i < POINTS; ++i) {
		vertices[i*2] = glm::vec3(-0.4*cos(90*i/(POINTS-1)*PI/180), 0.4*sin(90*i/(POINTS-1)*PI/180), 0.0);
		vertices[i*2+1] = glm::vec3(-0.6*cos(90*i/(POINTS-1)*PI/180), 0.6*sin(90*i/(POINTS-1)*PI/180), 0.0);

		LOGI("vertices[{}]:{}   {}", i*2, -0.4*cos(90*i/(POINTS-1)*PI/180), 0.4*sin(90*i/(POINTS-1)*PI/180));
		LOGI("vertices[{}]:{}   {}", i*2+1, -0.6*cos(90*i/(POINTS-1)*PI/180), 0.6*sin(90*i/(POINTS-1)*PI/180));

		if(i>0) {
			vertices_index[(i-1)*6] = (i-1)*2;
			vertices_index[(i-1)*6+1] = (i-1)*2 + 1;
			vertices_index[(i-1)*6+2] = i*2;
			vertices_index[(i-1)*6+3] = i*2;
			vertices_index[(i-1)*6+4] = (i-1)*2 + 1;
			vertices_index[(i-1)*6+5] = i*2 + 1;

			LOGI("vertices_index[{}]:{}", (i-1)*6, (i-1)*2);
			LOGI("vertices_index[{}]:{}", (i-1)*6+1, (i-1)*2+1);
			LOGI("vertices_index[{}]:{}", (i-1)*6+2, i*2);
			LOGI("vertices_index[{}]:{}", (i-1)*6+3, i*2);
			LOGI("vertices_index[{}]:{}", (i-1)*6+4, (i-1)*2+1);
			LOGI("vertices_index[{}]:{}", (i-1)*6+5, i*2+1);
		}
	}

	vertices_buffer->update(vertices.data(), sizeof(glm::vec3) * POINTS * 2, 0);
	vertices_index_buffer->update(vertices_index.data(), sizeof(uint32_t) * (POINTS -1) * 6, 0);
}

void TestGeo::setup_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings =
			{
			};

	VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info =
			vkb::initializers::descriptor_set_layout_create_info(
					set_layout_bindings.data(),
					vkb::to_u32(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout_create_info, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info =
			vkb::initializers::pipeline_layout_create_info(
					&descriptor_set_layout,
					1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

void TestGeo::draw()
{
	ApiVulkanSample::prepare_frame();

	// Command buffer to be sumitted to the queue
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	// Submit to queue
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

bool TestGeo::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}

	// Note: Using Revsered depth-buffer for increased precision, so Znear and Zfar are flipped
	camera.type = vkb::CameraType::LookAt;
	camera.set_perspective(60.0f, static_cast<float>(width) / static_cast<float>(height), 256.0f, 0.1f);
	camera.set_rotation(glm::vec3(-17.2f, -4.7f, 0.0f));
	camera.set_translation(glm::vec3(5.5f, -1.85f, -18.5f));

	setup_descriptor_set_layout();
	prepare_vertices_buffers();
	prepare_pipelines();
	build_command_buffers();
	prepared = true;
	return true;
}

void TestGeo::render(float delta_time)
{
	if (!prepared)
	{
		return;
	}
	draw();
	if (!paused || camera.updated)
	{
	}
}

void TestGeo::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("Statistics"))
	{
		drawer.text("Instances");
	}
}

bool TestGeo::resize(const uint32_t width, const uint32_t height)
{
	ApiVulkanSample::resize(width, height);
	build_command_buffers();
	return true;
}

std::unique_ptr<vkb::Application> create_test_geo()
{
	return std::make_unique<TestGeo>();
}