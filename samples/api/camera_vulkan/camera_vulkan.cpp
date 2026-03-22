/* Copyright (c) 2019-2025, User Modified
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example: Native Vulkan sample that uses camera input
 * This would be added to samples/api/ folder
 */

#include "rendering/render_pipeline.h"
#include "scene_graph/components/camera.h"
#include "scene_graph/components/image.h"
#include "scene_graph/components/sampler.h"
#include "scene_graph/components/sub_mesh.h"
#include "scene_graph/components/texture.h"
#include "scene_graph/components/material.h"
#include "scene_graph/components/mesh.h"

// Platform header for accessing the Surface
#include "platform/android/android_surface_platform.h"

namespace vkb
{
class CameraVulkanSample : public VulkanSample
{
  public:
	CameraVulkanSample()
	{
		set_name("Camera Vulkan Sample");
		set_description("Renders camera preview using Vulkan");
	}

	virtual ~CameraVulkanSample() = default;

	virtual void prepare() override
	{
		VulkanSample::prepare();

		// Create a render pipeline
		render_pipeline = std::make_unique<RenderPipeline>();

		// Load shaders
		load_shaders();

		// Setup camera texture
		setup_camera_texture();

		// Create quad to render camera feed
		create_render_quad();
	}

	virtual void update(float delta_time) override
	{
		// Update camera texture if new frame available
		if (camera_frame_available)
		{
			update_camera_texture();
			camera_frame_available = false;
		}

		VulkanSample::update(delta_time);
	}

	virtual void draw(vkb::CommandBuffer &command_buffer, vkb::RenderTarget &render_target) override
	{
		// Bind pipeline and render the camera quad
		auto &views = render_pipeline->get_active_views();

		// Set up render pass
		render_pipeline->draw(command_buffer, render_target);

		// Draw the camera texture quad
		command_buffer.bind_pipeline_layout(*pipeline_layout);
		command_buffer.bind_image(camera_texture, *sampler, 0, 0, 0);
		command_buffer.draw(4, 1, 0, 0);
	}

  private:
	std::unique_ptr<RenderPipeline> render_pipeline;
	std::unique_ptr<PipelineLayout> pipeline_layout;
	std::shared_ptr<Image> camera_texture;
	std::shared_ptr<Sampler> sampler;

	bool camera_frame_available{false};

	void load_shaders()
	{
		// Load vertex and fragment shaders
		// Shaders should convert YUV to RGB and apply any effects
		auto &resource_cache = device->get_resource_cache();
		resource_cache.request_shader_module(VK_SHADER_STAGE_VERTEX_BIT, SHADER_FILE("camera_texture.vert"));
		resource_cache.request_shader_module(VK_SHADER_STAGE_FRAGMENT_BIT, SHADER_FILE("camera_texture.frag"));
	}

	void setup_camera_texture()
	{
		// Create texture for camera input
		// Size should match camera preview size
		camera_texture = std::make_shared<Image>(*device, VkExtent3D{1920, 1080, 1},
		                                         VK_FORMAT_R8G8B8A8_UNORM,
		                                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		                                         VMA_MEMORY_USAGE_GPU_ONLY);

		// Create sampler
		VkSamplerCreateInfo sampler_info{};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		sampler = std::make_shared<Sampler>(*device, sampler_info);
	}

	void create_render_quad()
	{
		// Create a fullscreen quad mesh for rendering camera texture
		// This can be done using scene graph or direct vertex buffer
	}

	void update_camera_texture()
	{
		// Copy new camera frame to texture
		// This would be called from JNI callback when camera frame is available
	}

  public:
	// Called from JNI when camera frame is available
	void on_camera_frame_available()
	{
		camera_frame_available = true;
	}
};

// Register the sample
std::unique_ptr<Application> create_camera_vulkan_sample()
{
	return std::make_unique<CameraVulkanSample>();
}

} // namespace vkb
