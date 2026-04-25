#include "rendering_shader_container_webgpu.h"
#include "webgpu_conv.h"

#include <thirdparty/spirv-reflect/spirv_reflect.h>

#include "tint/tint.h"

#include "src/tint/utils/result.h"

#include "src/tint/lang/spirv/reader/reader.h"
#include "src/tint/lang/wgsl/writer/writer.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/lang/core/type/sampler.h"
#include "src/tint/lang/core/ir/referenced_module_vars.h"

#include <vector>

#ifdef DEBUG_DUMP_SHADER
#include "core/io/file_access.h"
#endif

uint32_t RenderingShaderContainerWebGpu::_format() const {
    return 0x57475055; // WGPU
}

uint32_t RenderingShaderContainerWebGpu::_format_version() const {
    return 1;
}


uint32_t RenderingShaderContainerWebGpu::_to_bytes_reflection_binding_uniform_extra_data(uint8_t *p_bytes, uint32_t p_index) const {
	if (p_bytes != nullptr) {
		*(UniformData *)p_bytes = webgpu_reflection_binding_set_uniforms_data[p_index];
	}
	return sizeof(UniformData);
}

uint32_t RenderingShaderContainerWebGpu::_from_bytes_reflection_binding_uniform_extra_data_start(const uint8_t *p_bytes) {
	webgpu_reflection_binding_set_uniforms_data.resize(reflection_binding_set_uniforms_data.size());
	return 0;
}

uint32_t RenderingShaderContainerWebGpu::_from_bytes_reflection_binding_uniform_extra_data(const uint8_t *p_bytes, uint32_t p_index) {
	webgpu_reflection_binding_set_uniforms_data.ptrw()[p_index] = *(UniformData *)p_bytes;
	return sizeof(UniformData);
}

bool RenderingShaderContainerWebGpu::_set_code_from_spirv(const ReflectShader &p_shader) {
    const LocalVector<ReflectShaderStage> &p_spirv = p_shader.shader_stages;
    String sname(shader_name.get_data());

    // Note: patched some glsl code in compile_glslang_shader avoid TINT errors below

    if (
        sname.begins_with("SdfgiPreprocessShaderRD") ||
// fails with: arrays of handle types are not supported
        sname.begins_with("SdfgiDirectLightShaderRD") ||
// fails with: arrays of handle types are not supported
        sname.begins_with("SdfgiDebugShaderRD") ||
// fails with: arrays of handle types are not supported
        sname.begins_with("SdfgiIntegrateShaderRD") ||
// fails with: arrays of handle types are not supported
        sname.begins_with("FsrUpscaleShaderRD") ||
// fails with: tint/lang/spirv/reader/parser/parser.cc:732 internal compiler error: TINT_ASSERT(int_ty->width() == 32)
        sname.begins_with("ResolveShaderRD") ||
// fails with: tint/lang/spirv/reader/parser/parser.cc:3156 internal compiler error: TINT_ASSERT(img_type->GetMultisampled() != type::Multisampled::kMultisampled) Creating an OpTypeSampledImage from a multisampled image is not supported
        // sname.begins_with("SceneForwardClusteredShaderRD:18") ||
        // sname.begins_with("SceneForwardClusteredShaderRD:19") ||
// fails with: tint/lang/spirv/reader/lower/texture.cc:606 internal compiler error: TINT_ASSERT(tex_ty)
        sname.begins_with("VoxelGiDebugShaderRD") ||
// fails with: error: var with 'storage' address space and 'read_write' access mode cannot be used by vertex pipeline stage
        sname.begins_with("ClusterDebugShaderRD") ||
// fails with: sample type error for layout(set = 0, binding = 3) uniform texture2D depth_buffer;
#if 0
ERROR: webgpu error: None of the supported sample types (UnfilterableFloat|Depth) of [Texture "texture format: 49 dim: 2"] match the expected sample types (Float).
 - While validating entries[2] against { binding: 3, visibility: ShaderStage::Compute, texture: {sampleType: TextureSampleType::Float, viewDimension: TextureViewDimension::e2D, multisampled: 0} }.
 - While validating [BindGroupDescriptor] against [BindGroupLayout "uniform set 0 for ClusterDebugShaderRD:0"]
 - While calling [Device].CreateBindGroup([BindGroupDescriptor]).
#endif
        false
    ) {
        // fails in TINT
        WARN_PRINT("Skipping: " + sname);
        return false;
    }

#if 0
    if (sname.begins_with("ParticlesShaderRD")) {
        // Tint trips up with an atomic related assert when accessed as src_particles.data[src_index].xform[3]

        // fails in TINT
        // dawn/src/tint/lang/spirv/reader/lower/atomics.cc:360 internal compiler error: TINT_ASSERT(ld->From()->Type()->UnwrapPtr()->Is<core::type::Atomic>())
        WARN_PRINT("Skipping: " + sname);
        return false;
    }
    if (sname.begins_with("ParticlesCopyShaderRD")) {
        // fails in TINT
        // dawn/src/tint/lang/core/constant/scalar.h:59 internal compiler error: TINT_ASSERT(std::isfinite(v.value))
        // after change to avoid 1/0
#if 0
********* SpirvToWgsl error for ParticlesCopyShaderRD:1:
:77:48 error: var: vars in the 'storage' address space must have access 'read' or 'read-write'
  %instances:ptr<storage, Transforms, write> = var undef @binding_point(0, 2)
                                               ^^^

:73:1 note: in block
$B1: {  # root
#endif
        WARN_PRINT("Skipping: " + sname);
        return false;
    }
    if (sname.contains("SkeletonShaderRD")) {
        // fails WGSL conversion with syntax error
#if 0
********* SpirvToWgsl error for SkeletonShaderRD:0:
:53:54 error: var: vars in the 'storage' address space must have access 'read' or 'read-write'
  %dst_vertices:ptr<storage, DstVertexData, write> = var undef @binding_point(0, 1)
                                                     ^^^

:46:1 note: in block
$B1: {  # root
^^^
#endif
        WARN_PRINT("Skipping: " + sname);
        return false;
    }
#endif

    webgpu_reflection_binding_set_uniforms_data.resize(reflection_binding_set_uniforms_data.size());
    UniformData *ud = webgpu_reflection_binding_set_uniforms_data.ptrw();

    for (const ReflectDescriptorSet &dset : p_shader.uniform_sets) {
        for (const ReflectUniform &runi : dset) {
            const SpvReflectDescriptorBinding &binding = runi.get_spv_reflect();
            UniformData &uniform = *ud;

            bool need_texture_is_multisample = false;
            bool need_image_format = false;
            bool need_image_access = false;
            bool need_texture_image_type = false;
            bool need_texture_sample_type = false;

            switch (binding.descriptor_type) {
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
                    need_texture_is_multisample = true;
                    need_texture_image_type = true;
                    need_texture_sample_type = true;
                } break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                    need_texture_is_multisample = true;
                    need_texture_image_type = true;
                    need_texture_sample_type = true;
                } break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
                    need_texture_is_multisample = true;
                    need_texture_image_type = true;
                    need_texture_sample_type = true;
                } break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                    need_image_format = true;
                    need_image_access = true;
                    need_texture_image_type = true;
                    need_texture_sample_type = false;
                } break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                    need_image_access = true;
                } break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
                    need_texture_is_multisample = true;
                } break;
                default:
                break;
            }

            if (need_texture_is_multisample) {
                uniform.texture_is_multisample = binding.image.ms == 1;
            }

            if (need_image_format) {
                uniform.image_format = webgpu_texture_format_from_rd(runi.image.format);
            }

            if (need_image_access) {
                if (binding.decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE) {
                    uniform.image_access = WGPUStorageTextureAccess_WriteOnly;
                } else if (binding.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) {
                    uniform.image_access = WGPUStorageTextureAccess_ReadOnly;
                    uniform.read_only_storage = true;
                } else {
                    uniform.image_access = WGPUStorageTextureAccess_ReadWrite;
                    uniform.read_only_storage = false;
                    print_error("binding.decoration_flags: " + itos(binding.decoration_flags));
                }
            }

            if (need_texture_image_type) {
                switch (binding.image.dim) {
                    case SpvDim1D:
                        uniform.texture_image_type = webgpu_texture_view_dimension_from_rd(
                                binding.image.arrayed ? RDC::TEXTURE_TYPE_1D_ARRAY : RDC::TEXTURE_TYPE_1D);
                        break;
                    case SpvDim2D:
                        uniform.texture_image_type = webgpu_texture_view_dimension_from_rd(
                                binding.image.arrayed ? RDC::TEXTURE_TYPE_2D_ARRAY : RDC::TEXTURE_TYPE_2D);
                        break;
                    case SpvDim3D:
                        uniform.texture_image_type = webgpu_texture_view_dimension_from_rd(RDC::TEXTURE_TYPE_3D);
                        break;
                    case SpvDimCube:
                        uniform.texture_image_type = webgpu_texture_view_dimension_from_rd(
                                binding.image.arrayed ? RDC::TEXTURE_TYPE_CUBE_ARRAY : RDC::TEXTURE_TYPE_CUBE);
                        break;
                    case SpvDimRect:
                    case SpvDimBuffer:
                    case SpvDimSubpassData:
                    case SpvDimTileImageDataEXT:
                    case SpvDimMax:
                        print_error("Only 1D, 2D, 3D, Cube images and their array equivalents supported");
                        break;
                }
            }

            if (need_texture_sample_type) {
                if (binding.image.depth) {
                    WARN_PRINT(String::utf8(binding.name) + " depth " + itos(binding.image.depth) + " for binding: " + itos(runi.binding));
                    uniform.texture_sample_type = WGPUTextureSampleType_Depth;
                } else if (binding.type_description->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
                    uniform.texture_sample_type = WGPUTextureSampleType_Float;
                } else if (binding.type_description->type_flags & SPV_REFLECT_TYPE_FLAG_INT) {
                    if (binding.type_description->traits.numeric.scalar.signedness == 0) {
                        uniform.texture_sample_type = WGPUTextureSampleType_Uint;
                    } else {
                        uniform.texture_sample_type = WGPUTextureSampleType_Sint;
                    }
                } else {
                    uniform.texture_sample_type = WGPUTextureSampleType_Sint;
                }
            }

            ud++;
        }
    }

	shaders.resize(p_spirv.size());
	for (uint64_t i = 0; i < p_spirv.size(); i++) {
        const ReflectShaderStage &s = p_spirv[i];
        std::vector<uint32_t> spirvBin;
        for (uint32_t b: s.spirv()) {
            spirvBin.push_back(b);
        }

        tint::wgsl::writer::Options wgsl_options{};
        wgsl_options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
        wgsl_options.allow_non_uniform_derivatives = true;
        wgsl_options.allow_non_uniform_subgroup_operations = true;
        wgsl_options.disable_unreachable_code_warning = true;

#define USE_TINT_SPIRV_TO_WGSL
#ifdef USE_TINT_SPIRV_TO_WGSL

#ifdef DEBUG_DUMP_SHADER
        {
            static int count = 1;
            const Vector<uint8_t> &v = s.spirv_data();
            Error err;
            Ref<FileAccess> file = FileAccess::open("/home/frank/proj/webgpu/spv-debug-out/" + sname + "-" + itos(s.shader_stage) + "-" + itos(count) + ".spv", FileAccess::WRITE, &err);
            file->store_buffer(v);
            count++;
        }
#endif

        tint::Result<std::string> result = tint::SpirvToWgsl(spirvBin, wgsl_options);
        if (result != tint::Success) {
            const std::string &reason = result.Failure().reason;
            print_verbose("********* SpirvToWgsl error for " + sname + ":");
            print_verbose(String::utf8(reason.data(), (int)reason.length()));

            return false;
        }
#else
        // tint::SpirvToWgsl does not give access to the IR module, which we
        // need to extract some reflection data.
        /*
           1. sampler type (regular or comparison)
           2. texture type (regular or depth)
           Given glsl snippet
                layout(set = 0, binding = 2) uniform sampler shadow_sampler;
                layout(set = 1, binding = 5) uniform texture2D shadow_atlas;

           and later usage
                float d = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), pos.xy, 0.0).r;
                shadow += half(textureProj(sampler2DShadow(shadow_atlas, shadow_sampler), vec4(pos.xy, z_norm, 1.0)));

           tint will generate wgsl (note sampler_comparison and texture_depth_2d)
                @group(0u) @binding(2u) var shadow_sampler : sampler_comparison;
                @group(1u) @binding(5u) var shadow_atlas : texture_depth_2d;

                textureSampleLevel(shadow_atlas, SAMPLER_LINEAR_CLAMP, ...
                textureSampleCompare(shadow_atlas, shadow_sampler, ...
            3. binding number mapping as tint will split image samplers
                layout(set = 0, binding = 0) uniform sampler2D u_texture;
            to
                @group(0) @binding(0) var u_texture: texture_2d<f32>;
                @group(0) @binding(1) var u_sampler: sampler;
        */

        // Convert the SPIR-V program to an IR module.
        const tint::spirv::reader::Options ir_options = {};
        auto ir_from_spirv = tint::spirv::reader::ReadIR(spirvBin, ir_options);
        if (ir_from_spirv != tint::Success) {
            const std::string &reason = ir_from_spirv.Failure().reason;
            print_verbose("********* ir_from_spirv error for " + sname + ":");
            print_verbose(String::utf8(reason.data(), (int)reason.length()));
            return false;
        }

        tint::core::ir::Module &ir_module = ir_from_spirv.Get();

        // Convert the IR module to WGSL.
        auto wgsl_from_ir = tint::wgsl::writer::WgslFromIR(ir_module, wgsl_options);
        if (wgsl_from_ir != tint::Success) {
            const std::string &reason = wgsl_from_ir.Failure().reason;
            print_verbose("********* wgsl_from_ir error for " + sname + ":");
            print_verbose(String::utf8(reason.data(), (int)reason.length()));
            return false;
        }

        tint::Result<std::string> result = wgsl_from_ir.Get().wgsl;

        if (!ir_options.sampler_mappings.empty()) {
            // Note: see sampler_mappings comment:
            // "If this map is empty, any binding conflicts will be automatically resolved by incrementing binding numbers until they are unique"
            // We handle the empty case (see binding_offset in drivers/webgpu/rendering_device_driver_webgpu.cpp)
            ERR_PRINT("TODO: sampler_mappings not empty");
        }

        // Iterate over global instructions
        int binding_count[8]{};

        for (auto* inst : *(ir_module.root_block)) {
            auto* var = inst->As<tint::core::ir::Var>();
            if (!var || !var->BindingPoint().has_value()) continue;

            auto binding_point = var->BindingPoint().value();
            // WARN_PRINT("TINT IR group: " + itos(binding_point.group) + " binding: " + itos(binding_point.binding));
            binding_count[binding_point.group]++;

            auto* base_type = var->Result()->Type();
            if (auto* view = base_type->template As<tint::core::type::MemoryView>()) {
                base_type = view->StoreType();
            }

            if (auto* sampler = base_type->template As<tint::core::type::Sampler>()) {
                if (sampler->Kind() == tint::core::type::SamplerKind::kComparisonSampler) {
                    WARN_PRINT("Found kComparisonSampler");
                } else {
                    WARN_PRINT("Found kSampler");
                }
            }
            else if (auto* depth_tex = base_type->template As<tint::core::type::DepthTexture>()) {
                WARN_PRINT("Found DepthTexture");
            }
            else if (auto* tex = base_type->template As<tint::core::type::SampledTexture>()) {
                WARN_PRINT("Found SampledTexture (float/int/uint)");
            }
        }

        int spirv_binding_count[8]{};
        for (uint64_t i=0; i<p_shader.uniform_sets.size(); i++) {
            const ReflectDescriptorSet &dset = p_shader.uniform_sets[i];
            spirv_binding_count[i] = dset.size();
        }

        for (uint64_t i=0; i<8; i++) {
            if (binding_count[i] > 0 && spirv_binding_count[i] > 0) {
                if (spirv_binding_count[i] != binding_count[i]) {
                    ERR_PRINT("binding count mismatch: stage: " + itos(s.shader_stage) + " group: " + itos(i) +
                        " count: " + itos(spirv_binding_count[i]) +
                        " count: " + itos(binding_count[i]));
                }
                WARN_PRINT("  group: " + itos(i) + " count: " + itos(binding_count[i]));
            }
        }
#endif

        RenderingShaderContainer::Shader &shader = shaders.ptrw()[i];

        const std::string &wgsl_text = result.Get();
        shader.shader_stage = s.shader_stage;

    #if 0 // WEBGPU TODO: compress wgls shader for shader cache
        shader.code_compressed_bytes.resize(wgsl_text.size());

        uint32_t compressed_size = 0;
        shader.code_decompressed_size = wgsl_text.size();
        bool compressed = compress_code((const uint8_t *)wgsl_text.data(), wgsl_text.size(), shader.code_compressed_bytes.ptrw(), &compressed_size, &shader.code_compression_flags);
        ERR_FAIL_COND_V_MSG(!compressed, false, vformat("Failed to compress shader code %s", sname));

        shader.code_compressed_bytes.resize(compressed_size);
    #else
        // store uncompressed
        shader.code_compressed_bytes.resize(wgsl_text.size());
        memcpy(shader.code_compressed_bytes.ptrw(), wgsl_text.data(), wgsl_text.size());
        shader.code_decompressed_size = 0; // 0 == not compressed
    #endif

        // print_verbose(wgsl_text.c_str());
    }

    return true;
}

RenderingShaderContainerWebGpu::WebGpuShaderReflection RenderingShaderContainerWebGpu::get_webgpu_shader_reflection() const {
	WebGpuShaderReflection res;

	uint32_t uniform_set_count = reflection_binding_set_uniforms_count.size();
	uint32_t start = 0;
	res.uniform_sets.resize(uniform_set_count);
	for (uint32_t i = 0; i < uniform_set_count; i++) {
		Vector<UniformData> &set = res.uniform_sets.ptrw()[i];
		uint32_t count = reflection_binding_set_uniforms_count.get(i);
		set.resize(count);
		memcpy(set.ptrw(), &webgpu_reflection_binding_set_uniforms_data.ptr()[start], count * sizeof(UniformData));
		start += count;
	}

	return res;
}
