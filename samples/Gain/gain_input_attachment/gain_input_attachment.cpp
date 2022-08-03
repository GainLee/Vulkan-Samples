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

#include "gain_input_attachment.h"

#include "common/vk_common.h"
#include "gltf_loader.h"
#include "gui.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "rendering/subpasses/forward_subpass.h"
#include "stats/stats.h"

Gain_InputAttachment::Gain_InputAttachment()
{
	zoom     = -2.5f;
	rotation = {0.0f, 15.0f, 0.0f};
	title    = "Gain_Subpasses";
}

Gain_InputAttachment::~Gain_InputAttachment()
{
	if (device)
	{
		// Clean up used Vulkan resources
		// Note : Inherited destructor cleans up resources stored in base class

		vkDestroyPipeline(get_device().get_handle(), pipeline, nullptr);

		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
	}
}

// Enable physical device features required for this example
void Gain_InputAttachment::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	// Enable anisotropic filtering if supported
	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = VK_TRUE;
	}
}

void Gain_InputAttachment::build_command_buffers()
{
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VkClearValue clear_values[2];
	clear_values[0].color        = {{0.1f, 0.1f, 0.2f, 1.0f}};
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

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0,
								nullptr);

		vkCmdDraw(draw_cmd_buffers[i], 3, 1, 0, 0);

		//		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void Gain_InputAttachment::draw()
{
	ApiVulkanSample::prepare_frame();

	// Command buffer to be sumitted to the queue
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	// Submit to queue
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

void Gain_InputAttachment::prepare_input_attachment()
{
	input_texture = load_texture("textures/vulkan_logo_full.ktx");
}

void Gain_InputAttachment::prepare_pipelines()
{
	VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts    = &descriptor_set_layout;
	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &layout_info, nullptr, &pipeline_layout));

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
			vkb::initializers::pipeline_input_assembly_state_create_info(
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					0,
					VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
			vkb::initializers::pipeline_rasterization_state_create_info(
					VK_POLYGON_MODE_FILL,
					VK_CULL_MODE_BACK_BIT,
					VK_FRONT_FACE_CLOCKWISE,
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
					static_cast<uint32_t>(dynamic_state_enables.size()),
					0);

	// Load shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;

	shader_stages[0] = load_shader("gain_input_attachment/input_attachment.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("gain_input_attachment/input_attachment.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

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

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline));
}

void Gain_InputAttachment::setup_render_pass()
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
	// Input attachment
	attachments[1].format         = VK_FORMAT_R8G8B8A8_SRGB;
	attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference color_reference = {};
	color_reference.attachment            = 0;
	color_reference.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference input_reference = {};
	input_reference.attachment            = 1;
	input_reference.layout                = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkSubpassDescription subpass_description    = {};
	subpass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description.colorAttachmentCount    = 1;
	subpass_description.pColorAttachments       = &color_reference;
	subpass_description.pDepthStencilAttachment = nullptr;
	subpass_description.inputAttachmentCount    = 1;
	subpass_description.pInputAttachments       = &input_reference;
	subpass_description.preserveAttachmentCount = 0;
	subpass_description.pPreserveAttachments    = nullptr;
	subpass_description.pResolveAttachments     = nullptr;

	std::vector<VkSubpassDescription> subpass_descriptions;
	subpass_descriptions.push_back(subpass_description);

	// Subpass dependencies for layout transitions
	VkSubpassDependency dependency = {0};
	dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass          = 0;
	dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	// Since we changed the image layout, we need to make the memory visible to
	// color attachment to modify.
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_create_info = {};
	render_pass_create_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount        = static_cast<uint32_t>(attachments.size());
	render_pass_create_info.pAttachments           = attachments.data();
	render_pass_create_info.subpassCount           = subpass_descriptions.size();
	render_pass_create_info.pSubpasses             = subpass_descriptions.data();
	render_pass_create_info.dependencyCount        = 1;
	render_pass_create_info.pDependencies          = &dependency;

	VK_CHECK(vkCreateRenderPass(device->get_handle(), &render_pass_create_info, nullptr, &render_pass));
}

void Gain_InputAttachment::setup_framebuffer()
{
    prepare_input_attachment();

	VkImageView attachments[2];

	attachments[1] = input_texture.image->get_vk_image_view().get_handle();

	VkFramebufferCreateInfo framebuffer_create_info = {};
	framebuffer_create_info.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_create_info.pNext                   = NULL;
	framebuffer_create_info.renderPass              = render_pass;
	framebuffer_create_info.attachmentCount         = 2;
	framebuffer_create_info.pAttachments            = attachments;
    framebuffer_create_info.width                   = get_render_context().get_surface_extent().width;
    framebuffer_create_info.height                  = get_render_context().get_surface_extent().height;
	framebuffer_create_info.layers                  = 1;

	// Delete existing frame buffers
	if (framebuffers.size() > 0)
	{
		for (uint32_t i = 0; i < framebuffers.size(); i++)
		{
			if (framebuffers[i] != VK_NULL_HANDLE)
			{
				vkDestroyFramebuffer(device->get_handle(), framebuffers[i], nullptr);
			}
		}
	}

	// Create frame buffers for every swap chain image
	framebuffers.resize(render_context->get_render_frames().size());
	for (uint32_t i = 0; i < framebuffers.size(); i++)
	{
		attachments[0] = swapchain_buffers[i].view;
		VK_CHECK(vkCreateFramebuffer(device->get_handle(), &framebuffer_create_info, nullptr, &framebuffers[i]));
	}
}

void Gain_InputAttachment::setup_descriptor_pool()
{
	// Example uses one ubo and one image sampler
	std::vector<VkDescriptorPoolSize> pool_sizes =
			{
					vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1)};

	VkDescriptorPoolCreateInfo descriptor_pool_create_info =
			vkb::initializers::descriptor_pool_create_info(
					static_cast<uint32_t>(pool_sizes.size()),
					pool_sizes.data(),
					1);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, nullptr, &descriptor_pool));
}

void Gain_InputAttachment::setup_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings =
			{
					vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 0)};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
			vkb::initializers::descriptor_set_layout_create_info(
					set_layout_bindings.data(),
					static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info =
			vkb::initializers::pipeline_layout_create_info(
					&descriptor_set_layout,
					1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

void Gain_InputAttachment::setup_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
			vkb::initializers::descriptor_set_allocate_info(
					descriptor_pool,
					&descriptor_set_layout,
					1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorImageInfo  image_descriptor  = create_descriptor(input_texture, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

	std::vector<VkWriteDescriptorSet> write_descriptor_sets = {
			vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0, &image_descriptor),
	};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, NULL);
}

bool Gain_InputAttachment::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}
	setup_descriptor_set_layout();
	prepare_pipelines();
	setup_descriptor_pool();
	setup_descriptor_set();
	build_command_buffers();
	prepared = true;
	return true;
}

void Gain_InputAttachment::render(float delta_time)
{
	if (!prepared)
		return;
	draw();
}

void Gain_InputAttachment::view_changed()
{
}

void Gain_InputAttachment::on_update_ui_overlay(vkb::Drawer &drawer)
{
}

std::unique_ptr<vkb::VulkanSample> create_gain_input_attachment()
{
	return std::make_unique<Gain_InputAttachment>();
}
