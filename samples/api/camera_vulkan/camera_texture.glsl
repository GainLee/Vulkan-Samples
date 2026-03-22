/* Copyright (c) 2019-2025, User Modified
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Camera texture shaders for Vulkan rendering
 */

// Vertex Shader (camera_texture.vert)
#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 out_uv;

void main()
{
    gl_Position = vec4(in_pos, 0.0, 1.0);
    out_uv = in_uv;
}

// Fragment Shader (camera_texture.frag)
#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D camera_texture;

// YUV to RGB conversion matrix
const mat3 YUV_TO_RGB = mat3(
    1.0,  0.0,      1.402,
    1.0, -0.344136, -0.714136,
    1.0,  1.772,    0.0
);

void main()
{
    // Sample from camera texture
    // If texture is RGBA, use directly
    // If texture is YUV, convert to RGB

    vec4 tex_color = texture(camera_texture, in_uv);

    // Simple YUV to RGB conversion if needed
    // float y = tex_color.r;
    // float u = tex_color.g - 0.5;
    // float v = tex_color.b - 0.5;
    // vec3 rgb = YUV_TO_RGB * vec3(y, u, v);

    out_color = tex_color;
}
