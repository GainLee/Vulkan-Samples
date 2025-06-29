/* Copyright (c) 2023, Thomas Atkinson
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

#include <core/platform/entrypoint.hpp>

#include "android/context.hpp"
#include "android/external_surface_context.hpp"

std::unique_ptr<vkb::PlatformContext> create_platform_context(android_app *app)
{
	return std::make_unique<vkb::AndroidPlatformContext>(app);
}

#ifdef EXTERNAL_SURFACE
std::unique_ptr<vkb::PlatformContext> create_platform_context(AAssetManager *asset_manager)
{
	return std::make_unique<vkb::ExternalSurfacePlatformContext>(asset_manager);
}
#endif