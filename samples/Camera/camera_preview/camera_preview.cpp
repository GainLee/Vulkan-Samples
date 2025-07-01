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

#include "camera_preview.h"

#include "common/vk_common.h"
#include "filesystem/legacy.h"
#include "platform/platform.h"
#include "common/vk_initializers.h"
#include "ktx.h"

#ifdef ANDROID
// Forward declaration to avoid include issues
extern "C" bool get_jni_image_data(std::vector<uint8_t>& data, uint32_t& width, uint32_t& height);
extern "C" bool check_texture_update_needed();
#endif

CameraPreview::CameraPreview()
{
	title = "Camera Preview - Image Display";
}

CameraPreview::~CameraPreview()
{
	if (has_device())
	{
		// Clean up used Vulkan resources
		vkDestroyPipeline(get_device().get_handle(), pipeline, nullptr);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layout, nullptr);
	}

	destroy_texture(texture);

	vertex_buffer.reset();
	index_buffer.reset();
	uniform_buffer_vs.reset();
}

// Enable physical device features required for this example
void CameraPreview::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	// Enable anisotropic filtering if supported
	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = VK_TRUE;
	}
}

void CameraPreview::load_texture()
{
	LOGI("Loading texture");
	std::vector<uint8_t> image_data;
	uint32_t image_width = 0;
	uint32_t image_height = 0;
	bool has_jni_image = false;
	
#ifdef ANDROID
	// Try to get image data from JNI first
	has_jni_image = get_jni_image_data(image_data, image_width, image_height);
	if (has_jni_image) {
		LOGI("Using image data from Java layer: {}x{}, {} bytes", image_width, image_height, image_data.size());
		
		// Set initial image aspect ratio
		current_image_aspect_ratio = static_cast<float>(image_width) / static_cast<float>(image_height);
		LOGI("Initial image aspect ratio: {} ({}x{})", current_image_aspect_ratio, image_width, image_height);
		
		create_texture_from_raw_data(image_data, image_width, image_height);
		return;
	} else {
		LOGI("No image data from Java layer");
	}
#endif
}

// Free all Vulkan resources used by a texture object
void CameraPreview::destroy_texture(Texture& texture)
{
	if (texture.view != VK_NULL_HANDLE) {
		vkDestroyImageView(get_device().get_handle(), texture.view, nullptr);
		texture.view = VK_NULL_HANDLE;
	}
	if (texture.image != VK_NULL_HANDLE) {
		vkDestroyImage(get_device().get_handle(), texture.image, nullptr);
		texture.image = VK_NULL_HANDLE;
	}
	if (texture.sampler != VK_NULL_HANDLE) {
		vkDestroySampler(get_device().get_handle(), texture.sampler, nullptr);
		texture.sampler = VK_NULL_HANDLE;
	}
	if (texture.device_memory != VK_NULL_HANDLE) {
		vkFreeMemory(get_device().get_handle(), texture.device_memory, nullptr);
		texture.device_memory = VK_NULL_HANDLE;
	}
}

void CameraPreview::build_command_buffers()
{
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VkClearValue clear_values[2];
	clear_values[0].color        = default_clear_color;
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
		render_pass_begin_info.framebuffer = framebuffers[i];

		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &command_buffer_begin_info));

		vkCmdBeginRenderPass(draw_cmd_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkb::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkDeviceSize offsets[1] = {0};
		vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, vertex_buffer->get(), offsets);
		vkCmdBindIndexBuffer(draw_cmd_buffers[i], index_buffer->get_handle(), 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(draw_cmd_buffers[i], index_count, 1, 0, 0, 0);

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void CameraPreview::draw()
{
	ApiVulkanSample::prepare_frame();

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

void CameraPreview::generate_quad()
{
	// Setup vertices for a single uv-mapped quad made from two triangles
	std::vector<VertexStructure> vertices =
	    {
	        {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},// Top-right
	        {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},// Top-left
	        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},// Bottom-left
	        {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}};// Bottom-right

	// Setup indices
	std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
	index_count                   = static_cast<uint32_t>(indices.size());

	auto vertex_buffer_size = vkb::to_u32(vertices.size() * sizeof(VertexStructure));
	auto index_buffer_size  = vkb::to_u32(indices.size() * sizeof(uint32_t));

	// Create buffers
	vertex_buffer = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                     vertex_buffer_size,
	                                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                                     VMA_MEMORY_USAGE_CPU_TO_GPU);
	vertex_buffer->update(vertices.data(), vertex_buffer_size);

	index_buffer = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                    index_buffer_size,
	                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

	index_buffer->update(indices.data(), index_buffer_size);
}

void CameraPreview::setup_descriptor_pool()
{
	std::vector<VkDescriptorPoolSize> pool_sizes =
	    {
	        vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	        vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};

	VkDescriptorPoolCreateInfo descriptor_pool_create_info =
	    vkb::initializers::descriptor_pool_create_info(
	        static_cast<uint32_t>(pool_sizes.size()),
	        pool_sizes.data(),
	        2);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, nullptr, &descriptor_pool));
}

void CameraPreview::setup_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings =
	    {
	        vkb::initializers::descriptor_set_layout_binding(
	            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	            VK_SHADER_STAGE_VERTEX_BIT,
	            0),
	        vkb::initializers::descriptor_set_layout_binding(
	            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	            VK_SHADER_STAGE_FRAGMENT_BIT,
	            1)};

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

void CameraPreview::setup_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        descriptor_pool,
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo buffer_descriptor = create_descriptor(*uniform_buffer_vs);

	VkDescriptorImageInfo image_descriptor;
	image_descriptor.imageView   = texture.view;
	image_descriptor.sampler     = texture.sampler;
	image_descriptor.imageLayout = texture.image_layout;

	std::vector<VkWriteDescriptorSet> write_descriptor_sets =
	    {
	        vkb::initializers::write_descriptor_set(
	            descriptor_set,
	            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	            0,
	            &buffer_descriptor),
	        vkb::initializers::write_descriptor_set(
	            descriptor_set,
	            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	            1,
	            &image_descriptor)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, NULL);
}

void CameraPreview::prepare_pipelines()
{
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

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(
	        VK_TRUE,
	        VK_TRUE,
	        VK_COMPARE_OP_LESS_OR_EQUAL);

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

	shader_stages[0] = load_shader("camera_preview", "image.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("camera_preview", "image.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	// Vertex bindings and attributes
	const std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(VertexStructure), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexStructure, pos)),
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexStructure, uv)),
	};
	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_input_attributes.data();

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

void CameraPreview::prepare_uniform_buffers()
{
	uniform_buffer_vs = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                         sizeof(ubo_vs),
	                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	                                                         VMA_MEMORY_USAGE_CPU_TO_GPU);

	update_uniform_buffers();
}

void CameraPreview::update_uniform_buffers()
{
	// Calculate proper orthographic projection to maintain image aspect ratio
	float left = -1.0f, right = 1.0f, bottom = -1.0f, top = 1.0f;
	
	// Adjust projection based on image and screen aspect ratios
	if (current_image_aspect_ratio > screen_aspect_ratio) {
		// Image is wider than screen - fit width, letterbox height
		float scale = screen_aspect_ratio / current_image_aspect_ratio;
		bottom = -scale;
		top = scale;
	} else {
		// Image is taller than screen - fit height, pillarbox width  
		float scale = current_image_aspect_ratio / screen_aspect_ratio;
		left = -scale;
		right = scale;
	}
	
	ubo_vs.projection = glm::ortho(left, right, bottom, top, 0.0f, 1.0f);
	
	// Identity model matrix (no scaling/rotation/translation)
	ubo_vs.model = glm::mat4(1.0f);
	
	// Identity view matrix (no camera transformation)
	ubo_vs.view = glm::mat4(1.0f);

	uniform_buffer_vs->convert_and_update(ubo_vs);
}

bool CameraPreview::prepare(const vkb::ApplicationOptions &options)
{
	if (!ApiVulkanSample::prepare(options))
	{
		return false;
	}

	load_texture();
	generate_quad();
	prepare_uniform_buffers();
	setup_descriptor_set_layout();
	prepare_pipelines();
	setup_descriptor_pool();
	setup_descriptor_set();
	build_command_buffers();

	prepared = true;
	return true;
}

void CameraPreview::check_and_update_texture()
{
	LOGI("check_and_update_texture out");
#ifdef ANDROID
	LOGI("check_and_update_texture");
	// Check if texture update is needed
	if (check_texture_update_needed()) {
		LOGI("Texture update requested, checking for new image data");
		
		// Check if there's new image data from JNI
		std::vector<uint8_t> new_image_data;
		uint32_t new_width = 0;
		uint32_t new_height = 0;
		
		if (get_jni_image_data(new_image_data, new_width, new_height)) {
			LOGI("Updating texture with new image data: {}x{}, {} bytes", new_width, new_height, new_image_data.size());
			
			// Update image aspect ratio
			current_image_aspect_ratio = static_cast<float>(new_width) / static_cast<float>(new_height);
			LOGI("Image aspect ratio updated: {} ({}x{})", current_image_aspect_ratio, new_width, new_height);
			
			// Wait for device to be idle before destroying old texture
			vkDeviceWaitIdle(get_device().get_handle());
			
			// Destroy old texture
			destroy_texture(texture);
			
			// Create new texture from the image data
			create_texture_from_raw_data(new_image_data, new_width, new_height);
			
			// Verify that the new texture was created successfully
			if (texture.view == VK_NULL_HANDLE) {
				LOGE("Failed to create new texture view, cannot update descriptor set");
				return;
			}
			
			LOGI("New texture created successfully - view: {}, sampler: {}", 
				 reinterpret_cast<void*>(texture.view), reinterpret_cast<void*>(texture.sampler));
			
			// Update the existing descriptor set with new texture instead of allocating a new one
			VkDescriptorImageInfo image_descriptor;
			image_descriptor.imageView   = texture.view;
			image_descriptor.sampler     = texture.sampler;
			image_descriptor.imageLayout = texture.image_layout;

			VkWriteDescriptorSet write_descriptor_set = 
				vkb::initializers::write_descriptor_set(
					descriptor_set,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,  // binding point 1 (texture)
					&image_descriptor);

			LOGI("Updating descriptor set with new texture - binding: 1, imageView: {}", 
				 reinterpret_cast<void*>(image_descriptor.imageView));

			vkUpdateDescriptorSets(get_device().get_handle(), 1, &write_descriptor_set, 0, nullptr);
			
			// Update uniform buffers with new aspect ratio
			update_uniform_buffers();
			
			// Safely rebuild command buffers using the base class method
			rebuild_command_buffers();
			
			LOGI("Texture updated successfully");
		} else {
			LOGW("Texture update requested but no image data available");
		}
	}
#endif
}

void CameraPreview::render(float delta_time)
{
	if (!prepared)
		return;
		
	// Check for texture updates before rendering
	check_and_update_texture();
	
	draw();
}

void CameraPreview::view_changed()
{
	// Update screen aspect ratio when view changes
	screen_aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
	LOGI("Screen aspect ratio updated: {} ({}x{})", screen_aspect_ratio, width, height);
	update_uniform_buffers();
}

void CameraPreview::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("Camera Preview"))
	{
		drawer.text("Displaying a simple image texture");
		drawer.text("Image size: %d x %d", texture.width, texture.height);
		drawer.text("Mip levels: %d", texture.mip_levels);
	}
}

void CameraPreview::create_texture_from_raw_data(const std::vector<uint8_t>& data, uint32_t width, uint32_t height)
{
	// Data from Java is now always in RGBA format
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	uint32_t expected_size = width * height * 4;
	uint32_t actual_size = static_cast<uint32_t>(data.size());
	
	LOGI("Image data analysis: {}x{}, actual size: {}, expected RGBA size: {}", 
		 width, height, actual_size, expected_size);
	
	if (actual_size != expected_size) {
		LOGE("Data size mismatch! Expected: {}, Actual: {}", expected_size, actual_size);
		return;
	}
	
	texture.width = width;
	texture.height = height;
	texture.mip_levels = 1; // No mipmaps for custom images

	// Create optimal tiled target image on the device
	VkImageCreateInfo image_create_info = vkb::initializers::image_create_info();
	image_create_info.imageType         = VK_IMAGE_TYPE_2D;
	image_create_info.format            = format;
	image_create_info.mipLevels         = texture.mip_levels;
	image_create_info.arrayLayers       = 1;
	image_create_info.samples           = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling            = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.extent            = {texture.width, texture.height, 1};
	image_create_info.usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK(vkCreateImage(get_device().get_handle(), &image_create_info, nullptr, &texture.image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(get_device().get_handle(), texture.image, &memory_requirements);
	
	VkMemoryAllocateInfo memory_allocate_info = vkb::initializers::memory_allocate_info();
	memory_allocate_info.allocationSize  = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = get_device().get_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(get_device().get_handle(), &memory_allocate_info, nullptr, &texture.device_memory));
	VK_CHECK(vkBindImageMemory(get_device().get_handle(), texture.image, texture.device_memory, 0));

	// Create staging buffer with the actual data size
	vkb::core::BufferC stage_buffer = vkb::core::BufferC::create_staging_buffer(get_device(), data.size(), data.data());

	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseMipLevel            = 0;
	subresource_range.levelCount              = 1;
	subresource_range.layerCount              = 1;

	VkCommandBuffer copy_cmd = get_device().create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferImageCopy buffer_copy_region               = {};
	buffer_copy_region.imageSubresource.aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT;
	buffer_copy_region.imageSubresource.mipLevel      = 0;
	buffer_copy_region.imageSubresource.baseArrayLayer = 0;
	buffer_copy_region.imageSubresource.layerCount    = 1;
	buffer_copy_region.imageExtent.width              = width;
	buffer_copy_region.imageExtent.height             = height;
	buffer_copy_region.imageExtent.depth              = 1;
	buffer_copy_region.bufferOffset                   = 0;

	// Image barrier for optimal image (target)
	vkb::image_layout_transition(
	    copy_cmd,
	    texture.image,
	    VK_IMAGE_LAYOUT_UNDEFINED,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    subresource_range);

	// Copy from staging buffer
	vkCmdCopyBufferToImage(
	    copy_cmd,
	    stage_buffer.get_handle(),
	    texture.image,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    1,
	    &buffer_copy_region);

	// Change texture image layout to shader read
	texture.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkb::image_layout_transition(
	    copy_cmd,
	    texture.image,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    texture.image_layout,
	    subresource_range);

	get_device().flush_command_buffer(copy_cmd, queue, true);

	// Calculate valid filter and mipmap modes
	VkFilter            filter      = VK_FILTER_LINEAR;
	VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkb::make_filters_valid(get_device().get_gpu().get_handle(), format, &filter, &mipmap_mode);

	// Create a texture sampler
	VkSamplerCreateInfo sampler = vkb::initializers::sampler_create_info();
	sampler.magFilter           = filter;
	sampler.minFilter           = filter;
	sampler.mipmapMode          = mipmap_mode;
	sampler.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.mipLodBias          = 0.0f;
	sampler.compareOp           = VK_COMPARE_OP_NEVER;
	sampler.minLod              = 0.0f;
	sampler.maxLod              = 1.0f;
	// Enable anisotropic filtering if supported
	if (get_device().get_gpu().get_features().samplerAnisotropy)
	{
		sampler.maxAnisotropy    = get_device().get_gpu().get_properties().limits.maxSamplerAnisotropy;
		sampler.anisotropyEnable = VK_TRUE;
	}
	else
	{
		sampler.maxAnisotropy    = 1.0;
		sampler.anisotropyEnable = VK_FALSE;
	}
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(get_device().get_handle(), &sampler, nullptr, &texture.sampler));

	// Create image view
	VkImageViewCreateInfo view = vkb::initializers::image_view_create_info();
	view.viewType              = VK_IMAGE_VIEW_TYPE_2D;
	view.format                = format;
	view.components            = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
	view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.baseMipLevel   = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount     = 1;
	view.subresourceRange.levelCount     = 1;
	view.image = texture.image;
	VK_CHECK(vkCreateImageView(get_device().get_handle(), &view, nullptr, &texture.view));
}

std::unique_ptr<vkb::VulkanSampleC> create_camera_preview()
{
	return std::make_unique<CameraPreview>();
}
