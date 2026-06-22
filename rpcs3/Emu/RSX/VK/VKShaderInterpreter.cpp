#include "stdafx.h"

#include "VKShaderInterpreter.h"
#include "VKCommonPipelineLayout.h"
#include "VKVertexProgram.h"
#include "VKFragmentProgram.h"
#include "../Program/GLSLCommon.h"
#include "../Program/ShaderInterpreter.h"
#include "../rsx_methods.h"
#include "VKHelpers.h"
#include "VKRenderPass.h"

#include <chrono>
#include <thread>

namespace vk
{
	glsl::shader* shader_interpreter::build_vs(u64 compiler_options)
	{
		::glsl::shader_properties properties{};
		properties.domain = ::glsl::program_domain::glsl_vertex_program;
		properties.require_lit_emulation = true;

		// TODO: Extend decompiler thread
		// TODO: Rename decompiler thread, it no longer spawns a thread
		RSXVertexProgram null_prog;
		std::string shader_str;
		ParamArray arr;
		VKVertexProgram vk_prog;

		null_prog.ctrl = (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_INSTANCING) ? RSX_SHADER_CONTROL_INSTANCED_CONSTANTS : 0;
		VKVertexDecompilerThread comp(null_prog, shader_str, arr, vk_prog);

		// Initialize compiler properties
		comp.properties.has_indexed_constants = true;

		ParamType uniforms = {PF_PARAM_UNIFORM, "vec4"};
		uniforms.items.emplace_back("vc[468]", -1);

		std::stringstream builder;
		comp.insertHeader(builder);
		comp.insertConstants(builder, {uniforms});
		comp.insertInputs(builder, {});

		// Insert vp stream input
		builder << "\n"
				   "layout(std140, set=0, binding="
				<< m_vertex_instruction_start << ") readonly restrict buffer VertexInstructionBlock\n"
												 "{\n"
												 "	uint base_address;\n"
												 "	uint entry;\n"
												 "	uint output_mask;\n"
												 "	uint control;\n"
												 "	uvec4 vp_instructions[];\n"
												 "};\n\n";

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_INSTANCING)
		{
			builder << "#define _ENABLE_INSTANCED_CONSTANTS\n";
		}

		if (compiler_options)
		{
			builder << "\n";
		}

		::glsl::insert_glsl_legacy_function(builder, properties);
		::glsl::insert_vertex_input_fetch(builder, ::glsl::glsl_rules::glsl_rules_vulkan);

		builder << program_common::interpreter::get_vertex_interpreter();
		const std::string s = builder.str();

		auto vs = std::make_unique<glsl::shader>();
		vs->create(::glsl::program_domain::glsl_vertex_program, s);
		vs->compile();

		// Prepare input table
		const auto& binding_table = vk::get_current_renderer()->get_pipeline_binding_table();
		vk::glsl::program_input in;

		in.location = binding_table.vertex_params_bind_slot;
		in.domain = ::glsl::glsl_vertex_program;
		in.name = "VertexContextBuffer";
		in.type = vk::glsl::input_type_uniform_buffer;
		m_vs_inputs.push_back(in);

		in.location = binding_table.vertex_buffers_first_bind_slot;
		in.name = "persistent_input_stream";
		in.type = vk::glsl::input_type_texel_buffer;
		m_vs_inputs.push_back(in);

		in.location = binding_table.vertex_buffers_first_bind_slot + 1;
		in.name = "volatile_input_stream";
		in.type = vk::glsl::input_type_texel_buffer;
		m_vs_inputs.push_back(in);

		in.location = binding_table.vertex_buffers_first_bind_slot + 2;
		in.name = "vertex_layout_stream";
		in.type = vk::glsl::input_type_texel_buffer;
		m_vs_inputs.push_back(in);

		in.location = binding_table.vertex_constant_buffers_bind_slot;
		in.name = "VertexConstantsBuffer";
		in.type = vk::glsl::input_type_uniform_buffer;
		m_vs_inputs.push_back(in);

		// TODO: Bind textures if needed

		auto ret = vs.get();
		m_shader_cache[compiler_options].m_vs = std::move(vs);
		return ret;
	}

	glsl::shader* shader_interpreter::build_fs(u64 compiler_options)
	{
		[[maybe_unused]] ::glsl::shader_properties properties{};
		properties.domain = ::glsl::program_domain::glsl_fragment_program;
		properties.require_depth_conversion = true;
		properties.require_wpos = true;

		u32 len;
		ParamArray arr;
		std::string shader_str;
		RSXFragmentProgram frag;
		VKFragmentProgram vk_prog;
		VKFragmentDecompilerThread comp(shader_str, arr, frag, len, vk_prog);

		const auto& binding_table = vk::get_current_renderer()->get_pipeline_binding_table();
		std::stringstream builder;
		builder << "#version 450\n"
				   "#extension GL_ARB_separate_shader_objects : enable\n\n";

		::glsl::insert_subheader_block(builder);
		comp.insertConstants(builder);

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_GE)
		{
			builder << "#define ALPHA_TEST_GEQUAL\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_G)
		{
			builder << "#define ALPHA_TEST_GREATER\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_LE)
		{
			builder << "#define ALPHA_TEST_LEQUAL\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_L)
		{
			builder << "#define ALPHA_TEST_LESS\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_EQ)
		{
			builder << "#define ALPHA_TEST_EQUAL\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_NE)
		{
			builder << "#define ALPHA_TEST_NEQUAL\n";
		}

		if (!(compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_F32_EXPORT))
		{
			builder << "#define WITH_HALF_OUTPUT_REGISTER\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_DEPTH_EXPORT)
		{
			builder << "#define WITH_DEPTH_EXPORT\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_FLOW_CTRL)
		{
			builder << "#define WITH_FLOW_CTRL\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_PACKING)
		{
			builder << "#define WITH_PACKING\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_KIL)
		{
			builder << "#define WITH_KIL\n";
		}

		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_STIPPLING)
		{
			builder << "#define WITH_STIPPLING\n";
		}

		const char* type_names[] = {"sampler1D", "sampler2D", "sampler3D", "samplerCube"};
		if (compiler_options & program_common::interpreter::COMPILER_OPT_ENABLE_TEXTURES)
		{
			builder << "#define WITH_TEXTURES\n\n";

			for (int i = 0, bind_location = m_fragment_textures_start; i < 4; ++i)
			{
				builder << "layout(set=0, binding=" << bind_location++ << ") " << "uniform " << type_names[i] << " " << type_names[i] << "_array[16];\n";
			}

			builder << "\n"
					   "#define IS_TEXTURE_RESIDENT(index) true\n"
					   "#define SAMPLER1D(index) sampler1D_array[index]\n"
					   "#define SAMPLER2D(index) sampler2D_array[index]\n"
					   "#define SAMPLER3D(index) sampler3D_array[index]\n"
					   "#define SAMPLERCUBE(index) samplerCube_array[index]\n\n";
		}

		builder << "layout(std430, binding=" << m_fragment_instruction_start << ") readonly restrict buffer FragmentInstructionBlock\n"
																				"{\n"
																				"	uint shader_control;\n"
																				"	uint texture_control;\n"
																				"	uint reserved1;\n"
																				"	uint reserved2;\n"
																				"	uvec4 fp_instructions[];\n"
																				"};\n\n";

		builder << program_common::interpreter::get_fragment_interpreter();
		const std::string s = builder.str();

		auto fs = std::make_unique<glsl::shader>();
		fs->create(::glsl::program_domain::glsl_fragment_program, s);
		fs->compile();

		// Prepare input table
		vk::glsl::program_input in;
		in.location = binding_table.fragment_constant_buffers_bind_slot;
		in.domain = ::glsl::glsl_fragment_program;
		in.name = "FragmentConstantsBuffer";
		in.type = vk::glsl::input_type_uniform_buffer;
		m_fs_inputs.push_back(in);

		in.location = binding_table.fragment_state_bind_slot;
		in.name = "FragmentStateBuffer";
		m_fs_inputs.push_back(in);

		in.location = binding_table.fragment_texture_params_bind_slot;
		in.name = "TextureParametersBuffer";
		m_fs_inputs.push_back(in);

		for (int i = 0, location = m_fragment_textures_start; i < 4; ++i, ++location)
		{
			in.location = location;
			in.name = std::string(type_names[i]) + "_array[16]";
			m_fs_inputs.push_back(in);
		}

		auto ret = fs.get();
		m_shader_cache[compiler_options].m_fs = std::move(fs);
		return ret;
	}

	std::pair<VkDescriptorSetLayout, VkPipelineLayout> shader_interpreter::create_layout(VkDevice dev)
	{
		const auto& binding_table = vk::get_current_renderer()->get_pipeline_binding_table();
		auto bindings = get_common_binding_table();
		u32 idx = ::size32(bindings);

		bindings.resize(binding_table.total_descriptor_bindings);

		// Texture 1D array
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 16;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot;
		bindings[idx].pImmutableSamplers = nullptr;

		m_fragment_textures_start = bindings[idx].binding;
		idx++;

		// Texture 2D array
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 16;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot + 1;
		bindings[idx].pImmutableSamplers = nullptr;

		idx++;

		// Texture 3D array
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 16;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot + 2;
		bindings[idx].pImmutableSamplers = nullptr;

		idx++;

		// Texture CUBE array
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 16;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot + 3;
		bindings[idx].pImmutableSamplers = nullptr;

		idx++;

		// Vertex texture array (2D only)
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 4;
		bindings[idx].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot + 4;
		bindings[idx].pImmutableSamplers = nullptr;

		idx++;

		// Vertex program ucode block
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot + 5;
		bindings[idx].pImmutableSamplers = nullptr;

		m_vertex_instruction_start = bindings[idx].binding;
		idx++;

		// Fragment program ucode block
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].binding = binding_table.textures_first_bind_slot + 6;
		bindings[idx].pImmutableSamplers = nullptr;

		m_fragment_instruction_start = bindings[idx].binding;
		idx++;
		bindings.resize(idx);

		// Compile descriptor pool sizes
		const u32 num_ubo = bindings.reduce(0, FN(x + (y.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? y.descriptorCount : 0)));
		const u32 num_texel_buffers = bindings.reduce(0, FN(x + (y.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ? y.descriptorCount : 0)));
		const u32 num_combined_image_sampler = bindings.reduce(0, FN(x + (y.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ? y.descriptorCount : 0)));
		const u32 num_ssbo = bindings.reduce(0, FN(x + (y.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? y.descriptorCount : 0)));

		ensure(num_ubo > 0 && num_texel_buffers > 0 && num_combined_image_sampler > 0 && num_ssbo > 0);

		m_descriptor_pool_sizes =
			{
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_ubo},
				{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, num_texel_buffers},
				{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, num_combined_image_sampler},
				{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, num_ssbo}};

		std::array<VkPushConstantRange, 1> push_constants;
		push_constants[0].offset = 0;
		push_constants[0].size = 16;
		push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		if (vk::emulate_conditional_rendering())
		{
			// Conditional render toggle
			push_constants[0].size = 20;
		}

		const auto set_layout = vk::descriptors::create_layout(bindings);

		VkPipelineLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.setLayoutCount = 1;
		layout_info.pSetLayouts = &set_layout;
		layout_info.pushConstantRangeCount = 1;
		layout_info.pPushConstantRanges = push_constants.data();

		VkPipelineLayout result;
		CHECK_RESULT(VK_GET_SYMBOL(vkCreatePipelineLayout)(dev, &layout_info, nullptr, &result));
		return {set_layout, result};
	}

	void shader_interpreter::create_descriptor_pools(const vk::render_device& dev)
	{
		const auto max_draw_calls = dev.get_descriptor_max_draw_calls();
		m_descriptor_pool.create(dev, m_descriptor_pool_sizes, max_draw_calls);
	}

	void shader_interpreter::init(const vk::render_device& dev)
	{
		m_device = dev;
		std::tie(m_shared_descriptor_layout, m_shared_pipeline_layout) = create_layout(dev);
		create_descriptor_pools(dev);
	}

	void shader_interpreter::destroy()
	{
		m_current_interpreter.reset();
		m_program_cache.clear();
		m_descriptor_pool.destroy();

		for (auto& fs : m_shader_cache)
		{
			fs.second.m_vs->destroy();
			fs.second.m_fs->destroy();
		}

		m_shader_cache.clear();

		if (m_shared_pipeline_layout)
		{
			VK_GET_SYMBOL(vkDestroyPipelineLayout)(m_device, m_shared_pipeline_layout, nullptr);
			m_shared_pipeline_layout = VK_NULL_HANDLE;
		}

		if (m_shared_descriptor_layout)
		{
			VK_GET_SYMBOL(vkDestroyDescriptorSetLayout)(m_device, m_shared_descriptor_layout, nullptr);
			m_shared_descriptor_layout = VK_NULL_HANDLE;
		}
	}

	std::shared_ptr<glsl::program> shader_interpreter::link(const vk::pipeline_props& properties, u64 compiler_opt, bool async, std::function<void()> async_done)
	{
		glsl::shader *fs, *vs;
		if (auto found = m_shader_cache.find(compiler_opt); found != m_shader_cache.end())
		{
			fs = found->second.m_fs.get();
			vs = found->second.m_vs.get();
		}
		else
		{
			fs = build_fs(compiler_opt);
			vs = build_vs(compiler_opt);
		}

		if (async)
		{
			// Async path: rebuild the pipeline from props on a worker thread (module-based deferred overload).
			// The fs/vs modules are already built/cached above; only the VkPipeline assembly is deferred.
			VkShaderModule modules[2] = { vs->get_handle(), fs->get_handle() };

			pipeline_key key{};
			key.compiler_opt = compiler_opt;
			key.properties = properties;

			auto done = std::move(async_done);
			auto callback = [this, key, done](std::unique_ptr<glsl::program>& prog)
			{
				// Runs on the pipe-compiler worker thread.
				std::shared_ptr<glsl::program> result = std::move(prog);

				std::lock_guard lock(m_program_cache_lock);
				pipeline_cache_entry_t cache_entry;
				cache_entry.program = result;
				cache_entry.flags = 0;
				m_program_cache[key] = std::move(cache_entry);

				// Incremental compatible-variant seeding. As each base interpreter pipeline lands,
				// map any full variant that is only *compatible* with this base (not an exact match)
				// to a CACHED_PIPE_UNOPTIMIZED stand-in, so get() binds it immediately and async-
				// upgrades it on first use. This replaces preload()'s former synchronous drain+seed,
				// which parked the RSX thread (the smooth-shader hang). Naturally a no-op for exact
				// recompile callbacks: no full variant's compatible-opt equals a full variant's exact opt.
				const auto seed_variants = program_common::interpreter::get_interpreter_variants();
				for (const auto& seed_variant : seed_variants.pipelines)
				{
					pipeline_key full_key;
					full_key.properties = key.properties;
					full_key.compiler_opt = seed_variant.vs_opts.shader_opt | seed_variant.fs_opts.shader_opt;

					if (m_program_cache.count(full_key))
					{
						continue;
					}

					const u64 compat_opt = seed_variant.vs_opts.compatible_shader_opts | seed_variant.fs_opts.compatible_shader_opts;
					if (compat_opt == key.compiler_opt)
					{
						pipeline_cache_entry_t stand_in;
						stand_in.program = result;
						stand_in.flags = program_common::interpreter::CACHED_PIPE_UNOPTIMIZED;
						m_program_cache[full_key] = std::move(stand_in);
					}
				}

				if (done)
				{
					done();
				}
			};

			auto compiler = vk::get_pipe_compiler();
			compiler->compile(properties, modules, m_shared_pipeline_layout, vk::pipe_compiler::COMPILE_DEFERRED, std::move(callback), m_vs_inputs, m_fs_inputs);
			return nullptr;
		}

		VkPipelineShaderStageCreateInfo shader_stages[2] = {};
		shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_stages[0].module = vs->get_handle();
		shader_stages[0].pName = "main";

		shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_stages[1].module = fs->get_handle();
		shader_stages[1].pName = "main";

		std::vector<VkDynamicState> dynamic_state_descriptors =
			{
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_LINE_WIDTH,
				VK_DYNAMIC_STATE_BLEND_CONSTANTS,
				VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
				VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
				VK_DYNAMIC_STATE_STENCIL_REFERENCE,
				VK_DYNAMIC_STATE_DEPTH_BIAS};

		if (vk::get_current_renderer()->get_depth_bounds_support())
		{
			dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
		}

		VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.pDynamicStates = dynamic_state_descriptors.data();
		dynamic_state_info.dynamicStateCount = ::size32(dynamic_state_descriptors);

		VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

		VkPipelineViewportStateCreateInfo vp = {};
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.viewportCount = 1;
		vp.scissorCount = 1;

		VkPipelineMultisampleStateCreateInfo ms = properties.state.ms;
		ensure(ms.rasterizationSamples == VkSampleCountFlagBits((properties.renderpass_key >> 16) & 0xF)); // "Multisample state mismatch!"
		if (ms.rasterizationSamples != VK_SAMPLE_COUNT_1_BIT)
		{
			// Update the sample mask pointer
			ms.pSampleMask = &properties.state.temp_storage.msaa_sample_mask;
		}

		// Rebase pointers from pipeline structure in case it is moved/copied
		VkPipelineColorBlendStateCreateInfo cs = properties.state.cs;
		cs.pAttachments = properties.state.att_state;

		VkPipelineTessellationStateCreateInfo ts = {};
		ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.pVertexInputState = &vi;
		info.pInputAssemblyState = &properties.state.ia;
		info.pRasterizationState = &properties.state.rs;
		info.pColorBlendState = &cs;
		info.pMultisampleState = &ms;
		info.pViewportState = &vp;
		info.pDepthStencilState = &properties.state.ds;
		info.pTessellationState = &ts;
		info.stageCount = 2;
		info.pStages = shader_stages;
		info.pDynamicState = &dynamic_state_info;
		info.layout = m_shared_pipeline_layout;
		info.basePipelineIndex = -1;
		info.basePipelineHandle = VK_NULL_HANDLE;
		info.renderPass = vk::get_renderpass(m_device, properties.renderpass_key);

		auto compiler = vk::get_pipe_compiler();
		auto program = compiler->compile(info, m_shared_pipeline_layout, vk::pipe_compiler::COMPILE_INLINE, {}, m_vs_inputs, m_fs_inputs);
		return std::shared_ptr<glsl::program>(std::move(program));
	}

	void shader_interpreter::update_fragment_textures(const std::array<VkDescriptorImageInfo, 68>& sampled_images, vk::descriptor_set& set)
	{
		const VkDescriptorImageInfo* texture_ptr = sampled_images.data();
		for (u32 i = 0, binding = m_fragment_textures_start; i < 4; ++i, ++binding, texture_ptr += 16)
		{
			set.push(texture_ptr, 16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding);
		}
	}

	VkDescriptorSet shader_interpreter::allocate_descriptor_set()
	{
		return m_descriptor_pool.allocate(m_shared_descriptor_layout);
	}

	glsl::program* shader_interpreter::get(
		const vk::pipeline_props& properties,
		const program_hash_util::fragment_program_utils::fragment_program_metadata& metadata,
		u32 vp_ctrl,
		u32 fp_ctrl)
	{
		pipeline_key key;
		key.compiler_opt = 0;
		key.properties = properties;

		if (rsx::method_registers.alpha_test_enabled()) [[unlikely]]
		{
			switch (rsx::method_registers.alpha_func())
			{
			case rsx::comparison_function::always:
				break;
			case rsx::comparison_function::never:
				return nullptr;
			case rsx::comparison_function::greater_or_equal:
				key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_GE;
				break;
			case rsx::comparison_function::greater:
				key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_G;
				break;
			case rsx::comparison_function::less_or_equal:
				key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_LE;
				break;
			case rsx::comparison_function::less:
				key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_L;
				break;
			case rsx::comparison_function::equal:
				key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_EQ;
				break;
			case rsx::comparison_function::not_equal:
				key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_ALPHA_TEST_NE;
				break;
			}
		}

		if (fp_ctrl & CELL_GCM_SHADER_CONTROL_DEPTH_EXPORT)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_DEPTH_EXPORT;
		if (fp_ctrl & CELL_GCM_SHADER_CONTROL_32_BITS_EXPORTS)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_F32_EXPORT;
		if (fp_ctrl & RSX_SHADER_CONTROL_USES_KIL)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_KIL;
		if (metadata.referenced_textures_mask)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_TEXTURES;
		if (metadata.has_branch_instructions)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_FLOW_CTRL;
		if (metadata.has_pack_instructions)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_PACKING;
		if (rsx::method_registers.polygon_stipple_enabled())
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_STIPPLING;
		if (vp_ctrl & RSX_SHADER_CONTROL_INSTANCED_CONSTANTS)
			key.compiler_opt |= program_common::interpreter::COMPILER_OPT_ENABLE_INSTANCING;

		if (m_current_key == key) [[likely]]
		{
			return m_current_interpreter.get();
		}
		else
		{
			m_current_key = key;
		}

		// Key changed (rare relative to the fast-path above), so an exclusive lock here is cheap.
		// We may both read the cache and flip the RECOMPILING flag, so take the writer lock directly.
		{
			std::lock_guard lock(m_program_cache_lock);

			auto found = m_program_cache.find(key);
			if (found != m_program_cache.end()) [[likely]]
			{
				m_current_interpreter = found->second.program;

				// If this is a stand-in (unoptimized) variant and we haven't already kicked off the
				// exact build, fire an async link. The compatible program stays bound (no stall) until
				// the worker swaps the exact one into the cache.
				if ((found->second.flags & (program_common::interpreter::CACHED_PIPE_UNOPTIMIZED | program_common::interpreter::CACHED_PIPE_RECOMPILING)) == program_common::interpreter::CACHED_PIPE_UNOPTIMIZED)
				{
					found->second.flags |= program_common::interpreter::CACHED_PIPE_RECOMPILING;
					link(properties, key.compiler_opt, true, {});
				}

				return m_current_interpreter.get();
			}
		}

		// Hard miss: build inline (this stalls, but the preload + compatible-variant fallback should
		// make this rare in the interpreter shader modes).
		m_current_interpreter = link(properties, key.compiler_opt);

		{
			std::lock_guard lock(m_program_cache_lock);
			pipeline_cache_entry_t cache_entry;
			cache_entry.program = m_current_interpreter;
			cache_entry.flags = 0;
			m_program_cache[key] = std::move(cache_entry);
		}

		return m_current_interpreter.get();
	}

	bool shader_interpreter::is_interpreter(const glsl::program* prog) const
	{
		return prog == m_current_interpreter.get();
	}

	u32 shader_interpreter::get_vertex_instruction_location() const
	{
		return m_vertex_instruction_start;
	}

	u32 shader_interpreter::get_fragment_instruction_location() const
	{
		return m_fragment_instruction_start;
	}

	void shader_interpreter::preload()
	{
		// Precompile the base interpreter pipeline variants up-front (load-time, off the RSX hot path).
		// Runs headless - no shader_loading_dialog is driven here.
		std::vector<vk::pipeline_props> pipe_properties;
		auto pdev = vk::get_current_renderer();

		// Base pipeline - simple color
		vk::pipeline_props base_props{};
		base_props.state.set_attachment_count(1);
		base_props.state.enable_cull_face(VK_CULL_MODE_BACK_BIT);
		base_props.state.set_primitive_type(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		base_props.state.set_color_mask(0, true, true, true, true);
		base_props.state.set_attachment_count(1);
		base_props.state.enable_depth_bias(true);
		base_props.state.enable_depth_clamp(true);
		base_props.state.enable_depth_bounds_test(pdev->get_depth_bounds_support());
		base_props.renderpass_key = vk::get_renderpass_key(VK_FORMAT_B8G8R8A8_UNORM);
		pipe_properties.push_back(base_props);

		// Add in some blending
		base_props.state.enable_blend(0,
			VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_OP_ADD, VK_BLEND_OP_ADD);
		pipe_properties.push_back(base_props);

		// Add a depth buffer
		const auto depth_format = pdev->get_formats_support().d24_unorm_s8 ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
		base_props.renderpass_key = vk::get_renderpass_key(VK_FORMAT_B8G8R8A8_UNORM, depth_format);
		base_props.state.enable_depth_test(VK_COMPARE_OP_LESS);
		base_props.state.set_depth_mask(true);
		pipe_properties.push_back(base_props);

		// Fire-and-forget: queue the base interpreter pipelines for async compilation and return
		// immediately. We deliberately do NOT drain the queue here - blocking the RSX thread until
		// the bases finish is exactly what parked flip/present and hung "smooth shaders" before.
		// As each base lands on a pipe-compiler worker, its link() callback seeds the compatible
		// full-variant stand-ins. A draw that arrives before its base is ready falls back to a
		// single inline compile in get() (self-limiting, never a hang).
		const auto variants = program_common::interpreter::get_interpreter_variants();
		for (const auto& props : pipe_properties)
		{
			for (auto& variant : variants.base_pipelines)
			{
				link(props, variant.first | variant.second, true, {});
			}
		}
	}
}; // namespace vk
