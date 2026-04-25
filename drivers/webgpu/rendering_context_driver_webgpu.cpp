#ifdef WEBGPU_ENABLED

#include "webgpu/webgpu.h"

#include "rendering_context_driver_webgpu.h"

#include "core/error/error_macros.h"

#include "rendering_device_driver_webgpu.h"
#include "webgpu_conv.h"

static void handle_request_adapter(WGPURequestAdapterStatus status,
		WGPUAdapter adapter, WGPUStringView message,
		void *userdata, void *_) {
	ERR_FAIL_COND_V_MSG(
			status != WGPURequestAdapterStatus_Success, (void)0,
			vformat("Failed to get wgpu adapter: %s", message.data));

	print_verbose("handle_request_adapter: status=" + itos(status));
	WGPUAdapterInfo info;
	WGPUStatus s = wgpuAdapterGetInfo(adapter, &info);
	print_verbose("wgpuAdapterGetInfo: status=" + itos(s));

	if (s == WGPUStatus_Success) {
		RenderingContextDriver::Device device;
		device.name = String::utf8(info.device.data, info.device.length);
		device.vendor = info.vendorID;
		device.type = webgpu_adapter_type_to_device_type(info.adapterType);

		RenderingContextDriverWebGpu *context = (RenderingContextDriverWebGpu *)userdata;
		context->adapter_push_back(adapter, device);
	}
}

RenderingContextDriverWebGpu::RenderingContextDriverWebGpu() {
}

RenderingContextDriverWebGpu::~RenderingContextDriverWebGpu() {
	if (instance != nullptr) {
		wgpuInstanceRelease(instance);
	}

	for (WGPUAdapter &adapter : adapters) {
		wgpuAdapterRelease(adapter);
	}
}

Error RenderingContextDriverWebGpu::initialize() {
	print_verbose("RenderingContextDriverWebGpu::initialize");

	WGPUChainedStruct *nextInChain = nullptr;
#define WEBGPU_DAWN_ENABLE_TOGGLES
#ifdef WEBGPU_DAWN_ENABLE_TOGGLES
	// See toggles: https://github.com/google/dawn/blob/main/src/dawn/native/Toggles.cpp
	// allow_unsafe_apis to access ImmediateAddressSpace (aka push constants) and TexelBuffers
    const char *toggles[] = {"allow_unsafe_apis"}; // allow_unsafe_apis, enable_subgroups_intel_gen9, expose_wgsl_experimental_features
	WGPUDawnTogglesDescriptor dawnToggles{};
	dawnToggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
	dawnToggles.enabledToggleCount = 1;
	dawnToggles.enabledToggles = toggles;
	nextInChain = &dawnToggles.chain;
#endif // WEBGPU_DAWN_ENABLE_TOGGLES

	static const auto kTimedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
	WGPUInstanceDescriptor instance_descriptor{
		.nextInChain = nextInChain,
		.requiredFeatureCount = 1,
		.requiredFeatures = &kTimedWaitAny,
	};
	instance = wgpuCreateInstance(&instance_descriptor);

	// We can check whether there is actually an instance created
	if (!instance) {
		print_verbose("Could not initialize WebGPU instance");
	}

	WGPURequestAdapterCallbackInfo adapter_callback_info = {
		.nextInChain = nullptr,
		.mode = WGPUCallbackMode_WaitAnyOnly,
		.callback = handle_request_adapter,
		.userdata1 = this,
	};

	// There is no way to request all adapters, so we just get the high and low power ones.

	WGPURequestAdapterOptions adapter_options = {
		.powerPreference = WGPUPowerPreference_HighPerformance,
	};
	WGPUFuture f1 = wgpuInstanceRequestAdapter(instance,
			&adapter_options,
			adapter_callback_info);

			WGPUFutureWaitInfo waitInfo{
		.future = f1,
		.completed = false,
	};
	wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX);

	adapter_options.powerPreference = WGPUPowerPreference_LowPower;
	WGPUFuture f2 = wgpuInstanceRequestAdapter(instance,
			&adapter_options,
			adapter_callback_info);

	WGPUFutureWaitInfo waitInfo2{
		.future = f2,
		.completed = false,
	};
	wgpuInstanceWaitAny(instance, 1, &waitInfo2, UINT64_MAX);

	return OK;
}

const RenderingContextDriver::Device &RenderingContextDriverWebGpu::device_get(uint32_t p_device_index) const {
	DEV_ASSERT(p_device_index < adapters.size());
	const RenderingContextDriver::Device &driver_device = driver_devices[p_device_index];
	return driver_device;
}

uint32_t RenderingContextDriverWebGpu::device_get_count() const {
	return adapters.size();
}

bool RenderingContextDriverWebGpu::device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const {
	DEV_ASSERT(p_device_index < adapters.size());
	WGPUAdapter adapter = adapters[p_device_index];
	Surface *surface = (Surface *)p_surface;
	WGPUSurfaceCapabilities caps;
	wgpuSurfaceGetCapabilities(surface->surface, adapter, &caps);
	return caps.formatCount != 0;
}

RenderingDeviceDriver *RenderingContextDriverWebGpu::driver_create() {
	return memnew(RenderingDeviceDriverWebGpu(this));
}

void RenderingContextDriverWebGpu::driver_free(RenderingDeviceDriver *p_driver) {
	memdelete(p_driver);
}

RenderingContextDriver::SurfaceID RenderingContextDriverWebGpu::surface_create(const void *p_platform_data) {
	DEV_ASSERT(false && "Surface creation should not be called on the platform-agnostic version of the driver.");
	return SurfaceID();
}

void RenderingContextDriverWebGpu::surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) {
	Surface *surface = (Surface *)(p_surface);
	surface->width = p_width;
	surface->height = p_height;
	surface->needs_resize = true;
}

void RenderingContextDriverWebGpu::surface_set_vsync_mode(SurfaceID p_surface, DisplayServerEnums::VSyncMode p_vsync_mode) {
	Surface *surface = (Surface *)(p_surface);
	surface->vsync_mode = p_vsync_mode;
	surface->needs_resize = true;
}

DisplayServerEnums::VSyncMode RenderingContextDriverWebGpu::surface_get_vsync_mode(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->vsync_mode;
}


void RenderingContextDriverWebGpu::surface_set_hdr_output_enabled(SurfaceID p_surface, bool p_enabled) {
	Surface *surface = (Surface *)(p_surface);
	surface->hdr_output = p_enabled;
	surface->needs_resize = true;
}

bool RenderingContextDriverWebGpu::surface_get_hdr_output_enabled(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->hdr_output;
}

void RenderingContextDriverWebGpu::surface_set_hdr_output_reference_luminance(SurfaceID p_surface, float p_reference_luminance) {
	Surface *surface = (Surface *)(p_surface);
	surface->hdr_reference_luminance = p_reference_luminance;
}

float RenderingContextDriverWebGpu::surface_get_hdr_output_reference_luminance(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->hdr_reference_luminance;
}

void RenderingContextDriverWebGpu::surface_set_hdr_output_max_luminance(SurfaceID p_surface, float p_max_luminance) {
	Surface *surface = (Surface *)(p_surface);
	surface->hdr_max_luminance = p_max_luminance;
}

float RenderingContextDriverWebGpu::surface_get_hdr_output_max_luminance(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->hdr_max_luminance;
}

void RenderingContextDriverWebGpu::surface_set_hdr_output_linear_luminance_scale(SurfaceID p_surface, float p_linear_luminance_scale) {
	Surface *surface = (Surface *)(p_surface);
	surface->hdr_linear_luminance_scale = p_linear_luminance_scale;
}

float RenderingContextDriverWebGpu::surface_get_hdr_output_linear_luminance_scale(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->hdr_linear_luminance_scale;
}

float RenderingContextDriverWebGpu::surface_get_hdr_output_max_value(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return MAX(surface->hdr_max_luminance / MAX(surface->hdr_reference_luminance, 1.0f), 1.0f);
}

uint32_t RenderingContextDriverWebGpu::surface_get_width(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->width;
}

uint32_t RenderingContextDriverWebGpu::surface_get_height(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->height;
}

void RenderingContextDriverWebGpu::surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) {
	Surface *surface = (Surface *)(p_surface);
	surface->needs_resize = p_needs_resize;
}

bool RenderingContextDriverWebGpu::surface_get_needs_resize(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->needs_resize;
}

void RenderingContextDriverWebGpu::surface_destroy(SurfaceID p_surface) {
	Surface *surface = (Surface *)(p_surface);
	wgpuSurfaceRelease(surface->surface);
	memdelete(surface);
}
bool RenderingContextDriverWebGpu::is_debug_utils_enabled() const {
	return false;
}

WGPUInstance RenderingContextDriverWebGpu::instance_get() const {
	return instance;
}

WGPUAdapter RenderingContextDriverWebGpu::adapter_get(uint32_t p_adapter_index) const {
	DEV_ASSERT(p_adapter_index < adapters.size());
	WGPUAdapter adapter = adapters[p_adapter_index];
	return adapter;
}

void RenderingContextDriverWebGpu::adapter_push_back(WGPUAdapter p_adapter, Device p_device) {
	adapters.push_back(p_adapter);
	driver_devices.push_back(p_device);
}

void RenderingContextDriverWebGpu::Surface::configure(WGPUAdapter p_adapter, WGPUDevice p_device) {
	WGPUSurfaceCapabilities capabilities;
	wgpuSurfaceGetCapabilities(surface, p_adapter, &capabilities);

	// Godot only supports these swapchain formats.
	for (uint32_t i = 0; i < capabilities.formatCount; i++) {
		WGPUTextureFormat tex_format = capabilities.formats[i];
		switch (tex_format) {
			case WGPUTextureFormat_BGRA8Unorm:
				this->format = tex_format;
				this->rd_format = RDD::DATA_FORMAT_B8G8R8A8_UNORM;
				i = capabilities.formatCount; // pick first match
				break;
			case WGPUTextureFormat_RGBA8Unorm:
				this->format = tex_format;
				this->rd_format = RDD::DATA_FORMAT_R8G8B8A8_UNORM;
				i = capabilities.formatCount; // pick first match
				break;
			default:
				break;
		}
	}

	WGPUSurfaceConfiguration surface_config = (WGPUSurfaceConfiguration){
		.device = p_device,
		.format = this->format,
		.usage = WGPUTextureUsage_RenderAttachment,
		.width = this->width,
		.height = this->height,
		.viewFormatCount = 0,
		.viewFormats = nullptr,
		.alphaMode = WGPUCompositeAlphaMode_Auto,
		.presentMode = WGPUPresentMode_Undefined,
	};

	wgpuSurfaceConfigure(this->surface, &surface_config);
}

#endif // WEBGPU_ENABLED
