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

#include "gain_subpasses.h"

#include "common/vk_common.h"
#include "gltf_loader.h"
#include "gui.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "rendering/subpasses/forward_subpass.h"
#include "stats/stats.h"


Gain_Subpasses::Gain_Subpasses()
{
	zoom     = -2.5f;
	rotation = {0.0f, 15.0f, 0.0f};
	title    = "Gain_Subpasses";
}

Gain_Subpasses::~Gain_Subpasses()
{
	if (device)
	{
		// Clean up used Vulkan resources
		// Note : Inherited destructor cleans up resources stored in base class

		vkDestroyPipeline(get_device().get_handle(), stencil_pipeline, nullptr);

		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
	}
}

// Enable physical device features required for this example
void Gain_Subpasses::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	// Enable anisotropic filtering if supported
	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = VK_TRUE;
	}
}

void Gain_Subpasses::build_command_buffers()
{
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VkClearValue clear_values[2];
	clear_values[0].color        =  {{0.1f, 0.1f, 0.2f, 1.0f}};
	clear_values[1].depthStencil = {0.0f, 0};

	VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
	render_pass_begin_info.renderPass               = render_pass;
	render_pass_begin_info.renderArea.offset.x      = 0;
	render_pass_begin_info.renderArea.offset.y      = 0;
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

		VkViewport viewport = vkb::initializers::viewport((float) width, (float) height, 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, stencil_pipeline);

		vkCmdDraw(draw_cmd_buffers[i], 3, 1, 0, 0);

		/* Advance to the next subpass */
		vkCmdNextSubpass(draw_cmd_buffers[i], VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, color_pipeline);

		vkCmdDraw(draw_cmd_buffers[i], 6, 1, 0, 0);

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void Gain_Subpasses::draw()
{
	ApiVulkanSample::prepare_frame();

	// Command buffer to be sumitted to the queue
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	// Submit to queue
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

void Gain_Subpasses::prepare_pipelines()
{
	VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &layout_info, nullptr, &pipeline_layout));

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
			vkb::initializers::pipeline_input_assembly_state_create_info(
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					0,
					VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
			vkb::initializers::pipeline_rasterization_state_create_info(
					VK_POLYGON_MODE_FILL,
					VK_CULL_MODE_NONE,
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
					VK_TRUE,
					VK_TRUE,
					VK_COMPARE_OP_GREATER);
	depth_stencil_state.stencilTestEnable = VK_TRUE;
	depth_stencil_state.back.failOp = VK_STENCIL_OP_REPLACE;
	depth_stencil_state.back.depthFailOp = VK_STENCIL_OP_REPLACE;
	depth_stencil_state.back.passOp = VK_STENCIL_OP_REPLACE;
	depth_stencil_state.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depth_stencil_state.back.compareMask = 0xff;
	depth_stencil_state.back.writeMask = 0xff;
	depth_stencil_state.back.reference = 1;
	depth_stencil_state.front = depth_stencil_state.back;

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
					static_cast<uint32_t>(dynamic_state_enables.size()),
					0);

	// Load shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;

	shader_stages[0] = load_shader("gain_subpasses/subpasses.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("gain_subpasses/subpasses.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();

	VkGraphicsPipelineCreateInfo pipeline_create_info =
			vkb::initializers::pipeline_create_info(
					pipeline_layout,
					render_pass,
					0);

	pipeline_create_info.pVertexInputState   = &vertex_input_state;
	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState    = &color_blend_state;
	pipeline_create_info.pMultisampleState   = &multisample_state;
	pipeline_create_info.pViewportState      = &viewport_state;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_state;
	pipeline_create_info.pDynamicState       = &dynamic_state;
	pipeline_create_info.stageCount          = static_cast<uint32_t>(shader_stages.size());
	pipeline_create_info.pStages             = shader_stages.data();

	pipeline_create_info.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &stencil_pipeline));

	shader_stages[0] = load_shader("gain_subpasses/subpasses.full.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("gain_subpasses/subpasses.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipeline_create_info.subpass = 1;

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &color_pipeline));

}

void Gain_Subpasses::setup_render_pass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format         = render_context->get_format();
	attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format         = depth_format;
	attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference color_reference = {};
	color_reference.attachment            = 0;
	color_reference.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_reference = {};
	depth_reference.attachment            = 1;
	depth_reference.layout                = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass_description    = {};
	subpass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description.colorAttachmentCount    = 0;
	subpass_description.pColorAttachments       = nullptr;
	subpass_description.pDepthStencilAttachment = &depth_reference;
	subpass_description.inputAttachmentCount    = 0;
	subpass_description.pInputAttachments       = nullptr;
	subpass_description.preserveAttachmentCount = 0;
	subpass_description.pPreserveAttachments    = nullptr;
	subpass_description.pResolveAttachments     = nullptr;

	std::vector<VkSubpassDescription> subpass_descriptions;

	/* first a depthstencil-only subpass */
	subpass_descriptions.push_back(subpass_description);

	subpass_description.colorAttachmentCount    = 1;
	subpass_description.pColorAttachments       = &color_reference;

	/* then depthstencil and color */
	subpass_descriptions.push_back(subpass_description);

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass      = 0;
	dependencies[0].dstSubpass      = 1;
	dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass      = 0;
	dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo render_pass_create_info = {};
	render_pass_create_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount        = static_cast<uint32_t>(attachments.size());
	render_pass_create_info.pAttachments           = attachments.data();
	render_pass_create_info.subpassCount           = subpass_descriptions.size();
	render_pass_create_info.pSubpasses             = subpass_descriptions.data();
	render_pass_create_info.dependencyCount        = static_cast<uint32_t>(dependencies.size());
	render_pass_create_info.pDependencies          = dependencies.data();

	VK_CHECK(vkCreateRenderPass(device->get_handle(), &render_pass_create_info, nullptr, &render_pass));
}

bool Gain_Subpasses::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}
	prepare_pipelines();
	build_command_buffers();
	prepared = true;
	return true;
}

void Gain_Subpasses::render(float delta_time)
{
	if (!prepared)
		return;
	draw();
}

void Gain_Subpasses::view_changed()
{
}

void Gain_Subpasses::on_update_ui_overlay(vkb::Drawer &drawer)
{
}

std::unique_ptr<vkb::VulkanSample> create_gain_subpasses()
{
	return std::make_unique<Gain_Subpasses>();
}
