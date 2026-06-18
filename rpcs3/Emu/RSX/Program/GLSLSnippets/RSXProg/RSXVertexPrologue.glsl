R"(
#ifdef _FORCE_POSITION_INVARIANCE
// PS3 has shader invariance, but we don't really care about most attributes outside ATTR0
invariant gl_Position;
#endif

#ifdef _EMULATE_ZCLIP_XFORM_STANDARD
// Technically the depth value here is the 'final' depth that should be stored in the Z buffer.
// Forward mapping eqn is d' = d * (f - n) + n, where d' is the stored Z value (this) and d is the normalized API value.
vec4 apply_zclip_xform(
	const in vec4 pos,
	const in float near_plane,
	const in float far_plane)
{
	if (pos.w != 0.0)
	{
		const float real_n = min(far_plane, near_plane);
		const float real_f = max(far_plane, near_plane);
		const double depth_range = double(real_f - real_n);
		const double inv_range = (depth_range > 0.000001) ? (1.0 / (depth_range * pos.w)) : 0.0;
		const double actual_d = (double(pos.z) - double(real_n * pos.w)) * inv_range;
		const double nearest_d = floor(actual_d + 0.5);
		const double epsilon = (inv_range * pos.w) / 16777215.;     // Epsilon value is the minimum discernable change in Z that should affect the stored Z
		const double d = _select(actual_d, nearest_d, abs(actual_d - nearest_d) < epsilon);
		return vec4(pos.xy, float(d * pos.w), pos.w);
	}
	else
	{
		return pos; // Only values where Z=0 can ever pass this clip
	}
}
#elif defined(_EMULATE_ZCLIP_XFORM_FALLBACK)
// Path taken by GPUs without fp64 (e.g. Adreno). The previous heuristic passed through
// in-[0,1] depths unchanged and only crudely handled overshoot - clamping near-plane
// geometry to 0 and compressing far-plane geometry into [0.99, 1.0]. That assumes the API
// depth range is already [0,1] and skips the actual forward remap, so for a light projection
// with a non-trivial [near, far] range the depth extremes are mangled: the head (near) is
// flattened and the legs (far) are squashed to ~1.0 and cast no shadow, while the mid-depth
// torso passes through and survives. Desktop avoids this because the fp64 STANDARD path does
// the real linear remap. Mirror that remap here in fp32 (forward eqn d' = d * (f - n) + n,
// inverted to normalize [near, far] -> [0, 1]); computing in normalized depth (pos.z / pos.w)
// keeps fp32 precision acceptable and reproduces the full depth range so every vertex lands
// at its correct depth.
vec4 apply_zclip_xform(
	const in vec4 pos,
	const in float near_plane,
	const in float far_plane)
{
	if (pos.w != 0.0)
	{
		const float real_n = min(far_plane, near_plane);
		const float real_f = max(far_plane, near_plane);
		const float depth_range = real_f - real_n;
		const float d_ndc = pos.z / pos.w;
		const float actual_d = (depth_range > 0.000001) ? ((d_ndc - real_n) / depth_range) : d_ndc;
		const float nearest_d = floor(actual_d + 0.5);
		const float epsilon = 1.0 / (max(depth_range, 0.000001) * 16777215.);   // Minimum discernable change in Z
		const float d = (abs(actual_d - nearest_d) < epsilon) ? nearest_d : actual_d;
		return vec4(pos.xy, d * pos.w, pos.w);
	}
	else
	{
		return pos; // Only values where Z=0 can ever pass this clip
	}
}
#endif

#if defined(_ENABLE_INSTANCED_CONSTANTS)
// Workaround for GL vs VK builtin variable naming
#ifdef VULKAN
#define _gl_InstanceID gl_InstanceIndex
#else
#define _gl_InstanceID gl_InstanceID
#endif

vec4 _fetch_constant(const in int base_offset)
{
	// Get virtual draw/instance id. Normally will be 1:1 based on instance index
	const int indirection_offset = (_gl_InstanceID * CONSTANTS_ARRAY_LENGTH) + base_offset;
	const int corrected_offset = constants_addressing_lookup[indirection_offset];
	return instanced_constants_array[corrected_offset];
}

vec4 _fetch_constant(const in uint base_offset)
{
	// uint override
	return _fetch_constant(int(base_offset));
}
#else
#define _fetch_constant(x) vc[x]
#endif

)"
