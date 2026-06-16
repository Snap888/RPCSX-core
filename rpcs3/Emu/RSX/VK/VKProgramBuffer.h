#pragma once
#include "VKVertexProgram.h"
#include "VKFragmentProgram.h"
#include "VKPipelineCompiler.h"
#include "../Program/ProgramStateCache.h"

#ifdef ANDROID
#include <mutex>
#include <unordered_set>
#endif

namespace vk
{
	struct VKTraits
	{
		using vertex_program_type = VKVertexProgram;
		using fragment_program_type = VKFragmentProgram;
		using pipeline_type = vk::glsl::program;
		using pipeline_storage_type = std::unique_ptr<vk::glsl::program>;
		using pipeline_properties = vk::pipeline_props;

		static void recompile_fragment_program(const RSXFragmentProgram& RSXFP, fragment_program_type& fragmentProgramData, usz ID)
		{
			fragmentProgramData.Decompile(RSXFP);
			fragmentProgramData.id = static_cast<u32>(ID);
			fragmentProgramData.Compile();
		}

		static void recompile_vertex_program(const RSXVertexProgram& RSXVP, vertex_program_type& vertexProgramData, usz ID)
		{
			vertexProgramData.Decompile(RSXVP);
			vertexProgramData.id = static_cast<u32>(ID);
			vertexProgramData.Compile();
		}

		static void validate_pipeline_properties(const VKVertexProgram&, const VKFragmentProgram& fp, vk::pipeline_props& properties)
		{
			// Explicitly disable writing to undefined registers
			properties.state.att_state[0].colorWriteMask &= fp.output_color_masks[0];
			properties.state.att_state[1].colorWriteMask &= fp.output_color_masks[1];
			properties.state.att_state[2].colorWriteMask &= fp.output_color_masks[2];
			properties.state.att_state[3].colorWriteMask &= fp.output_color_masks[3];

#ifdef ANDROID
			// [seethru-diag] Localize the see-through-geometry bug. Three audits proved the
			// RSX->VK state, the pipeline-cache key, and the FP decompiler colour/alpha output are
			// all byte-identical to upstream 0.0.41 (which renders these surfaces opaque), so the
			// cause is runtime-data-dependent and only an on-device dump can pin it. Logged ONCE
			// per unique fragment program (dedup'd): validate_pipeline_properties runs on every
			// pipeline lookup, not just per build, so an unguarded log floods the file. For a
			// see-through draw this reveals whether it is (a) fully colour-masked (out_masks[0]==0
			// -> the decompiler dropped the colour write), (b) wrongly blend-enabled (blend=1 -> a
			// fixed-function blend leak), or (c) neither (then the fragment is discarded at runtime
			// by alpha-test/alpha-kill and the next probe is rop_control/alpha_ref). Temporary -
			// remove once the bug is localized.
			{
				static std::mutex s_seethru_mtx;
				static std::unordered_set<u32> s_seethru_seen;
				bool first_seen;
				{
					std::lock_guard lock(s_seethru_mtx);
					first_seen = s_seethru_seen.insert(fp.id).second;
				}
				if (first_seen)
				{
					const auto& a0 = properties.state.att_state[0];
					rsx_log.warning("[seethru] fp_id=%u out_masks=[%#x,%#x,%#x,%#x] att0{blend=%u colorWriteMask=%#x srcC=%u dstC=%u srcA=%u dstA=%u}",
						fp.id,
						fp.output_color_masks[0], fp.output_color_masks[1], fp.output_color_masks[2], fp.output_color_masks[3],
						static_cast<u32>(a0.blendEnable), static_cast<u32>(a0.colorWriteMask),
						static_cast<u32>(a0.srcColorBlendFactor), static_cast<u32>(a0.dstColorBlendFactor),
						static_cast<u32>(a0.srcAlphaBlendFactor), static_cast<u32>(a0.dstAlphaBlendFactor));
				}
			}
#endif
		}

		static pipeline_type* build_pipeline(
			const vertex_program_type& vertexProgramData,
			const fragment_program_type& fragmentProgramData,
			const vk::pipeline_props& pipelineProperties,
			bool compile_async,
			std::function<pipeline_type*(pipeline_storage_type&)> callback,
			VkPipelineLayout common_pipeline_layout)
		{
			const auto compiler_flags = compile_async ? vk::pipe_compiler::COMPILE_DEFERRED : vk::pipe_compiler::COMPILE_INLINE;
			VkShaderModule modules[2] = {vertexProgramData.handle, fragmentProgramData.handle};

			auto compiler = vk::get_pipe_compiler();
			auto result = compiler->compile(
				pipelineProperties, modules, common_pipeline_layout,
				compiler_flags, callback,
				vertexProgramData.uniforms,
				fragmentProgramData.uniforms);

			return callback(result);
		}
	};

	struct program_cache : public program_state_cache<VKTraits>
	{
		program_cache(decompiler_callback_t callback)
		{
			notify_pipeline_compiled = callback;
		}

		u64 get_hash(const vk::pipeline_props& props)
		{
			return rpcs3::hash_struct<vk::pipeline_props>(props);
		}

		u64 get_hash(const RSXVertexProgram& prog)
		{
			return program_hash_util::vertex_program_utils::get_vertex_program_ucode_hash(prog);
		}

		u64 get_hash(const RSXFragmentProgram& prog)
		{
			return program_hash_util::fragment_program_utils::get_fragment_program_ucode_hash(prog);
		}

		template <typename... Args>
		void add_pipeline_entry(RSXVertexProgram& vp, RSXFragmentProgram& fp, vk::pipeline_props& props, Args&&... args)
		{
			get_graphics_pipeline(nullptr, vp, fp, props, false, false, std::forward<Args>(args)...);
		}

		void preload_programs(rsx::program_cache_hint_t* cache_hint, const RSXVertexProgram& vp, const RSXFragmentProgram& fp)
		{
			search_vertex_program(cache_hint, vp);
			search_fragment_program(cache_hint, fp);
		}

		bool check_cache_missed() const
		{
			return m_cache_miss_flag;
		}
	};
} // namespace vk
