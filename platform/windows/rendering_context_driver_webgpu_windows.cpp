/**************************************************************************/
/*  rendering_context_driver_vulkan_windows.cpp                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#if defined(WINDOWS_ENABLED) && defined(WEBGPU_ENABLED)

#include "rendering_context_driver_webgpu_windows.h"

RenderingContextDriver::SurfaceID RenderingContextDriverWebGpuWindows::surface_create(const void *p_platform_data) {
	const WindowPlatformData *wpd = (const WindowPlatformData *)(p_platform_data);

	const WGPUSurfaceSourceWindowsHWND winHWND_desc =
			(const WGPUSurfaceSourceWindowsHWND){
				.chain =
						(const WGPUChainedStruct){
								.sType = WGPUSType_SurfaceSourceWindowsHWND,
						},
				.hinstance = wpd->instance,
				.hwnd = wpd->window,
			};

	WGPUSurfaceDescriptor surface_desc =
			(WGPUSurfaceDescriptor){
				.nextInChain =
						(const WGPUChainedStruct *)&winHWND_desc
			};

	WGPUSurface wgpu_surface = wgpuInstanceCreateSurface(
			instance_get(),
			&surface_desc);

	ERR_FAIL_COND_V(!wgpu_surface, SurfaceID());

	Surface *surface = memnew(Surface);
	surface->surface = wgpu_surface;
	return SurfaceID(surface);
}


RenderingContextDriverWebGpuWindows::RenderingContextDriverWebGpuWindows() {
	// Does nothing.
}

RenderingContextDriverWebGpuWindows::~RenderingContextDriverWebGpuWindows() {
	// Does nothing.
}

#endif // WINDOWS_ENABLED && WEBGPU_ENABLED
