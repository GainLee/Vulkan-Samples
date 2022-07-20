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

#include "camera_preview.h"

#include "common/vk_common.h"
#include "gltf_loader.h"
#include "gui.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "rendering/subpasses/forward_subpass.h"
#include "stats/stats.h"

CameraPreview::CameraPreview()
{
}

bool CameraPreview::prepare(vkb::Platform &platform)
{
	if (!VulkanSample::prepare(platform))
	{
		return false;
	}

	// Example Scene Render Pipeline
	vkb::ShaderSource vert_shader("triangle.vert");
	vkb::ShaderSource frag_shader("triangle.frag");
	auto              triangle_subpass   = std::make_unique<TriangleSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader));
	auto              render_pipeline = vkb::RenderPipeline();
	render_pipeline.add_subpass(std::move(triangle_subpass));
	set_render_pipeline(std::move(render_pipeline));

	// Add a GUI with the stats you want to monitor
	stats->request_stats({/*stats you require*/});
	gui = std::make_unique<vkb::Gui>(*this, platform.get_window(), stats.get());

	return true;
}

std::unique_ptr<vkb::VulkanSample> create_camera_preview()
{
	return std::make_unique<CameraPreview>();
}

CameraPreview::TriangleSubpass::TriangleSubpass(vkb::RenderContext &render_context, vkb::ShaderSource &&vertex_shader, vkb::ShaderSource &&fragment_shader) :
        vkb::Subpass(render_context, std::move(vertex_shader), std::move(fragment_shader))
{
}

void CameraPreview::TriangleSubpass::prepare()
{
    auto &device   = render_context.get_device();
    auto &vertex   = device.get_resource_cache().request_shader_module(VK_SHADER_STAGE_VERTEX_BIT, get_vertex_shader());
    auto &fragment = device.get_resource_cache().request_shader_module(VK_SHADER_STAGE_FRAGMENT_BIT, get_fragment_shader());
    layout         = &device.get_resource_cache().request_pipeline_layout({&vertex, &fragment});
}

void CameraPreview::TriangleSubpass::draw(vkb::CommandBuffer &command_buffer)
{
    command_buffer.bind_pipeline_layout(*layout);

    // A depth-stencil attachment exists in the default render pass, make sure we ignore it.
    /*vkb::DepthStencilState ds_state = {};
    ds_state.depth_test_enable      = VK_FALSE;
    ds_state.stencil_test_enable    = VK_FALSE;
    ds_state.depth_write_enable     = VK_FALSE;
    ds_state.depth_compare_op       = VK_COMPARE_OP_ALWAYS;
    command_buffer.set_depth_stencil_state(ds_state);*/

    command_buffer.draw(3, 1, 0, 0);
}
