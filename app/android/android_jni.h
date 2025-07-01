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

#pragma once

#include <vector>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get image data that was passed from Java layer
 * @param data Output vector to store the image data
 * @param width Output width of the image
 * @param height Output height of the image
 * @return true if image data is available, false otherwise
 */
bool get_jni_image_data(std::vector<uint8_t>& data, uint32_t& width, uint32_t& height);

/**
 * @brief Check if texture update is needed and reset the flag
 * @return true if texture update is needed, false otherwise
 */
bool check_texture_update_needed();

/**
 * @brief Initialize the Vulkan sample (replaces part of nativeRunSample)
 * @return true if initialization was successful, false otherwise
 */
bool init_vulkan_sample();

/**
 * @brief Render one frame (replaces the main loop part of nativeRunSample)
 * @return true if frame was rendered successfully, false otherwise
 */
bool render_vulkan_frame();

/**
 * @brief Notify camera_preview sample that texture needs update
 */
void notify_texture_update_needed();

/**
 * @brief Terminate the Vulkan sample and clean up resources
 */
void terminate_vulkan_sample();

#ifdef __cplusplus
}
#endif 