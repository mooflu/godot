#pragma once

#include "servers/rendering/rendering_shader_container.h"

#include "webgpu/webgpu.h"

class RenderingShaderContainerWebGpu : public RenderingShaderContainer {
	GDSOFTCLASS(RenderingShaderContainerWebGpu, RenderingShaderContainer);
public:
	struct UniformData {
		// Applies for UNIFORM_TYPE_TEXTURE and UNIFORM_TYPE_SAMPLER_WITH_TEXTURE.
		bool texture_is_multisample = false;

		// Applies for UNIFORM_TYPE_IMAGE.
		WGPUTextureFormat image_format = WGPUTextureFormat_Undefined;

		// For texture and image uniform types.
		WGPUTextureViewDimension texture_image_type = WGPUTextureViewDimension_Undefined;
		WGPUTextureSampleType texture_sample_type = WGPUTextureSampleType_Undefined;
		WGPUStorageTextureAccess image_access = WGPUStorageTextureAccess_Undefined;
		bool read_only_storage = false;
	};

	struct WebGpuShaderReflection {
		Vector<Vector<UniformData>> uniform_sets;
	};

	WebGpuShaderReflection get_webgpu_shader_reflection() const;

protected:
	virtual uint32_t _from_bytes_reflection_binding_uniform_extra_data_start(const uint8_t *p_bytes) override;
	virtual uint32_t _from_bytes_reflection_binding_uniform_extra_data(const uint8_t *p_bytes, uint32_t p_index) override;
	virtual uint32_t _to_bytes_reflection_binding_uniform_extra_data(uint8_t *p_bytes, uint32_t p_index) const override;

	virtual uint32_t _format() const override;
	virtual uint32_t _format_version() const override;
	virtual bool _set_code_from_spirv(const ReflectShader &p_shader) override;

private:
	Vector<UniformData> webgpu_reflection_binding_set_uniforms_data; // compliment to reflection_binding_set_uniforms_data
};

class RenderingShaderContainerFormatWebGpu: public RenderingShaderContainerFormat {
public:
	virtual Ref<RenderingShaderContainer> create_container() const {
        return memnew(RenderingShaderContainerWebGpu());
    }
	virtual RenderingDeviceCommons::ShaderLanguageVersion get_shader_language_version() const {
	    return RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1;
    }
	virtual RenderingDeviceCommons::ShaderSpirvVersion get_shader_spirv_version() const {
	  	return RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_3; // Tint validates against Vulkan 1.1 / spir-v 1.3
    }
	RenderingShaderContainerFormatWebGpu() = default;
	virtual ~RenderingShaderContainerFormatWebGpu() = default;
};
