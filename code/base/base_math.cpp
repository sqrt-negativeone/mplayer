
  /*
* angle unit conversion functions
*/
internal inline f32 to_rad(f32 angle)
{
	#if defined(HANDMADE_MATH_USE_RADIANS)
		f32 result = angle;
	#elif defined(HANDMADE_MATH_USE_DEGREES)
		f32 result = angle * DegToRad;
	#elif defined(HANDMADE_MATH_USE_TURNS)
		f32 result = angle * TurnToRad;
	#endif
		
		return result;
}

internal inline f32 to_deg(f32 angle)
{
	#if defined(HANDMADE_MATH_USE_RADIANS)
		f32 result = angle * RadToDeg;
	#elif defined(HANDMADE_MATH_USE_DEGREES)
		f32 result = angle;
	#elif defined(HANDMADE_MATH_USE_TURNS)
		f32 result = angle * TurnToDeg;
	#endif
		
		return result;
}

internal inline f32 to_turn(f32 angle)
{
	#if defined(HANDMADE_MATH_USE_RADIANS)
		f32 result = angle * RadToTurn;
	#elif defined(HANDMADE_MATH_USE_DEGREES)
		f32 result = angle * DegToTurn;
	#elif defined(HANDMADE_MATH_USE_TURNS)
		f32 result = angle;
	#endif
		
		return result;
}

/*
 * floating-point math functions
 */

internal inline f32 sin_f(f32 angle)
{
  return SINF(ANGLE_USER_TO_INTERNAL(angle));
}

internal inline f32 cos_f(f32 angle)
{
  return COSF(ANGLE_USER_TO_INTERNAL(angle));
}

internal inline f32 tan_f(f32 angle)
{
  return TANF(ANGLE_USER_TO_INTERNAL(angle));
}

internal inline f32 acos_f(f32 arg)
{
  return ANGLE_INTERNAL_TO_USER(ACOSF(arg));
}

internal inline f32 pow_f(f32 base, f32 exp)
{
  f32 result = powf(base, exp);
	return result;
}

internal inline f32 sqrt_f(f32 f)
{
  f32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 In = _mm_set_ss(f);
  __m128 Out = _mm_sqrt_ss(In);
  result = _mm_cvtss_f32(Out);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t In = vdupq_n_f32(f);
  f3232x4_t Out = vsqrtq_f32(In);
  result = vgetq_lane_f32(Out, 0);
	#else
		result = SQRTF(f);
	#endif
		
		return result;
}

internal inline f32 inv_sqrt_f(f32 f)
{
  f32 result;
  result = f? 1.0f/sqrt_f(f):0;
  return result;
}


/*
 * Utility functions
 */

internal inline f32
lerpf(f32 a, f32 t, f32 b)
{
  return (1.0f - t) * a + t * b;
}

internal inline f32
clamp(f32 min, f32 v, f32 max)
{
  f32 result = v;
  
  if (result < min)
  {
    result = min;
  }
  
  if (result > max)
  {
    result = max;
  }
  
  return result;
}

internal inline f32 map_into_range01(f32 min, f32 value, f32 max)
{
	f32 result = 0;
	value = CLAMP(min, value, max);
  f32 width = (max - min);
  if (width != 0)
  {
    result = (value - min) / width;
  }
	return result;
}

internal inline f64
map_into_range01_f64(f64 min, f64 value, f64 max)
{
  f64 result = 0;
	value = CLAMP(min, value, max);
  f64 width = (max - min);
  if (width != 0)
  {
    result = (value - min) / width;
  }
  return result;
}

internal inline f32
map_into_range_no(f32 min, f32 v, f32 max)
{
  f32 result = -1.0f + 2.0f * map_into_range01(min, v, max);
  return result;
}

internal inline f32
map_into_range(f32 value, f32 old_min, f32 old_max, f32 new_min, f32 new_max)
{
	value = CLAMP(old_min, value, old_max);
	f32 result = new_min + map_into_range01(old_min, value, old_max) * (new_max - new_min);
	return result;
}

internal inline i32 round_f32_i32(f32 v)
{
  i32 Result = _mm_cvtss_si32(_mm_set_ss(v));
  return(Result);
}

internal inline u32 round_f32_u32(f32 v)
{
  u32 Result = (u32)_mm_cvtss_si32(_mm_set_ss(v));
  return(Result);
}

internal inline i32
trancuate_f32_i32(f32 v)
{
  i32 Result = (i32)v;
  return(Result);
}

// returns a region where to draw, the region has the same aspect ratio as
// render_dim, and is centered inside the window_dim
internal Range2_I32
compute_draw_region_aspect_ratio_fit(V2_I32 render_dim, V2_I32 window_dim)
{
	Range2_I32 result = ZERO_STRUCT;
  
	f32 render_aspect_ratio = (f32)render_dim.width / (f32)render_dim.height;
	f32 optimal_width  = (f32)window_dim.height * render_aspect_ratio;
	f32 optimal_height = (f32)window_dim.width * (1.0f / render_aspect_ratio);
  
	if (optimal_width > (f32)window_dim.width)
	{
		result.minp.x = 0;
		result.maxp.x = window_dim.width;
		
		i32 half_empty = round_f32_i32(0.5f * ((f32)window_dim.height - optimal_height));
		i32 use_height = round_f32_i32(optimal_height);
		
		result.minp.y = half_empty;
		result.maxp.y = half_empty + use_height;
	}
	else
	{
		result.minp.y = 0;
		result.maxp.y = window_dim.height;
		
		i32 half_empty = round_f32_i32(0.5f * ((f32)window_dim.width - optimal_width));
		i32 use_width  = round_f32_i32(optimal_width);
		
		result.minp.x = half_empty;
		result.maxp.x = half_empty + use_width;
	}
  
	return result;
}

/*
 * Vector initialization
 */

internal inline V2_F32 
v2(f32 X, f32 Y)
{
  
  V2_F32 result;
  result.x = X;
  result.y = Y;
  
  return result;
}

internal inline V2_F32
v2d(f32 v)
{
	V2_F32 result = v2(v, v);
	return result;
}

internal inline V2_F32
v2vi(V2_I32 v)
{
	V2_F32 result = v2((f32)(v.x), (f32)(v.y));
	return result;
}


internal inline V2_I32 
v2i(i32 X, i32 Y)
{
  
  V2_I32 result;
  result.x = X;
  result.y = Y;
  
  return result;
}
internal inline V2_I32
v2id(V2_F32 v)
{
	V2_I32 result = v2i((i32)(v.x), (i32)(v.y));
	return result;
}

internal inline V3_F32
v3(f32 x, f32 y, f32 z)
{
	V3_F32 result;
	result.x = x;
	result.y = y;
	result.z = z;
	return result;
}

internal inline V3_F32
v3d(f32 v)
{
	V3_F32 result = v3(v,v,v);
	return result;
}

internal inline V3_F32
v3v(V2_F32 xy, f32 z)
{
	V3_F32 result;
	result.xy = xy;
	result.z = z;
	return result;
}

internal inline V4_F32 
v4(f32 X, f32 Y, f32 Z, f32 W)
{
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_setr_ps(X, Y, Z, W);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t v = {X, Y, Z, W};
  result.NEON = v;
	#else
		result.x = X;
  result.y = Y;
  result.z = Z;
  result.w = W;
	#endif
		
		return result;
}

internal inline V4_F32 
v4v(V3_F32 v, f32 w)
{
  V4_F32 result = v4(v.x, v.y, v.z, w);
	return result;
}

internal inline V4_F32 
v4d(f32 d)
{
  V4_F32 result = v4(d, d, d, d);
	return result;
}

/*
 * Binary vector operations
 */

internal inline V2_F32 add_v2(V2_F32 lhs, V2_F32 rhs)
{
  
  V2_F32 result;
  result.x = lhs.x + rhs.x;
  result.y = lhs.y + rhs.y;
  
  return result;
}

internal inline V3_F32 add_v3(V3_F32 lhs, V3_F32 rhs)
{
  
  V3_F32 result;
  result.x = lhs.x + rhs.x;
  result.y = lhs.y + rhs.y;
  result.z = lhs.z + rhs.z;
  
  return result;
}

internal inline V4_F32 add_v4(V4_F32 lhs, V4_F32 rhs)
{
  
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_add_ps(lhs.SSE, rhs.SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vaddq_f32(lhs.NEON, rhs.NEON);
	#else
		result.x = lhs.x + rhs.x;
  result.y = lhs.y + rhs.y;
  result.z = lhs.z + rhs.z;
  result.w = lhs.w + rhs.w;
	#endif
		
		return result;
}

internal inline V2_F32 sub_v2(V2_F32 lhs, V2_F32 rhs)
{
  
  V2_F32 result;
  result.x = lhs.x - rhs.x;
  result.y = lhs.y - rhs.y;
  
  return result;
}

internal inline V3_F32 sub_v3(V3_F32 lhs, V3_F32 rhs)
{
  
  V3_F32 result;
  result.x = lhs.x - rhs.x;
  result.y = lhs.y - rhs.y;
  result.z = lhs.z - rhs.z;
  
  return result;
}

internal inline V4_F32 sub_v4(V4_F32 lhs, V4_F32 rhs)
{
  
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_sub_ps(lhs.SSE, rhs.SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vsubq_f32(lhs.NEON, rhs.NEON);
	#else
		result.x = lhs.x - rhs.x;
  result.y = lhs.y - rhs.y;
  result.z = lhs.z - rhs.z;
  result.w = lhs.w - rhs.w;
	#endif
		
		return result;
}

internal inline V2_F32 mul_v2(V2_F32 lhs, V2_F32 rhs)
{
  V2_F32 result;
  result.x = lhs.x * rhs.x;
  result.y = lhs.y * rhs.y;
  return result;
}

internal inline V2_F32 mul_v2f(V2_F32 lhs, f32 rhs)
{
  V2_F32 result;
  result.x = lhs.x * rhs;
  result.y = lhs.y * rhs;
  return result;
}

internal inline V3_F32 mul_v3(V3_F32 lhs, V3_F32 rhs)
{
  
  V3_F32 result;
  result.x = lhs.x * rhs.x;
  result.y = lhs.y * rhs.y;
  result.z = lhs.z * rhs.z;
  
  return result;
}

internal inline V3_F32 mul_v3f(V3_F32 lhs, f32 rhs)
{
  
  V3_F32 result;
  result.x = lhs.x * rhs;
  result.y = lhs.y * rhs;
  result.z = lhs.z * rhs;
  
  return result;
}

internal inline V4_F32 mul_v4(V4_F32 lhs, V4_F32 rhs)
{
  
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_mul_ps(lhs.SSE, rhs.SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vmulq_f32(lhs.NEON, rhs.NEON);
	#else
		result.x = lhs.x * rhs.x;
  result.y = lhs.y * rhs.y;
  result.z = lhs.z * rhs.z;
  result.w = lhs.w * rhs.w;
	#endif
		
		return result;
}

internal inline V4_F32 mul_v4f(V4_F32 lhs, f32 rhs)
{
  
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 Scalar = _mm_set1_ps(rhs);
  result.SSE = _mm_mul_ps(lhs.SSE, Scalar);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vmulq_n_f32(lhs.NEON, rhs);
	#else
		result.x = lhs.x * rhs;
  result.y = lhs.y * rhs;
  result.z = lhs.z * rhs;
  result.w = lhs.w * rhs;
	#endif
		
		return result;
}

internal inline V2_F32 div_v2(V2_F32 lhs, V2_F32 rhs)
{
  
  V2_F32 result;
  result.x = lhs.x / rhs.x;
  result.y = lhs.y / rhs.y;
  
  return result;
}

internal inline V2_F32 div_v2f(V2_F32 lhs, f32 rhs)
{
  
  V2_F32 result;
  result.x = lhs.x / rhs;
  result.y = lhs.y / rhs;
  
  return result;
}

internal inline V3_F32 div_v3(V3_F32 lhs, V3_F32 rhs)
{
  
  V3_F32 result;
  result.x = lhs.x / rhs.x;
  result.y = lhs.y / rhs.y;
  result.z = lhs.z / rhs.z;
  
  return result;
}

internal inline V3_F32 div_v3f(V3_F32 lhs, f32 rhs)
{
  
  V3_F32 result;
  result.x = lhs.x / rhs;
  result.y = lhs.y / rhs;
  result.z = lhs.z / rhs;
  
  return result;
}

internal inline V4_F32 div_v4(V4_F32 lhs, V4_F32 rhs)
{
  
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_div_ps(lhs.SSE, rhs.SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vdivq_f32(lhs.NEON, rhs.NEON);
	#else
		result.x = lhs.x / rhs.x;
  result.y = lhs.y / rhs.y;
  result.z = lhs.z / rhs.z;
  result.w = lhs.w / rhs.w;
	#endif
		
		return result;
}

internal inline V4_F32 div_v4f(V4_F32 lhs, f32 rhs)
{
  
  V4_F32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 Scalar = _mm_set1_ps(rhs);
  result.SSE = _mm_div_ps(lhs.SSE, Scalar);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t Scalar = vdupq_n_f32(rhs);
  result.NEON = vdivq_f32(lhs.NEON, Scalar);
	#else
		result.x = lhs.x / rhs;
  result.y = lhs.y / rhs;
  result.z = lhs.z / rhs;
  result.w = lhs.w / rhs;
	#endif
		
		return result;
}

internal inline b32 eq_v2(V2_F32 lhs, V2_F32 rhs)
{
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

internal inline b32 eq_v3(V3_F32 lhs, V3_F32 rhs)
{
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

internal inline b32 eq_v4(V4_F32 lhs, V4_F32 rhs)
{
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

internal inline f32 dot_v2(V2_F32 lhs, V2_F32 rhs)
{
  return (lhs.x * rhs.x) + (lhs.y * rhs.y);
}

internal inline f32 dot_v3(V3_F32 lhs, V3_F32 rhs)
{
  return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

internal inline f32 dot_v4(V4_F32 lhs, V4_F32 rhs)
{
  
  f32 result;
  
  // NOTE(zak): IN the future if we wanna check what version SSE is support
  // we can use _mm_dp_ps (4.3) but for now we will use the old way.
  // Or a r = _mm_mul_ps(v1, v2), r = _mm_hadd_ps(r, r), r = _mm_hadd_ps(r, r) for SSE3
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSEresultOne = _mm_mul_ps(lhs.SSE, rhs.SSE);
  __m128 SSEresultTwo = _mm_shuffle_ps(SSEresultOne, SSEresultOne, _MM_SHUFFLE(2, 3, 0, 1));
  SSEresultOne = _mm_add_ps(SSEresultOne, SSEresultTwo);
  SSEresultTwo = _mm_shuffle_ps(SSEresultOne, SSEresultOne, _MM_SHUFFLE(0, 1, 2, 3));
  SSEresultOne = _mm_add_ps(SSEresultOne, SSEresultTwo);
  _mm_store_ss(&result, SSEresultOne);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t NEONmultiplyresult = vmulq_f32(lhs.NEON, rhs.NEON);
  f3232x4_t NEONHalfadd = vpaddq_f32(NEONmultiplyresult, NEONmultiplyresult);
  f3232x4_t NEONFulladd = vpaddq_f32(NEONHalfadd, NEONHalfadd);
  result = vgetq_lane_f32(NEONFulladd, 0);
	#else
		result = ((lhs.x * rhs.x) + (lhs.z * rhs.z)) + ((lhs.y * rhs.y) + (lhs.w * rhs.w));
	#endif
		
		return result;
}

internal inline V3_F32 cross_v3(V3_F32 lhs, V3_F32 rhs)
{
  
  V3_F32 result;
  result.x = (lhs.y * rhs.z) - (lhs.z * rhs.y);
  result.y = (lhs.z * rhs.x) - (lhs.x * rhs.z);
  result.z = (lhs.x * rhs.y) - (lhs.y * rhs.x);
  
  return result;
}


/*
 * Unary vector operations
 */

internal inline f32 len_sqr_v2(V2_F32 A)
{
  return dot_v2(A, A);
}

internal inline f32 len_sqr_v3(V3_F32 A)
{
  return dot_v3(A, A);
}

internal inline f32 len_sqr_v4(V4_F32 A)
{
  return dot_v4(A, A);
}

internal inline f32 len_v2(V2_F32 A)
{
  return sqrt_f(len_sqr_v2(A));
}

internal inline f32 len_v3(V3_F32 A)
{
  return sqrt_f(len_sqr_v3(A));
}

internal inline f32 len_v4(V4_F32 A)
{
  return sqrt_f(len_sqr_v4(A));
}

internal inline V2_F32 v2_normalize(V2_F32 A)
{
  return mul_v2f(A, inv_sqrt_f(dot_v2(A, A)));
}

internal inline V3_F32 v3_normalize(V3_F32 A)
{
  return mul_v3f(A, inv_sqrt_f(dot_v3(A, A)));
}

internal inline V3_F32
compute_plane_normal(V3_F32 v1, V3_F32 v2, V3_F32 v3)
{
	V3_F32 n = ZERO_STRUCT;
	not_impemeneted();
	return n;
}

internal inline V4_F32 v4_normalize(V4_F32 A)
{
  return mul_v4f(A, inv_sqrt_f(dot_v4(A, A)));
}

/*
 * Utility vector functions
 */

internal inline V2_F32 lerp_v2(V2_F32 A, f32 t, V2_F32 B)
{
  return add_v2(mul_v2f(A, 1.0f - t), mul_v2f(B, t));
}

internal inline V3_F32 lerp_v3(V3_F32 A, f32 t, V3_F32 B)
{
  return add_v3(mul_v3f(A, 1.0f - t), mul_v3f(B, t));
}

internal inline V4_F32 lerp_v4(V4_F32 A, f32 t, V4_F32 B)
{
  return add_v4(mul_v4f(A, 1.0f - t), mul_v4f(B, t));
}

/*
 * SSE stuff
 */

internal inline V4_F32 linear_combine_v4m4(V4_F32 lhs, M4 rhs)
{
  
  V4_F32 result;
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0x00), rhs.columns[0].SSE);
  result.SSE = _mm_add_ps(result.SSE, _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0x55), rhs.columns[1].SSE));
  result.SSE = _mm_add_ps(result.SSE, _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0xaa), rhs.columns[2].SSE));
  result.SSE = _mm_add_ps(result.SSE, _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0xff), rhs.columns[3].SSE));
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vmulq_laneq_f32(rhs.columns[0].NEON, lhs.NEON, 0);
  result.NEON = vfmaq_laneq_f32(result.NEON, rhs.columns[1].NEON, lhs.NEON, 1);
  result.NEON = vfmaq_laneq_f32(result.NEON, rhs.columns[2].NEON, lhs.NEON, 2);
  result.NEON = vfmaq_laneq_f32(result.NEON, rhs.columns[3].NEON, lhs.NEON, 3);
	#else
		result.x = lhs.elements[0] * rhs.columns[0].x;
  result.y = lhs.elements[0] * rhs.columns[0].y;
  result.z = lhs.elements[0] * rhs.columns[0].z;
  result.w = lhs.elements[0] * rhs.columns[0].w;
  
  result.x += lhs.elements[1] * rhs.columns[1].x;
  result.y += lhs.elements[1] * rhs.columns[1].y;
  result.z += lhs.elements[1] * rhs.columns[1].z;
  result.w += lhs.elements[1] * rhs.columns[1].w;
  
  result.x += lhs.elements[2] * rhs.columns[2].x;
  result.y += lhs.elements[2] * rhs.columns[2].y;
  result.z += lhs.elements[2] * rhs.columns[2].z;
  result.w += lhs.elements[2] * rhs.columns[2].w;
  
  result.x += lhs.elements[3] * rhs.columns[3].x;
  result.y += lhs.elements[3] * rhs.columns[3].y;
  result.z += lhs.elements[3] * rhs.columns[3].z;
  result.w += lhs.elements[3] * rhs.columns[3].w;
	#endif
		
		return result;
}

/*
 * 2x2 Mrices
 */

internal inline M2 m2(void)
{
  M2 result = {0};
  return result;
}

internal inline M2 m2d(f32 Diagonal)
{
  
  M2 result = {0};
  result.elements[0][0] = Diagonal;
  result.elements[1][1] = Diagonal;
  
  return result;
}

internal inline M2 transpose_m2(M2 matrix)
{
  
  M2 result = matrix;
  
  result.elements[0][1] = matrix.elements[1][0];
  result.elements[1][0] = matrix.elements[0][1];
  
  return result;
}

internal inline M2 add_m2(M2 lhs, M2 rhs)
{
  
  M2 result;
  
  result.elements[0][0] = lhs.elements[0][0] + rhs.elements[0][0];
  result.elements[0][1] = lhs.elements[0][1] + rhs.elements[0][1];
  result.elements[1][0] = lhs.elements[1][0] + rhs.elements[1][0];
  result.elements[1][1] = lhs.elements[1][1] + rhs.elements[1][1];
  
  return result;
}

internal inline M2 sub_m2(M2 lhs, M2 rhs)
{
  
  M2 result;
  
  result.elements[0][0] = lhs.elements[0][0] - rhs.elements[0][0];
  result.elements[0][1] = lhs.elements[0][1] - rhs.elements[0][1];
  result.elements[1][0] = lhs.elements[1][0] - rhs.elements[1][0];
  result.elements[1][1] = lhs.elements[1][1] - rhs.elements[1][1];
  
  return result;
}

internal inline V2_F32 mul_m2v2(M2 matrix, V2_F32 Vector)
{
  
  V2_F32 result;
  
  result.x = Vector.elements[0] * matrix.columns[0].x;
  result.y = Vector.elements[0] * matrix.columns[0].y;
  
  result.x += Vector.elements[1] * matrix.columns[1].x;
  result.y += Vector.elements[1] * matrix.columns[1].y;
  
  return result;
}

internal inline M2 mul_m2(M2 lhs, M2 rhs)
{
  
  M2 result;
  result.columns[0] = mul_m2v2(lhs, rhs.columns[0]);
  result.columns[1] = mul_m2v2(lhs, rhs.columns[1]);
  
  return result;
}

internal inline M2 mul_m2f(M2 matrix, f32 Scalar)
{
  
  M2 result;
  
  result.elements[0][0] = matrix.elements[0][0] * Scalar;
  result.elements[0][1] = matrix.elements[0][1] * Scalar;
  result.elements[1][0] = matrix.elements[1][0] * Scalar;
  result.elements[1][1] = matrix.elements[1][1] * Scalar;
  
  return result;
}

internal inline M2 div_m2f(M2 matrix, f32 Scalar)
{
  
  M2 result;
  
  result.elements[0][0] = matrix.elements[0][0] / Scalar;
  result.elements[0][1] = matrix.elements[0][1] / Scalar;
  result.elements[1][0] = matrix.elements[1][0] / Scalar;
  result.elements[1][1] = matrix.elements[1][1] / Scalar;
  
  return result;
}

internal inline f32 determinant_m2(M2 matrix)
{
  return matrix.elements[0][0]*matrix.elements[1][1] - matrix.elements[0][1]*matrix.elements[1][0];
}


internal inline M2 inv_general_m2(M2 matrix)
{
  
  M2 result;
  f32 Invdeterminant = 1.0f / determinant_m2(matrix);
  result.elements[0][0] = Invdeterminant * +matrix.elements[1][1];
  result.elements[1][1] = Invdeterminant * +matrix.elements[0][0];
  result.elements[0][1] = Invdeterminant * -matrix.elements[0][1];
  result.elements[1][0] = Invdeterminant * -matrix.elements[1][0];
  
  return result;
}

/*
 * 3x3 Mrices
 */

internal inline M3 m3(void)
{
  M3 result = {0};
  return result;
}

internal inline M3 m3d(f32 Diagonal)
{
  
  M3 result = {0};
  result.elements[0][0] = Diagonal;
  result.elements[1][1] = Diagonal;
  result.elements[2][2] = Diagonal;
  
  return result;
}

internal inline M3 transpose_m3(M3 matrix)
{
  
  M3 result = matrix;
  
  result.elements[0][1] = matrix.elements[1][0];
  result.elements[0][2] = matrix.elements[2][0];
  result.elements[1][0] = matrix.elements[0][1];
  result.elements[1][2] = matrix.elements[2][1];
  result.elements[2][1] = matrix.elements[1][2];
  result.elements[2][0] = matrix.elements[0][2];
  
  return result;
}

internal inline M3 add_m3(M3 lhs, M3 rhs)
{
  
  M3 result;
  
  result.elements[0][0] = lhs.elements[0][0] + rhs.elements[0][0];
  result.elements[0][1] = lhs.elements[0][1] + rhs.elements[0][1];
  result.elements[0][2] = lhs.elements[0][2] + rhs.elements[0][2];
  result.elements[1][0] = lhs.elements[1][0] + rhs.elements[1][0];
  result.elements[1][1] = lhs.elements[1][1] + rhs.elements[1][1];
  result.elements[1][2] = lhs.elements[1][2] + rhs.elements[1][2];
  result.elements[2][0] = lhs.elements[2][0] + rhs.elements[2][0];
  result.elements[2][1] = lhs.elements[2][1] + rhs.elements[2][1];
  result.elements[2][2] = lhs.elements[2][2] + rhs.elements[2][2];
  
  return result;
}

internal inline M3 sub_m3(M3 lhs, M3 rhs)
{
  
  M3 result;
  
  result.elements[0][0] = lhs.elements[0][0] - rhs.elements[0][0];
  result.elements[0][1] = lhs.elements[0][1] - rhs.elements[0][1];
  result.elements[0][2] = lhs.elements[0][2] - rhs.elements[0][2];
  result.elements[1][0] = lhs.elements[1][0] - rhs.elements[1][0];
  result.elements[1][1] = lhs.elements[1][1] - rhs.elements[1][1];
  result.elements[1][2] = lhs.elements[1][2] - rhs.elements[1][2];
  result.elements[2][0] = lhs.elements[2][0] - rhs.elements[2][0];
  result.elements[2][1] = lhs.elements[2][1] - rhs.elements[2][1];
  result.elements[2][2] = lhs.elements[2][2] - rhs.elements[2][2];
  
  return result;
}

internal inline V3_F32 mul_m3v3(M3 matrix, V3_F32 Vector)
{
  
  V3_F32 result;
  
  result.x = Vector.elements[0] * matrix.columns[0].x;
  result.y = Vector.elements[0] * matrix.columns[0].y;
  result.z = Vector.elements[0] * matrix.columns[0].z;
  
  result.x += Vector.elements[1] * matrix.columns[1].x;
  result.y += Vector.elements[1] * matrix.columns[1].y;
  result.z += Vector.elements[1] * matrix.columns[1].z;
  
  result.x += Vector.elements[2] * matrix.columns[2].x;
  result.y += Vector.elements[2] * matrix.columns[2].y;
  result.z += Vector.elements[2] * matrix.columns[2].z;
  
  return result;
}

internal inline M3 mul_m3(M3 lhs, M3 rhs)
{
  
  M3 result;
  result.columns[0] = mul_m3v3(lhs, rhs.columns[0]);
  result.columns[1] = mul_m3v3(lhs, rhs.columns[1]);
  result.columns[2] = mul_m3v3(lhs, rhs.columns[2]);
  
  return result;
}

internal inline M3 mul_m3f(M3 matrix, f32 Scalar)
{
  
  M3 result;
  
  result.elements[0][0] = matrix.elements[0][0] * Scalar;
  result.elements[0][1] = matrix.elements[0][1] * Scalar;
  result.elements[0][2] = matrix.elements[0][2] * Scalar;
  result.elements[1][0] = matrix.elements[1][0] * Scalar;
  result.elements[1][1] = matrix.elements[1][1] * Scalar;
  result.elements[1][2] = matrix.elements[1][2] * Scalar;
  result.elements[2][0] = matrix.elements[2][0] * Scalar;
  result.elements[2][1] = matrix.elements[2][1] * Scalar;
  result.elements[2][2] = matrix.elements[2][2] * Scalar;
  
  return result;
}

internal inline M3 div_m3f(M3 matrix, f32 Scalar)
{
  
  M3 result;
  
  result.elements[0][0] = matrix.elements[0][0] / Scalar;
  result.elements[0][1] = matrix.elements[0][1] / Scalar;
  result.elements[0][2] = matrix.elements[0][2] / Scalar;
  result.elements[1][0] = matrix.elements[1][0] / Scalar;
  result.elements[1][1] = matrix.elements[1][1] / Scalar;
  result.elements[1][2] = matrix.elements[1][2] / Scalar;
  result.elements[2][0] = matrix.elements[2][0] / Scalar;
  result.elements[2][1] = matrix.elements[2][1] / Scalar;
  result.elements[2][2] = matrix.elements[2][2] / Scalar;
  
  return result;
}

internal inline f32 determinant_m3(M3 matrix)
{
  
  M3 c;
  c.columns[0] = cross_v3(matrix.columns[1], matrix.columns[2]);
  c.columns[1] = cross_v3(matrix.columns[2], matrix.columns[0]);
  c.columns[2] = cross_v3(matrix.columns[0], matrix.columns[1]);
  
  return dot_v3(c.columns[2], matrix.columns[2]);
}

internal inline M3 inv_general_m3(M3 matrix)
{
  
  M3 c;
  c.columns[0] = cross_v3(matrix.columns[1], matrix.columns[2]);
  c.columns[1] = cross_v3(matrix.columns[2], matrix.columns[0]);
  c.columns[2] = cross_v3(matrix.columns[0], matrix.columns[1]);
  
  f32 Invdeterminant = 1.0f / dot_v3(c.columns[2], matrix.columns[2]);
  
  M3 result;
  result.columns[0] = mul_v3f(c.columns[0], Invdeterminant);
  result.columns[1] = mul_v3f(c.columns[1], Invdeterminant);
  result.columns[2] = mul_v3f(c.columns[2], Invdeterminant);
  
  return transpose_m3(result);
}

/*
 * 4x4 Mrices
 */

internal inline M4 m4(void)
{
  M4 result = {0};
  return result;
}

internal inline M4 m4d(f32 Diagonal)
{
  
  M4 result = {0};
  result.elements[0][0] = Diagonal;
  result.elements[1][1] = Diagonal;
  result.elements[2][2] = Diagonal;
  result.elements[3][3] = Diagonal;
  
  return result;
}

internal inline M4 transpose_m4(M4 matrix)
{
  
  M4 result;
	#ifdef HANDMADE_MATH__USE_SSE
		result = matrix;
  _MM_TRANSPOSE4_PS(result.columns[0].SSE, result.columns[1].SSE, result.columns[2].SSE, result.columns[3].SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4x4_t transposed = vld4q_f32((f32*)matrix.columns);
  result.columns[0].NEON = transposed.val[0];
  result.columns[1].NEON = transposed.val[1];
  result.columns[2].NEON = transposed.val[2];
  result.columns[3].NEON = transposed.val[3];
	#else
		result.elements[0][0] = matrix.elements[0][0];
  result.elements[0][1] = matrix.elements[1][0];
  result.elements[0][2] = matrix.elements[2][0];
  result.elements[0][3] = matrix.elements[3][0];
  result.elements[1][0] = matrix.elements[0][1];
  result.elements[1][1] = matrix.elements[1][1];
  result.elements[1][2] = matrix.elements[2][1];
  result.elements[1][3] = matrix.elements[3][1];
  result.elements[2][0] = matrix.elements[0][2];
  result.elements[2][1] = matrix.elements[1][2];
  result.elements[2][2] = matrix.elements[2][2];
  result.elements[2][3] = matrix.elements[3][2];
  result.elements[3][0] = matrix.elements[0][3];
  result.elements[3][1] = matrix.elements[1][3];
  result.elements[3][2] = matrix.elements[2][3];
  result.elements[3][3] = matrix.elements[3][3];
	#endif
		
		return result;
}

internal inline M4 add_m4(M4 lhs, M4 rhs)
{
  
  M4 result;
  
  result.columns[0] = add_v4(lhs.columns[0], rhs.columns[0]);
  result.columns[1] = add_v4(lhs.columns[1], rhs.columns[1]);
  result.columns[2] = add_v4(lhs.columns[2], rhs.columns[2]);
  result.columns[3] = add_v4(lhs.columns[3], rhs.columns[3]);
  
  return result;
}

internal inline M4 sub_m4(M4 lhs, M4 rhs)
{
  
  M4 result;
  
  result.columns[0] = sub_v4(lhs.columns[0], rhs.columns[0]);
  result.columns[1] = sub_v4(lhs.columns[1], rhs.columns[1]);
  result.columns[2] = sub_v4(lhs.columns[2], rhs.columns[2]);
  result.columns[3] = sub_v4(lhs.columns[3], rhs.columns[3]);
  
  return result;
}

internal inline M4 mul_m4(M4 lhs, M4 rhs)
{
  
  M4 result;
  result.columns[0] = linear_combine_v4m4(rhs.columns[0], lhs);
  result.columns[1] = linear_combine_v4m4(rhs.columns[1], lhs);
  result.columns[2] = linear_combine_v4m4(rhs.columns[2], lhs);
  result.columns[3] = linear_combine_v4m4(rhs.columns[3], lhs);
  
  return result;
}

internal inline M4 mul_m4f(M4 matrix, f32 Scalar)
{
  
  M4 result;
  
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSEScalar = _mm_set1_ps(Scalar);
  result.columns[0].SSE = _mm_mul_ps(matrix.columns[0].SSE, SSEScalar);
  result.columns[1].SSE = _mm_mul_ps(matrix.columns[1].SSE, SSEScalar);
  result.columns[2].SSE = _mm_mul_ps(matrix.columns[2].SSE, SSEScalar);
  result.columns[3].SSE = _mm_mul_ps(matrix.columns[3].SSE, SSEScalar);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.columns[0].NEON = vmulq_n_f32(matrix.columns[0].NEON, Scalar);
  result.columns[1].NEON = vmulq_n_f32(matrix.columns[1].NEON, Scalar);
  result.columns[2].NEON = vmulq_n_f32(matrix.columns[2].NEON, Scalar);
  result.columns[3].NEON = vmulq_n_f32(matrix.columns[3].NEON, Scalar);
	#else
		result.elements[0][0] = matrix.elements[0][0] * Scalar;
  result.elements[0][1] = matrix.elements[0][1] * Scalar;
  result.elements[0][2] = matrix.elements[0][2] * Scalar;
  result.elements[0][3] = matrix.elements[0][3] * Scalar;
  result.elements[1][0] = matrix.elements[1][0] * Scalar;
  result.elements[1][1] = matrix.elements[1][1] * Scalar;
  result.elements[1][2] = matrix.elements[1][2] * Scalar;
  result.elements[1][3] = matrix.elements[1][3] * Scalar;
  result.elements[2][0] = matrix.elements[2][0] * Scalar;
  result.elements[2][1] = matrix.elements[2][1] * Scalar;
  result.elements[2][2] = matrix.elements[2][2] * Scalar;
  result.elements[2][3] = matrix.elements[2][3] * Scalar;
  result.elements[3][0] = matrix.elements[3][0] * Scalar;
  result.elements[3][1] = matrix.elements[3][1] * Scalar;
  result.elements[3][2] = matrix.elements[3][2] * Scalar;
  result.elements[3][3] = matrix.elements[3][3] * Scalar;
	#endif
		
		return result;
}

internal inline V4_F32 mul_m4v4(M4 matrix, V4_F32 Vector)
{
  return linear_combine_v4m4(Vector, matrix);
}

internal inline M4 div_m4f(M4 matrix, f32 Scalar)
{
  
  M4 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSEScalar = _mm_set1_ps(Scalar);
  result.columns[0].SSE = _mm_div_ps(matrix.columns[0].SSE, SSEScalar);
  result.columns[1].SSE = _mm_div_ps(matrix.columns[1].SSE, SSEScalar);
  result.columns[2].SSE = _mm_div_ps(matrix.columns[2].SSE, SSEScalar);
  result.columns[3].SSE = _mm_div_ps(matrix.columns[3].SSE, SSEScalar);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t NEONScalar = vdupq_n_f32(Scalar);
  result.columns[0].NEON = vdivq_f32(matrix.columns[0].NEON, NEONScalar);
  result.columns[1].NEON = vdivq_f32(matrix.columns[1].NEON, NEONScalar);
  result.columns[2].NEON = vdivq_f32(matrix.columns[2].NEON, NEONScalar);
  result.columns[3].NEON = vdivq_f32(matrix.columns[3].NEON, NEONScalar);
	#else
		result.elements[0][0] = matrix.elements[0][0] / Scalar;
  result.elements[0][1] = matrix.elements[0][1] / Scalar;
  result.elements[0][2] = matrix.elements[0][2] / Scalar;
  result.elements[0][3] = matrix.elements[0][3] / Scalar;
  result.elements[1][0] = matrix.elements[1][0] / Scalar;
  result.elements[1][1] = matrix.elements[1][1] / Scalar;
  result.elements[1][2] = matrix.elements[1][2] / Scalar;
  result.elements[1][3] = matrix.elements[1][3] / Scalar;
  result.elements[2][0] = matrix.elements[2][0] / Scalar;
  result.elements[2][1] = matrix.elements[2][1] / Scalar;
  result.elements[2][2] = matrix.elements[2][2] / Scalar;
  result.elements[2][3] = matrix.elements[2][3] / Scalar;
  result.elements[3][0] = matrix.elements[3][0] / Scalar;
  result.elements[3][1] = matrix.elements[3][1] / Scalar;
  result.elements[3][2] = matrix.elements[3][2] / Scalar;
  result.elements[3][3] = matrix.elements[3][3] / Scalar;
	#endif
		
		return result;
}

internal inline f32 determinant_m4(M4 matrix)
{
  
  V3_F32 C01 = cross_v3(matrix.columns[0].xyz, matrix.columns[1].xyz);
  V3_F32 C23 = cross_v3(matrix.columns[2].xyz, matrix.columns[3].xyz);
  V3_F32 B10 = sub_v3(mul_v3f(matrix.columns[0].xyz, matrix.columns[1].w), mul_v3f(matrix.columns[1].xyz, matrix.columns[0].w));
  V3_F32 B32 = sub_v3(mul_v3f(matrix.columns[2].xyz, matrix.columns[3].w), mul_v3f(matrix.columns[3].xyz, matrix.columns[2].w));
  
  return dot_v3(C01, B32) + dot_v3(C23, B10);
}

// Returns a general-purpose inverse of an M4. Note that special-purpose inverses of many transformations
// are available and will be more efficient.
internal inline M4 inv_general_m4(M4 matrix)
{
  
  V3_F32 C01 = cross_v3(matrix.columns[0].xyz, matrix.columns[1].xyz);
  V3_F32 C23 = cross_v3(matrix.columns[2].xyz, matrix.columns[3].xyz);
  V3_F32 B10 = sub_v3(mul_v3f(matrix.columns[0].xyz, matrix.columns[1].w), mul_v3f(matrix.columns[1].xyz, matrix.columns[0].w));
  V3_F32 B32 = sub_v3(mul_v3f(matrix.columns[2].xyz, matrix.columns[3].w), mul_v3f(matrix.columns[3].xyz, matrix.columns[2].w));
  
  f32 Invdeterminant = 1.0f / (dot_v3(C01, B32) + dot_v3(C23, B10));
  C01 = mul_v3f(C01, Invdeterminant);
  C23 = mul_v3f(C23, Invdeterminant);
  B10 = mul_v3f(B10, Invdeterminant);
  B32 = mul_v3f(B32, Invdeterminant);
  
  M4 result;
  result.columns[0] = v4v(add_v3(cross_v3(matrix.columns[1].xyz, B32), mul_v3f(C23, matrix.columns[1].w)), -dot_v3(matrix.columns[1].xyz, C23));
  result.columns[1] = v4v(sub_v3(cross_v3(B32, matrix.columns[0].xyz), mul_v3f(C23, matrix.columns[0].w)), +dot_v3(matrix.columns[0].xyz, C23));
  result.columns[2] = v4v(add_v3(cross_v3(matrix.columns[3].xyz, B10), mul_v3f(C01, matrix.columns[3].w)), -dot_v3(matrix.columns[3].xyz, C01));
  result.columns[3] = v4v(sub_v3(cross_v3(B10, matrix.columns[2].xyz), mul_v3f(C01, matrix.columns[2].w)), +dot_v3(matrix.columns[2].xyz, C01));
  
  return transpose_m4(result);
}

/*
 * Common graphics transformations
 */

// Produces a right-handed m4_ortho3d projection matrix with Z ranging from -1 to 1 (the GL convention).
// lhs, rhs, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
internal inline M4 m4_ortho3d_rh_no(f32 lhs, f32 rhs, f32 Bottom, f32 Top, f32 Near, f32 Far)
{
  
  M4 result = {0};
  
  result.elements[0][0] = 2.0f / (rhs - lhs);
  result.elements[1][1] = 2.0f / (Top - Bottom);
  result.elements[2][2] = 2.0f / (Near - Far);
  result.elements[3][3] = 1.0f;
  
  result.elements[3][0] = (lhs + rhs) / (lhs - rhs);
  result.elements[3][1] = (Bottom + Top) / (Bottom - Top);
  result.elements[3][2] = (Near + Far) / (Near - Far);
  
  return result;
}

// Produces a right-handed m4_ortho3d projection matrix with Z ranging from 0 to 1 (the DirectX convention).
// lhs, rhs, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
internal inline M4 m4_ortho3d_rh_zo(f32 lhs, f32 rhs, f32 Bottom, f32 Top, f32 Near, f32 Far)
{
  
  M4 result = {0};
  
  result.elements[0][0] = 2.0f / (rhs - lhs);
  result.elements[1][1] = 2.0f / (Top - Bottom);
  result.elements[2][2] = 1.0f / (Near - Far);
  result.elements[3][3] = 1.0f;
  
  result.elements[3][0] = (lhs + rhs) / (lhs - rhs);
  result.elements[3][1] = (Bottom + Top) / (Bottom - Top);
  result.elements[3][2] = (Near) / (Near - Far);
  
  return result;
}

// Produces a left-handed m4_ortho3d projection matrix with Z ranging from -1 to 1 (the GL convention).
// lhs, rhs, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
internal inline M4 m4_ortho3d_lh_no(f32 lhs, f32 rhs, f32 Bottom, f32 Top, f32 Near, f32 Far)
{
  
  M4 result = m4_ortho3d_rh_no(lhs, rhs, Bottom, Top, Near, Far);
  result.elements[2][2] = -result.elements[2][2];
  
  return result;
}

// Produces a left-handed m4_ortho3d projection matrix with Z ranging from 0 to 1 (the DirectX convention).
// lhs, rhs, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
internal inline M4 m4_ortho3d_lh_zo(f32 lhs, f32 rhs, f32 Bottom, f32 Top, f32 Near, f32 Far)
{
  
  M4 result = m4_ortho3d_rh_zo(lhs, rhs, Bottom, Top, Near, Far);
  result.elements[2][2] = -result.elements[2][2];
  
  return result;
}

// Returns an inverse for the given m4_ortho3d projection matrix. Works for all m4_ortho3d
// projection matrices, regardless of handedness or NDC convention.
internal inline M4 m4_inv_ortho(M4 Orthomatrix)
{
  
  M4 result = {0};
  result.elements[0][0] = 1.0f / Orthomatrix.elements[0][0];
  result.elements[1][1] = 1.0f / Orthomatrix.elements[1][1];
  result.elements[2][2] = 1.0f / Orthomatrix.elements[2][2];
  result.elements[3][3] = 1.0f;
  
  result.elements[3][0] = -Orthomatrix.elements[3][0] * result.elements[0][0];
  result.elements[3][1] = -Orthomatrix.elements[3][1] * result.elements[1][1];
  result.elements[3][2] = -Orthomatrix.elements[3][2] * result.elements[2][2];
  
  return result;
}

internal inline M4 m4_perspective_rh_no(f32 FOV, f32 AspectRatio, f32 Near, f32 Far)
{
  
  M4 result = {0};
  
  // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glum4_perspective.xml
  
  f32 Cotangent = 1.0f / tan_f(FOV / 2.0f);
  result.elements[0][0] = Cotangent / AspectRatio;
  result.elements[1][1] = Cotangent;
  result.elements[2][3] = -1.0f;
  
  result.elements[2][2] = (Near + Far) / (Near - Far);
  result.elements[3][2] = (2.0f * Near * Far) / (Near - Far);
  
  return result;
}

internal inline M4 m4_perspective_rh_zo(f32 FOV, f32 AspectRatio, f32 Near, f32 Far)
{
  
  M4 result = {0};
  
  // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glum4_perspective.xml
  
  f32 Cotangent = 1.0f / tan_f(FOV / 2.0f);
  result.elements[0][0] = Cotangent / AspectRatio;
  result.elements[1][1] = Cotangent;
  result.elements[2][3] = -1.0f;
  
  result.elements[2][2] = (Far) / (Near - Far);
  result.elements[3][2] = (Near * Far) / (Near - Far);
  
  return result;
}

internal inline M4 m4_perspective_lh_no(f32 FOV, f32 AspectRatio, f32 Near, f32 Far)
{
  
  M4 result = m4_perspective_rh_no(FOV, AspectRatio, Near, Far);
  result.elements[2][2] = -result.elements[2][2];
  result.elements[2][3] = -result.elements[2][3];
  
  return result;
}

internal inline M4 m4_perspective_lh_zo(f32 FOV, f32 AspectRatio, f32 Near, f32 Far)
{
  
  M4 result = m4_perspective_rh_zo(FOV, AspectRatio, Near, Far);
  result.elements[2][2] = -result.elements[2][2];
  result.elements[2][3] = -result.elements[2][3];
  
  return result;
}

internal inline M4 m4_inv_perspective_rh(M4 m4_perspectivematrix)
{
  
  M4 result = {0};
  result.elements[0][0] = 1.0f / m4_perspectivematrix.elements[0][0];
  result.elements[1][1] = 1.0f / m4_perspectivematrix.elements[1][1];
  result.elements[2][2] = 0.0f;
  
  result.elements[2][3] = 1.0f / m4_perspectivematrix.elements[3][2];
  result.elements[3][3] = m4_perspectivematrix.elements[2][2] * result.elements[2][3];
  result.elements[3][2] = m4_perspectivematrix.elements[2][3];
  
  return result;
}

internal inline M4 m4_inv_perspective_lh(M4 m4_perspectivematrix)
{
  
  M4 result = {0};
  result.elements[0][0] = 1.0f / m4_perspectivematrix.elements[0][0];
  result.elements[1][1] = 1.0f / m4_perspectivematrix.elements[1][1];
  result.elements[2][2] = 0.0f;
  
  result.elements[2][3] = 1.0f / m4_perspectivematrix.elements[3][2];
  result.elements[3][3] = m4_perspectivematrix.elements[2][2] * -result.elements[2][3];
  result.elements[3][2] = m4_perspectivematrix.elements[2][3];
  
  return result;
}

internal inline M4 
m4_translate(V3_F32 Translation)
{
  
  M4 result = m4d(1.0f);
  result.elements[3][0] = Translation.x;
  result.elements[3][1] = Translation.y;
  result.elements[3][2] = Translation.z;
  
  return result;
}

internal inline M4
m4_inv_translate(M4 Translationmatrix)
{
  
  M4 result = Translationmatrix;
  result.elements[3][0] = -result.elements[3][0];
  result.elements[3][1] = -result.elements[3][1];
  result.elements[3][2] = -result.elements[3][2];
  
  return result;
}

internal inline M4 
m4_rotate_rh(f32 angle, V3_F32 axis)
{
  
  M4 result = m4d(1.0f);
  
  axis = v3_normalize(axis);
  
  f32 SinTheta = sin_f(angle);
  f32 CosTheta = cos_f(angle);
  f32 Cosv = 1.0f - CosTheta;
  
  result.elements[0][0] = (axis.x * axis.x * Cosv) + CosTheta;
  result.elements[0][1] = (axis.x * axis.y * Cosv) + (axis.z * SinTheta);
  result.elements[0][2] = (axis.x * axis.z * Cosv) - (axis.y * SinTheta);
  
  result.elements[1][0] = (axis.y * axis.x * Cosv) - (axis.z * SinTheta);
  result.elements[1][1] = (axis.y * axis.y * Cosv) + CosTheta;
  result.elements[1][2] = (axis.y * axis.z * Cosv) + (axis.x * SinTheta);
  
  result.elements[2][0] = (axis.z * axis.x * Cosv) + (axis.y * SinTheta);
  result.elements[2][1] = (axis.z * axis.y * Cosv) - (axis.x * SinTheta);
  result.elements[2][2] = (axis.z * axis.z * Cosv) + CosTheta;
  
  return result;
}

internal inline M4
m4_rotate_lh(f32 angle, V3_F32 axis)
{
  /* NOTE(lcf): matrix will be inverse/transpose of rh. */
  return m4_rotate_rh(-angle, axis);
}

internal inline M4
m4_inv_rotate(M4 Rotationmatrix)
{
  return transpose_m4(Rotationmatrix);
}

internal inline M4
m4_scale_m(V3_F32 scale)
{
  
  M4 result = m4d(1.0f);
  result.elements[0][0] = scale.x;
  result.elements[1][1] = scale.y;
  result.elements[2][2] = scale.z;
  
  return result;
}

internal inline M4
m4_inv_scale(M4 scalematrix)
{
  
  M4 result = scalematrix;
  result.elements[0][0] = 1.0f / result.elements[0][0];
  result.elements[1][1] = 1.0f / result.elements[1][1];
  result.elements[2][2] = 1.0f / result.elements[2][2];
  
  return result;
}

internal inline M4 _look_at(V3_F32 F,  V3_F32 S, V3_F32 U,  V3_F32 Eye)
{
  M4 result;
  
  result.elements[0][0] = S.x;
  result.elements[0][1] = U.x;
  result.elements[0][2] = -F.x;
  result.elements[0][3] = 0.0f;
  
  result.elements[1][0] = S.y;
  result.elements[1][1] = U.y;
  result.elements[1][2] = -F.y;
  result.elements[1][3] = 0.0f;
  
  result.elements[2][0] = S.z;
  result.elements[2][1] = U.z;
  result.elements[2][2] = -F.z;
  result.elements[2][3] = 0.0f;
  
  result.elements[3][0] = -dot_v3(S, Eye);
  result.elements[3][1] = -dot_v3(U, Eye);
  result.elements[3][2] = dot_v3(F, Eye);
  result.elements[3][3] = 1.0f;
  
  return result;
}

internal inline M4 look_at_rh(V3_F32 Eye, V3_F32 Center, V3_F32 Up)
{
  
  V3_F32 F = v3_normalize(sub_v3(Center, Eye));
  V3_F32 S = v3_normalize(cross_v3(F, Up));
  V3_F32 U = cross_v3(S, F);
  
  return _look_at(F, S, U, Eye);
}

internal inline M4 look_at_lh(V3_F32 Eye, V3_F32 Center, V3_F32 Up)
{
  
  V3_F32 F = v3_normalize(sub_v3(Eye, Center));
  V3_F32 S = v3_normalize(cross_v3(F, Up));
  V3_F32 U = cross_v3(S, F);
  
  return _look_at(F, S, U, Eye);
}

internal inline M4 inv_look_at(M4 matrix)
{
  M4 result;
  
  M3 Rotation = {0};
  Rotation.columns[0] = matrix.columns[0].xyz;
  Rotation.columns[1] = matrix.columns[1].xyz;
  Rotation.columns[2] = matrix.columns[2].xyz;
  Rotation = transpose_m3(Rotation);
  
  result.columns[0] = v4v(Rotation.columns[0], 0.0f);
  result.columns[1] = v4v(Rotation.columns[1], 0.0f);
  result.columns[2] = v4v(Rotation.columns[2], 0.0f);
  result.columns[3] = mul_v4f(matrix.columns[3], -1.0f);
  result.elements[3][0] = -1.0f * matrix.elements[3][0] /
  (Rotation.elements[0][0] + Rotation.elements[0][1] + Rotation.elements[0][2]);
  result.elements[3][1] = -1.0f * matrix.elements[3][1] /
  (Rotation.elements[1][0] + Rotation.elements[1][1] + Rotation.elements[1][2]);
  result.elements[3][2] = -1.0f * matrix.elements[3][2] /
  (Rotation.elements[2][0] + Rotation.elements[2][1] + Rotation.elements[2][2]);
  result.elements[3][3] = 1.0f;
  
  return result;
}

/*
 * Quaternion operations
 */

internal inline Quat quat(f32 X, f32 Y, f32 Z, f32 W)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_setr_ps(X, Y, Z, W);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t v = { X, Y, Z, W };
  result.NEON = v;
	#else
		result.x = X;
  result.y = Y;
  result.z = Z;
  result.w = W;
	#endif
		
		return result;
}

internal inline Quat quat_v4(V4_F32 Vector)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = Vector.SSE;
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = Vector.NEON;
	#else
		result.x = Vector.x;
  result.y = Vector.y;
  result.z = Vector.z;
  result.w = Vector.w;
	#endif
		
		return result;
}

internal inline Quat add_q(Quat lhs, Quat rhs)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_add_ps(lhs.SSE, rhs.SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vaddq_f32(lhs.NEON, rhs.NEON);
	#else
		
		result.x = lhs.x + rhs.x;
  result.y = lhs.y + rhs.y;
  result.z = lhs.z + rhs.z;
  result.w = lhs.w + rhs.w;
	#endif
		
		return result;
}

internal inline Quat sub_q(Quat lhs, Quat rhs)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		result.SSE = _mm_sub_ps(lhs.SSE, rhs.SSE);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vsubq_f32(lhs.NEON, rhs.NEON);
	#else
		result.x = lhs.x - rhs.x;
  result.y = lhs.y - rhs.y;
  result.z = lhs.z - rhs.z;
  result.w = lhs.w - rhs.w;
	#endif
		
		return result;
}

internal inline Quat mul_q(Quat lhs, Quat rhs)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSEresultOne = _mm_xor_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, _MM_SHUFFLE(0, 0, 0, 0)), _mm_setr_ps(0.f, -0.f, 0.f, -0.f));
  __m128 SSEresultTwo = _mm_shuffle_ps(rhs.SSE, rhs.SSE, _MM_SHUFFLE(0, 1, 2, 3));
  __m128 SSEresultThree = _mm_mul_ps(SSEresultTwo, SSEresultOne);
  
  SSEresultOne = _mm_xor_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, _MM_SHUFFLE(1, 1, 1, 1)) , _mm_setr_ps(0.f, 0.f, -0.f, -0.f));
  SSEresultTwo = _mm_shuffle_ps(rhs.SSE, rhs.SSE, _MM_SHUFFLE(1, 0, 3, 2));
  SSEresultThree = _mm_add_ps(SSEresultThree, _mm_mul_ps(SSEresultTwo, SSEresultOne));
  
  SSEresultOne = _mm_xor_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, _MM_SHUFFLE(2, 2, 2, 2)), _mm_setr_ps(-0.f, 0.f, 0.f, -0.f));
  SSEresultTwo = _mm_shuffle_ps(rhs.SSE, rhs.SSE, _MM_SHUFFLE(2, 3, 0, 1));
  SSEresultThree = _mm_add_ps(SSEresultThree, _mm_mul_ps(SSEresultTwo, SSEresultOne));
  
  SSEresultOne = _mm_shuffle_ps(lhs.SSE, lhs.SSE, _MM_SHUFFLE(3, 3, 3, 3));
  SSEresultTwo = _mm_shuffle_ps(rhs.SSE, rhs.SSE, _MM_SHUFFLE(3, 2, 1, 0));
  result.SSE = _mm_add_ps(SSEresultThree, _mm_mul_ps(SSEresultTwo, SSEresultOne));
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t rhs1032 = vrev64q_f32(rhs.NEON);
  f3232x4_t rhs3210 = vcombine_f32(vget_high_f32(rhs1032), vget_low_f32(rhs1032));
  f3232x4_t rhs2301 = vrev64q_f32(rhs3210);
  
  f3232x4_t FirstSign = {1.0f, -1.0f, 1.0f, -1.0f};
  result.NEON = vmulq_f32(rhs3210, vmulq_f32(vdupq_laneq_f32(lhs.NEON, 0), FirstSign));
  f3232x4_t SecondSign = {1.0f, 1.0f, -1.0f, -1.0f};
  result.NEON = vfmaq_f32(result.NEON, rhs2301, vmulq_f32(vdupq_laneq_f32(lhs.NEON, 1), SecondSign));
  f3232x4_t ThirdSign = {-1.0f, 1.0f, 1.0f, -1.0f};
  result.NEON = vfmaq_f32(result.NEON, rhs1032, vmulq_f32(vdupq_laneq_f32(lhs.NEON, 2), ThirdSign));
  result.NEON = vfmaq_laneq_f32(result.NEON, rhs.NEON, lhs.NEON, 3);
  
	#else
		result.x =  rhs.elements[3] * +lhs.elements[0];
  result.y =  rhs.elements[2] * -lhs.elements[0];
  result.z =  rhs.elements[1] * +lhs.elements[0];
  result.w =  rhs.elements[0] * -lhs.elements[0];
  
  result.x += rhs.elements[2] * +lhs.elements[1];
  result.y += rhs.elements[3] * +lhs.elements[1];
  result.z += rhs.elements[0] * -lhs.elements[1];
  result.w += rhs.elements[1] * -lhs.elements[1];
  
  result.x += rhs.elements[1] * -lhs.elements[2];
  result.y += rhs.elements[0] * +lhs.elements[2];
  result.z += rhs.elements[3] * +lhs.elements[2];
  result.w += rhs.elements[2] * -lhs.elements[2];
  
  result.x += rhs.elements[0] * +lhs.elements[3];
  result.y += rhs.elements[1] * +lhs.elements[3];
  result.z += rhs.elements[2] * +lhs.elements[3];
  result.w += rhs.elements[3] * +lhs.elements[3];
	#endif
		
		return result;
}

internal inline Quat mul_qf(Quat lhs, f32 multiplicative)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 Scalar = _mm_set1_ps(multiplicative);
  result.SSE = _mm_mul_ps(lhs.SSE, Scalar);
	#elif defined(HANDMADE_MATH__USE_NEON)
		result.NEON = vmulq_n_f32(lhs.NEON, multiplicative);
	#else
		result.x = lhs.x * multiplicative;
  result.y = lhs.y * multiplicative;
  result.z = lhs.z * multiplicative;
  result.w = lhs.w * multiplicative;
	#endif
		
		return result;
}

internal inline Quat div_qf(Quat lhs, f32 divnd)
{
  
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 Scalar = _mm_set1_ps(divnd);
  result.SSE = _mm_div_ps(lhs.SSE, Scalar);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t Scalar = vdupq_n_f32(divnd);
  result.NEON = vdivq_f32(lhs.NEON, Scalar);
	#else
		result.x = lhs.x / divnd;
  result.y = lhs.y / divnd;
  result.z = lhs.z / divnd;
  result.w = lhs.w / divnd;
	#endif
		
		return result;
}

internal inline f32 dot_q(Quat lhs, Quat rhs)
{
  
  f32 result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSEresultOne = _mm_mul_ps(lhs.SSE, rhs.SSE);
  __m128 SSEresultTwo = _mm_shuffle_ps(SSEresultOne, SSEresultOne, _MM_SHUFFLE(2, 3, 0, 1));
  SSEresultOne = _mm_add_ps(SSEresultOne, SSEresultTwo);
  SSEresultTwo = _mm_shuffle_ps(SSEresultOne, SSEresultOne, _MM_SHUFFLE(0, 1, 2, 3));
  SSEresultOne = _mm_add_ps(SSEresultOne, SSEresultTwo);
  _mm_store_ss(&result, SSEresultOne);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t NEONmultiplyresult = vmulq_f32(lhs.NEON, rhs.NEON);
  f3232x4_t NEONHalfadd = vpaddq_f32(NEONmultiplyresult, NEONmultiplyresult);
  f3232x4_t NEONFulladd = vpaddq_f32(NEONHalfadd, NEONHalfadd);
  result = vgetq_lane_f32(NEONFulladd, 0);
	#else
		result = ((lhs.x * rhs.x) + (lhs.z * rhs.z)) + ((lhs.y * rhs.y) + (lhs.w * rhs.w));
	#endif
		
		return result;
}

internal inline Quat inv_q(Quat lhs)
{
  Quat result;
  result.x = -lhs.x;
  result.y = -lhs.y;
  result.z = -lhs.z;
  result.w = lhs.w;
  
  return div_qf(result, (dot_q(lhs, lhs)));
}

internal inline Quat q_normalize(Quat q)
{
  /* NOTE(lcf): Take advantage of SSE implementation in v4_normalize */
  V4_F32 v= {q.x, q.y, q.z,q .w};
  v = v4_normalize(v);
  Quat result = {v.x, v.y, v.z, v.w};
  
  return result;
}

internal inline Quat _mix_q(Quat lhs, f32 Mixlhs, Quat rhs, f32 Mixrhs) {
  Quat result;
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 Scalarlhs = _mm_set1_ps(Mixlhs);
  __m128 Scalarrhs = _mm_set1_ps(Mixrhs);
  __m128 SSEresultOne = _mm_mul_ps(lhs.SSE, Scalarlhs);
  __m128 SSEresultTwo = _mm_mul_ps(rhs.SSE, Scalarrhs);
  result.SSE = _mm_add_ps(SSEresultOne, SSEresultTwo);
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t scaledlhs = vmulq_n_f32(lhs.NEON, Mixlhs);
  f3232x4_t scaledrhs = vmulq_n_f32(rhs.NEON, Mixrhs);
  result.NEON = vaddq_f32(scaledlhs, scaledrhs);
	#else
		result.x = lhs.x*Mixlhs + rhs.x*Mixrhs;
  result.y = lhs.y*Mixlhs + rhs.y*Mixrhs;
  result.z = lhs.z*Mixlhs + rhs.z*Mixrhs;
  result.w = lhs.w*Mixlhs + rhs.w*Mixrhs;
	#endif
		
		return result;
}

internal inline Quat nlerp(Quat lhs, f32 t, Quat rhs)
{
  Quat result = _mix_q(lhs, 1.0f-t, rhs, t);
  result = q_normalize(result);
  
  return result;
}

internal inline Quat slerp(Quat lhs, f32 t, Quat rhs)
{
  Quat result;
  
  f32 cos_theta = dot_q(lhs, rhs);
  
  if (cos_theta < 0.0f) { /* NOTE(lcf): Take shortest path on Hyper-sphere */
    cos_theta = -cos_theta;
    rhs = quat(-rhs.x, -rhs.y, -rhs.z, -rhs.w);
  }
  
  /* NOTE(lcf): Use normalized Linear interpolation when vectors are roughly not L.I. */
  if (cos_theta > 0.9995f) {
    result = nlerp(lhs, t, rhs);
  } else {
    f32 angle = acos_f(cos_theta);
    f32 Mixlhs = sin_f((1.0f - t) * angle);
    f32 Mixrhs = sin_f(t * angle);
    
    result = _mix_q(lhs, Mixlhs, rhs, Mixrhs);
    result = q_normalize(result);
  }
  
  return result;
}

internal inline M4 q_to_m4(Quat lhs)
{
  
  M4 result;
  
  Quat normalizedQ = q_normalize(lhs);
  
  f32 XX, YY, ZZ,
  XY, XZ, YZ,
  WX, WY, WZ;
  
  XX = normalizedQ.x * normalizedQ.x;
  YY = normalizedQ.y * normalizedQ.y;
  ZZ = normalizedQ.z * normalizedQ.z;
  XY = normalizedQ.x * normalizedQ.y;
  XZ = normalizedQ.x * normalizedQ.z;
  YZ = normalizedQ.y * normalizedQ.z;
  WX = normalizedQ.w * normalizedQ.x;
  WY = normalizedQ.w * normalizedQ.y;
  WZ = normalizedQ.w * normalizedQ.z;
  
  result.elements[0][0] = 1.0f - 2.0f * (YY + ZZ);
  result.elements[0][1] = 2.0f * (XY + WZ);
  result.elements[0][2] = 2.0f * (XZ - WY);
  result.elements[0][3] = 0.0f;
  
  result.elements[1][0] = 2.0f * (XY - WZ);
  result.elements[1][1] = 1.0f - 2.0f * (XX + ZZ);
  result.elements[1][2] = 2.0f * (YZ + WX);
  result.elements[1][3] = 0.0f;
  
  result.elements[2][0] = 2.0f * (XZ + WY);
  result.elements[2][1] = 2.0f * (YZ - WX);
  result.elements[2][2] = 1.0f - 2.0f * (XX + YY);
  result.elements[2][3] = 0.0f;
  
  result.elements[3][0] = 0.0f;
  result.elements[3][1] = 0.0f;
  result.elements[3][2] = 0.0f;
  result.elements[3][3] = 1.0f;
  
  return result;
}

// This method taken from Mike Day at Insomniac Games.
// https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
//
// Note that as mentioned at the top of the paper, the paper assumes the matrix
// would be *post*-multiplied to a vector to rotate it, meaning the matrix is
// the transpose of what we're dealing with. But, because our matrices are
// stored in column-major order, the indices *appear* to match the paper.
//
// For example, m12 in the paper is row 1, column 2. We need to transpose it to
// row 2, column 1. But, because the column comes first when referencing
// elements, it looks like M.elements[1][2].
//
// Don't be confused! Or if you must be confused, at least trust this
// comment. :)
internal inline Quat
m4_to_q_rh(M4 M)
{
  f32 T;
  Quat Q;
  
  if (M.elements[2][2] < 0.0f) {
    if (M.elements[0][0] > M.elements[1][1]) {
      
      T = 1 + M.elements[0][0] - M.elements[1][1] - M.elements[2][2];
      Q = quat(
        T,
        M.elements[0][1] + M.elements[1][0],
        M.elements[2][0] + M.elements[0][2],
        M.elements[1][2] - M.elements[2][1]
      );
    } else {
      T = 1 - M.elements[0][0] + M.elements[1][1] - M.elements[2][2];
      Q = quat(
        M.elements[0][1] + M.elements[1][0],
        T,
        M.elements[1][2] + M.elements[2][1],
        M.elements[2][0] - M.elements[0][2]
      );
    }
  } else {
    if (M.elements[0][0] < -M.elements[1][1]) {
      T = 1 - M.elements[0][0] - M.elements[1][1] + M.elements[2][2];
      Q = quat(
        M.elements[2][0] + M.elements[0][2],
        M.elements[1][2] + M.elements[2][1],
        T,
        M.elements[0][1] - M.elements[1][0]
      );
    } else {
      T = 1 + M.elements[0][0] + M.elements[1][1] + M.elements[2][2];
      Q = quat(
        M.elements[1][2] - M.elements[2][1],
        M.elements[2][0] - M.elements[0][2],
        M.elements[0][1] - M.elements[1][0],
        T
      );
    }
  }
  
  Q = mul_qf(Q, 0.5f / sqrt_f(T));
  
  return Q;
}

internal inline Quat 
m4_to_q_lh(M4 M)
{
  f32 T;
  Quat Q;
  
  if (M.elements[2][2] < 0.0f) {
    if (M.elements[0][0] > M.elements[1][1]) {
      
      T = 1 + M.elements[0][0] - M.elements[1][1] - M.elements[2][2];
      Q = quat(
        T,
        M.elements[0][1] + M.elements[1][0],
        M.elements[2][0] + M.elements[0][2],
        M.elements[2][1] - M.elements[1][2]
      );
    } else {
      T = 1 - M.elements[0][0] + M.elements[1][1] - M.elements[2][2];
      Q = quat(
        M.elements[0][1] + M.elements[1][0],
        T,
        M.elements[1][2] + M.elements[2][1],
        M.elements[0][2] - M.elements[2][0]
      );
    }
  } else {
    if (M.elements[0][0] < -M.elements[1][1]) {
      
      T = 1 - M.elements[0][0] - M.elements[1][1] + M.elements[2][2];
      Q = quat(
        M.elements[2][0] + M.elements[0][2],
        M.elements[1][2] + M.elements[2][1],
        T,
        M.elements[1][0] - M.elements[0][1]
      );
    } else {
      T = 1 + M.elements[0][0] + M.elements[1][1] + M.elements[2][2];
      Q = quat(
        M.elements[2][1] - M.elements[1][2],
        M.elements[0][2] - M.elements[2][0],
        M.elements[1][0] - M.elements[0][2],
        T
      );
    }
  }
  
  Q = mul_qf(Q, 0.5f / sqrt_f(T));
  
  return Q;
}


internal inline Quat quat_from_axis_angle_rh(V3_F32 axis, f32 angle)
{
  Quat result;
  
  V3_F32 normalized = v3_normalize(axis);
	
  result.xyz = mul_v3f(normalized, sin_f(angle / 2.0f));
  result.w = cos_f(angle / 2.0f);
  
  return result;
}

internal inline Quat quat_from_axis_angle_lh(V3_F32 axis, f32 angle)
{
  
  return quat_from_axis_angle_rh(axis, -angle);
}

internal inline V2_F32 rotateV2(V2_F32 V, f32 angle)
{
  
  f32 sinA = sin_f(angle);
  f32 cosA = cos_f(angle);
  
  return v2(V.x * cosA - V.y * sinA, V.x * sinA + V.y * cosA);
}

// implementation from
// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
internal inline V3_F32 rotate_v3_q(V3_F32 V, Quat Q)
{
  V3_F32 t = mul_v3f(cross_v3(Q.xyz, V), 2);
  return add_v3(V, add_v3(mul_v3f(t, Q.w), cross_v3(Q.xyz, t)));
}

internal inline V3_F32 rotatev3_axis_angle_lh(V3_F32 V, V3_F32 axis, f32 angle) {
  
  return rotate_v3_q(V, quat_from_axis_angle_lh(axis, angle));
}

internal inline V3_F32 rotatev3_axis_angle_rh(V3_F32 V, V3_F32 axis, f32 angle) {
  
  return rotate_v3_q(V, quat_from_axis_angle_rh(axis, angle));
}

#ifdef HANDMADE_MATH__USE_C11_GENERICS
#define add(A, B) _Generic((A), \
V2_F32: add_v2, \
V3_F32: add_v3, \
V4_F32: add_v4, \
M2: add_m2, \
M3: add_m3, \
M4: add_m4, \
Quat: add_q \
)(A, B)

#define sub(A, B) _Generic((A), \
V2_F32: sub_v2, \
V3_F32: sub_v3, \
V4_F32: sub_v4, \
M2: sub_m2, \
M3: sub_m3, \
M4: sub_m4, \
Quat: sub_q \
)(A, B)

#define mul(A, B) _Generic((B), \
f32: _Generic((A), \
V2_F32: mul_v2f, \
V3_F32: mul_v3f, \
V4_F32: mul_v4f, \
M2: mul_m2f, \
M3: mul_m3f, \
M4: mul_m4f, \
Quat: mul_qf \
), \
M2: mul_m2, \
M3: mul_m3, \
M4: mul_m4, \
Quat: mul_q, \
default: _Generic((A), \
V2_F32: mul_v2, \
V3_F32: mul_v3, \
V4_F32: mul_v4, \
M2: mul_m2v2, \
M3: mul_m3v3, \
M4: mul_m4v4 \
) \
)(A, B)

#define div(A, B) _Generic((B), \
f32: _Generic((A), \
M2: div_m2f, \
M3: div_m3f, \
M4: div_m4f, \
V2_F32: div_v2f, \
V3_F32: div_v3f, \
V4_F32: div_v4f, \
Quat: div_qf  \
), \
M2: div_m2f, \
M3: div_m3f, \
M4: div_m4f, \
Quat: div_qf, \
default: _Generic((A), \
V2_F32: div_v2, \
V3_F32: div_v3, \
V4_F32: div_v4  \
) \
)(A, B)

#define len(A) _Generic((A), \
V2_F32: len_v2, \
V3_F32: len_v3, \
V4_F32: len_v4  \
)(A)

#define len_sqr(A) _Generic((A), \
V2_F32: len_sqr_v2, \
V3_F32: len_sqr_v3, \
V4_F32: len_sqr_v4  \
)(A)

#define norm(A) _Generic((A), \
V2_F32: v2_normalize, \
V3_F32: v3_normalize, \
V4_F32: v4_normalize  \
)(A)

#define dot(A, B) _Generic((A), \
V2_F32: dot_v2, \
V3_F32: dot_v3, \
V4_F32: dot_v4  \
)(A, B)

#define lerp(A, T, B) _Generic((A), \
f32: lerpf, \
V2_F32: lerp_v2, \
V3_F32: lerp_v3, \
V4_F32: lerp_v4 \
)(A, T, B)

#define eq(A, B) _Generic((A), \
V2_F32: eq_v2, \
V3_F32: eq_v3, \
V4_F32: eq_v4  \
)(A, B)

#define transpose(M) _Generic((M), \
M2: transpose_m2, \
M3: transpose_m3, \
M4: transpose_m4  \
)(M)

#define determinant(M) _Generic((M), \
M2: determinant_m2, \
M3: determinant_m3, \
M4: determinant_m4  \
)(M)

#define inv_general(M) _Generic((M), \
M2: inv_general_m2, \
M3: inv_general_m3, \
M4: inv_general_m4  \
)(M)

#endif


#if LANG_CPP


internal V4_F32
v4(V2_F32 xy)
{
	V4_F32 result;
	result.xy = xy;
	result.z = 0;
	result.w = 0;
	return result;
}

internal inline f32 len(V2_F32 A)
{
	return len_v2(A);
}

internal inline f32 len(V3_F32 A)
{
	return len_v3(A);
}

internal inline f32 len(V4_F32 A)
{
	return len_v4(A);
}

internal inline f32 len_sqr(V2_F32 A)
{
	return len_sqr_v2(A);
}

internal inline f32 len_sqr(V3_F32 A)
{
	return len_sqr_v3(A);
}

internal inline f32 len_sqr(V4_F32 A)
{
	return len_sqr_v4(A);
}

internal inline V2_F32 norm(V2_F32 A)
{
	return v2_normalize(A);
}

internal inline V3_F32 norm(V3_F32 A)
{
	return v3_normalize(A);
}

internal inline V4_F32 norm(V4_F32 A)
{
	return v4_normalize(A);
}

internal inline Quat norm(Quat A)
{
	return q_normalize(A);
}

internal inline f32 dot(V2_F32 lhs, V2_F32 VecTwo)
{
	return dot_v2(lhs, VecTwo);
}

internal inline f32 dot(V3_F32 lhs, V3_F32 VecTwo)
{
	return dot_v3(lhs, VecTwo);
}

internal inline f32 dot(V4_F32 lhs, V4_F32 VecTwo)
{
	return dot_v4(lhs, VecTwo);
}

internal inline V2_F32 lerp(V2_F32 lhs, f32 t, V2_F32 rhs)
{
	return lerp_v2(lhs, t, rhs);
}

internal inline V3_F32 lerp(V3_F32 lhs, f32 t, V3_F32 rhs)
{
	return lerp_v3(lhs, t, rhs);
}

internal inline V4_F32 lerp(V4_F32 lhs, f32 t, V4_F32 rhs)
{
	return lerp_v4(lhs, t, rhs);
}

internal inline M2 transpose(M2 matrix)
{
	return transpose_m2(matrix);
}

internal inline M3 transpose(M3 matrix)
{
	return transpose_m3(matrix);
}

internal inline M4 transpose(M4 matrix)
{
	return transpose_m4(matrix);
}

internal inline f32 determinant(M2 matrix)
{
	return determinant_m2(matrix);
}

internal inline f32 determinant(M3 matrix)
{
	return determinant_m3(matrix);
}

internal inline f32 determinant(M4 matrix)
{
	return determinant_m4(matrix);
}

internal inline M2 inv_general(M2 matrix)
{
	return inv_general_m2(matrix);
}

internal inline M3 inv_general(M3 matrix)
{
	return inv_general_m3(matrix);
}

internal inline M4 inv_general(M4 matrix)
{
	return inv_general_m4(matrix);
}

internal inline f32 dot(Quat QuatOne, Quat QuatTwo)
{
	return dot_q(QuatOne, QuatTwo);
}

internal inline V2_F32 add(V2_F32 lhs, V2_F32 rhs)
{
	return add_v2(lhs, rhs);
}

internal inline V3_F32 add(V3_F32 lhs, V3_F32 rhs)
{
	return add_v3(lhs, rhs);
}

internal inline V4_F32 add(V4_F32 lhs, V4_F32 rhs)
{
	return add_v4(lhs, rhs);
}

internal inline M2 add(M2 lhs, M2 rhs)
{
	return add_m2(lhs, rhs);
}

internal inline M3 add(M3 lhs, M3 rhs)
{
	return add_m3(lhs, rhs);
}

internal inline M4 add(M4 lhs, M4 rhs)
{
	return add_m4(lhs, rhs);
}

internal inline Quat add(Quat lhs, Quat rhs)
{
	return add_q(lhs, rhs);
}

internal inline V2_F32 sub(V2_F32 lhs, V2_F32 rhs)
{
	return sub_v2(lhs, rhs);
}

internal inline V3_F32 sub(V3_F32 lhs, V3_F32 rhs)
{
	return sub_v3(lhs, rhs);
}

internal inline V4_F32 sub(V4_F32 lhs, V4_F32 rhs)
{
	return sub_v4(lhs, rhs);
}

internal inline M2 sub(M2 lhs, M2 rhs)
{
	return sub_m2(lhs, rhs);
}

internal inline M3 sub(M3 lhs, M3 rhs)
{
	return sub_m3(lhs, rhs);
}

internal inline M4 sub(M4 lhs, M4 rhs)
{
	return sub_m4(lhs, rhs);
}

internal inline Quat sub(Quat lhs, Quat rhs)
{
	return sub_q(lhs, rhs);
}

internal inline V2_F32 mul(V2_F32 lhs, V2_F32 rhs)
{
	return mul_v2(lhs, rhs);
}

internal inline V2_F32 mul(V2_F32 lhs, f32 rhs)
{
	return mul_v2f(lhs, rhs);
}

internal inline V3_F32 mul(V3_F32 lhs, V3_F32 rhs)
{
	return mul_v3(lhs, rhs);
}

internal inline V3_F32 mul(V3_F32 lhs, f32 rhs)
{
	return mul_v3f(lhs, rhs);
}

internal inline V4_F32 mul(V4_F32 lhs, V4_F32 rhs)
{
	return mul_v4(lhs, rhs);
}

internal inline V4_F32 mul(V4_F32 lhs, f32 rhs)
{
	return mul_v4f(lhs, rhs);
}

internal inline M2 mul(M2 lhs, M2 rhs)
{
	return mul_m2(lhs, rhs);
}

internal inline M3 mul(M3 lhs, M3 rhs)
{
	return mul_m3(lhs, rhs);
}

internal inline M4 mul(M4 lhs, M4 rhs)
{
	return mul_m4(lhs, rhs);
}

internal inline M2 mul(M2 lhs, f32 rhs)
{
	return mul_m2f(lhs, rhs);
}

internal inline M3 mul(M3 lhs, f32 rhs)
{
	return mul_m3f(lhs, rhs);
}

internal inline M4 mul(M4 lhs, f32 rhs)
{
	return mul_m4f(lhs, rhs);
}

internal inline V2_F32 mul(M2 matrix, V2_F32 Vector)
{
	return mul_m2v2(matrix, Vector);
}

internal inline V3_F32 mul(M3 matrix, V3_F32 Vector)
{
	return mul_m3v3(matrix, Vector);
}

internal inline V4_F32 mul(M4 matrix, V4_F32 Vector)
{
	return mul_m4v4(matrix, Vector);
}

internal inline Quat mul(Quat lhs, Quat rhs)
{
	return mul_q(lhs, rhs);
}

internal inline Quat mul(Quat lhs, f32 rhs)
{
	return mul_qf(lhs, rhs);
}

internal inline V2_F32 div(V2_F32 lhs, V2_F32 rhs)
{
	return div_v2(lhs, rhs);
}

internal inline V2_F32 div(V2_F32 lhs, f32 rhs)
{
	return div_v2f(lhs, rhs);
}

internal inline V3_F32 div(V3_F32 lhs, V3_F32 rhs)
{
	return div_v3(lhs, rhs);
}

internal inline V3_F32 div(V3_F32 lhs, f32 rhs)
{
	return div_v3f(lhs, rhs);
}

internal inline V4_F32 div(V4_F32 lhs, V4_F32 rhs)
{
	return div_v4(lhs, rhs);
}

internal inline V4_F32 div(V4_F32 lhs, f32 rhs)
{
	return div_v4f(lhs, rhs);
}

internal inline M2 div(M2 lhs, f32 rhs)
{
	return div_m2f(lhs, rhs);
}

internal inline M3 div(M3 lhs, f32 rhs)
{
	return div_m3f(lhs, rhs);
}

internal inline M4 div(M4 lhs, f32 rhs)
{
	return div_m4f(lhs, rhs);
}

internal inline Quat div(Quat lhs, f32 rhs)
{
	return div_qf(lhs, rhs);
}

internal inline b32 eq(V2_F32 lhs, V2_F32 rhs)
{
	return eq_v2(lhs, rhs);
}

internal inline b32 eq(V3_F32 lhs, V3_F32 rhs)
{
	return eq_v3(lhs, rhs);
}

internal inline b32 eq(V4_F32 lhs, V4_F32 rhs)
{
	return eq_v4(lhs, rhs);
}

internal inline V2_F32 operator+(V2_F32 lhs, V2_F32 rhs)
{
	return add_v2(lhs, rhs);
}

internal inline V3_F32 operator+(V3_F32 lhs, V3_F32 rhs)
{
	return add_v3(lhs, rhs);
}

internal inline V4_F32 operator+(V4_F32 lhs, V4_F32 rhs)
{
	return add_v4(lhs, rhs);
}

internal inline M2 operator+(M2 lhs, M2 rhs)
{
	return add_m2(lhs, rhs);
}

internal inline M3 operator+(M3 lhs, M3 rhs)
{
	return add_m3(lhs, rhs);
}

internal inline M4 operator+(M4 lhs, M4 rhs)
{
	return add_m4(lhs, rhs);
}

internal inline Quat operator+(Quat lhs, Quat rhs)
{
	return add_q(lhs, rhs);
}

internal inline V2_F32 operator-(V2_F32 lhs, V2_F32 rhs)
{
	return sub_v2(lhs, rhs);
}

internal inline V3_F32 operator-(V3_F32 lhs, V3_F32 rhs)
{
	return sub_v3(lhs, rhs);
}

internal inline V4_F32 operator-(V4_F32 lhs, V4_F32 rhs)
{
	return sub_v4(lhs, rhs);
}

internal inline M2 operator-(M2 lhs, M2 rhs)
{
	return sub_m2(lhs, rhs);
}

internal inline M3 operator-(M3 lhs, M3 rhs)
{
	return sub_m3(lhs, rhs);
}

internal inline M4 operator-(M4 lhs, M4 rhs)
{
	return sub_m4(lhs, rhs);
}

internal inline Quat operator-(Quat lhs, Quat rhs)
{
	return sub_q(lhs, rhs);
}

internal inline V2_F32 operator*(V2_F32 lhs, V2_F32 rhs)
{
	return mul_v2(lhs, rhs);
}

internal inline V3_F32 operator*(V3_F32 lhs, V3_F32 rhs)
{
	return mul_v3(lhs, rhs);
}

internal inline V4_F32 operator*(V4_F32 lhs, V4_F32 rhs)
{
	return mul_v4(lhs, rhs);
}

internal inline M2 operator*(M2 lhs, M2 rhs)
{
	return mul_m2(lhs, rhs);
}

internal inline M3 operator*(M3 lhs, M3 rhs)
{
	return mul_m3(lhs, rhs);
}

internal inline M4 operator*(M4 lhs, M4 rhs)
{
	return mul_m4(lhs, rhs);
}

internal inline Quat operator*(Quat lhs, Quat rhs)
{
	return mul_q(lhs, rhs);
}

internal inline V2_F32 operator*(V2_F32 lhs, f32 rhs)
{
	return mul_v2f(lhs, rhs);
}

internal inline V3_F32 operator*(V3_F32 lhs, f32 rhs)
{
	return mul_v3f(lhs, rhs);
}

internal inline V4_F32 operator*(V4_F32 lhs, f32 rhs)
{
	return mul_v4f(lhs, rhs);
}

internal inline M2 operator*(M2 lhs, f32 rhs)
{
	return mul_m2f(lhs, rhs);
}

internal inline M3 operator*(M3 lhs, f32 rhs)
{
	return mul_m3f(lhs, rhs);
}

internal inline M4 operator*(M4 lhs, f32 rhs)
{
	return mul_m4f(lhs, rhs);
}

internal inline Quat operator*(Quat lhs, f32 rhs)
{
	return mul_qf(lhs, rhs);
}

internal inline V2_F32 operator*(f32 lhs, V2_F32 rhs)
{
	return mul_v2f(rhs, lhs);
}

internal inline V3_F32 operator*(f32 lhs, V3_F32 rhs)
{
	return mul_v3f(rhs, lhs);
}

internal inline V4_F32 operator*(f32 lhs, V4_F32 rhs)
{
	return mul_v4f(rhs, lhs);
}

internal inline M2 operator*(f32 lhs, M2 rhs)
{
	return mul_m2f(rhs, lhs);
}

internal inline M3 operator*(f32 lhs, M3 rhs)
{
	return mul_m3f(rhs, lhs);
}

internal inline M4 operator*(f32 lhs, M4 rhs)
{
	return mul_m4f(rhs, lhs);
}

internal inline Quat operator*(f32 lhs, Quat rhs)
{
	return mul_qf(rhs, lhs);
}

internal inline V2_F32 operator*(M2 matrix, V2_F32 Vector)
{
	return mul_m2v2(matrix, Vector);
}

internal inline V3_F32 operator*(M3 matrix, V3_F32 Vector)
{
	return mul_m3v3(matrix, Vector);
}

internal inline V4_F32 operator*(M4 matrix, V4_F32 Vector)
{
	return mul_m4v4(matrix, Vector);
}

internal inline V2_F32 operator/(V2_F32 lhs, V2_F32 rhs)
{
	return div_v2(lhs, rhs);
}

internal inline V3_F32 operator/(V3_F32 lhs, V3_F32 rhs)
{
	return div_v3(lhs, rhs);
}

internal inline V4_F32 operator/(V4_F32 lhs, V4_F32 rhs)
{
	return div_v4(lhs, rhs);
}

internal inline V2_F32 operator/(V2_F32 lhs, f32 rhs)
{
	return div_v2f(lhs, rhs);
}

internal inline V3_F32 operator/(V3_F32 lhs, f32 rhs)
{
	return div_v3f(lhs, rhs);
}

internal inline V4_F32 operator/(V4_F32 lhs, f32 rhs)
{
	return div_v4f(lhs, rhs);
}

internal inline M4 operator/(M4 lhs, f32 rhs)
{
	return div_m4f(lhs, rhs);
}

internal inline M3 operator/(M3 lhs, f32 rhs)
{
	return div_m3f(lhs, rhs);
}

internal inline M2 operator/(M2 lhs, f32 rhs)
{
	return div_m2f(lhs, rhs);
}

internal inline Quat operator/(Quat lhs, f32 rhs)
{
	return div_qf(lhs, rhs);
}

internal inline V2_F32 &operator+=(V2_F32 &lhs, V2_F32 rhs)
{
	return lhs = lhs + rhs;
}

internal inline V3_F32 &operator+=(V3_F32 &lhs, V3_F32 rhs)
{
	return lhs = lhs + rhs;
}

internal inline V4_F32 &operator+=(V4_F32 &lhs, V4_F32 rhs)
{
	return lhs = lhs + rhs;
}

internal inline M2 &operator+=(M2 &lhs, M2 rhs)
{
	return lhs = lhs + rhs;
}

internal inline M3 &operator+=(M3 &lhs, M3 rhs)
{
	return lhs = lhs + rhs;
}

internal inline M4 &operator+=(M4 &lhs, M4 rhs)
{
	return lhs = lhs + rhs;
}

internal inline Quat &operator+=(Quat &lhs, Quat rhs)
{
	return lhs = lhs + rhs;
}

internal inline V2_F32 &operator-=(V2_F32 &lhs, V2_F32 rhs)
{
	return lhs = lhs - rhs;
}

internal inline V3_F32 &operator-=(V3_F32 &lhs, V3_F32 rhs)
{
	return lhs = lhs - rhs;
}

internal inline V4_F32 &operator-=(V4_F32 &lhs, V4_F32 rhs)
{
	return lhs = lhs - rhs;
}

internal inline M2 &operator-=(M2 &lhs, M2 rhs)
{
	return lhs = lhs - rhs;
}

internal inline M3 &operator-=(M3 &lhs, M3 rhs)
{
	return lhs = lhs - rhs;
}

internal inline M4 &operator-=(M4 &lhs, M4 rhs)
{
	return lhs = lhs - rhs;
}

internal inline Quat &operator-=(Quat &lhs, Quat rhs)
{
	return lhs = lhs - rhs;
}

internal inline V2_F32 &operator*=(V2_F32 &lhs, V2_F32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline V3_F32 &operator*=(V3_F32 &lhs, V3_F32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline V4_F32 &operator*=(V4_F32 &lhs, V4_F32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline V2_F32 &operator*=(V2_F32 &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline V3_F32 &operator*=(V3_F32 &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline V4_F32 &operator*=(V4_F32 &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline M2 &operator*=(M2 &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline M3 &operator*=(M3 &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline M4 &operator*=(M4 &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline Quat &operator*=(Quat &lhs, f32 rhs)
{
	return lhs = lhs * rhs;
}

internal inline V2_F32 &operator/=(V2_F32 &lhs, V2_F32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline V3_F32 &operator/=(V3_F32 &lhs, V3_F32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline V4_F32 &operator/=(V4_F32 &lhs, V4_F32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline V2_F32 &operator/=(V2_F32 &lhs, f32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline V3_F32 &operator/=(V3_F32 &lhs, f32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline V4_F32 &operator/=(V4_F32 &lhs, f32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline M4 &operator/=(M4 &lhs, f32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline Quat &operator/=(Quat &lhs, f32 rhs)
{
	return lhs = lhs / rhs;
}

internal inline b32 operator==(V2_F32 lhs, V2_F32 rhs)
{
	return eq_v2(lhs, rhs);
}

internal inline b32 operator==(V3_F32 lhs, V3_F32 rhs)
{
	return eq_v3(lhs, rhs);
}

internal inline b32 operator==(V4_F32 lhs, V4_F32 rhs)
{
	return eq_v4(lhs, rhs);
}

internal inline b32 operator!=(V2_F32 lhs, V2_F32 rhs)
{
	return !eq_v2(lhs, rhs);
}

internal inline b32 operator!=(V3_F32 lhs, V3_F32 rhs)
{
	return !eq_v3(lhs, rhs);
}

internal inline b32 operator!=(V4_F32 lhs, V4_F32 rhs)
{
	return !eq_v4(lhs, rhs);
}

internal inline V2_F32 operator-(V2_F32 In)
{
	
	V2_F32 Result;
	Result.x = -In.x;
	Result.y = -In.y;
	
	return Result;
}

internal inline V3_F32 operator-(V3_F32 In)
{
	
	V3_F32 Result;
	Result.x = -In.x;
	Result.y = -In.y;
	Result.z = -In.z;
	
	return Result;
}

internal inline V4_F32 operator-(V4_F32 In)
{
	
	V4_F32 Result;
	#if HANDMADE_MATH__USE_SSE
		Result.SSE = _mm_xor_ps(In.SSE, _mm_set1_ps(-0.0f));
	#elif defined(HANDMADE_MATH__USE_NEON)
		f3232x4_t Zero = vdupq_n_f32(0.0f);
	Result.NEON = vsubq_f32(Zero, In.NEON);
	#else
		Result.X = -In.X;
	Result.Y = -In.Y;
	Result.Z = -In.Z;
	Result.W = -In.W;
	#endif
		
		return Result;
}

#endif

//~ NOTE(fakhri): Range functions
internal inline Range2_F32
range2f32(f32 min_x, f32 max_x, f32 min_y, f32 max_y)
{
	Range2_F32 result;
	result.min_x = min_x;
	result.max_x = max_x;
	result.min_y = min_y;
	result.max_y = max_y;
	return result;
}

internal inline Range2_F32
range2f32_min_max(V2_F32 min_p, V2_F32 max_p)
{
	Range2_F32 result;
  result.minp = min_p;
  result.maxp = max_p;
  return result;
}

internal inline Range2_F32 
range2f32_center_dim(V2_F32 center, V2_F32 dim)
{
	V2_F32 half_dim = mul(dim, 0.5f);
	Range2_F32 result = range2f32_min_max(sub(center, half_dim),
		add(center, half_dim));
  return result;
}

internal inline Range2_F32 
range2f32_topleft_dim(V2_F32 topleft, V2_F32 dim)
{
  Range2_F32 result = range2f32_min_max(v2(topleft.x, topleft.y - dim.height), v2(topleft.x + dim.width, topleft.y));
  return result;
}

internal V2_I32
range2i32_dim(Range2_I32 range)
{
	V2_I32 result;
	result = v2i(ABS(range.max_x - range.min_x), 
		ABS(range.max_y - range.min_y)
	);
	return result;
}

internal V2_F32
range2f32_dim(Range2_F32 range)
{
	V2_F32 result;
	result = v2(ABS(range.max_x - range.min_x), 
		ABS(range.max_y - range.min_y)
	);
	return result;
}

internal V2_F32
range2f32_center(Range2_F32 range)
{
	V2_F32 dim = range2f32_dim(range);
	V2_F32 result = add(range.minp, mul(dim, 0.5f));
	return result;
}



internal Range2_F32_Cut
range2f32_cut_top(Range2_F32 rect, f32 cut_h)
{
	if (cut_h < 0) cut_h = 0; 
	
	Range2_F32 top    = rect;
	Range2_F32 bottom = rect;
	
	top.min_y    = top.max_y - cut_h;
	bottom.max_y = top.min_y;
	
	Range2_F32_Cut result;
	result.top    = top;
	result.bottom = bottom;
	
	return result;
}

internal Range2_F32_Cut
range2f32_cut_bottom(Range2_F32 rect, f32 cut_h)
{
	if (cut_h < 0) cut_h = 0; 
	
	Range2_F32 top    = rect;
	Range2_F32 bottom = rect;
	
	bottom.max_y = bottom.min_y + cut_h;
	top   .min_y = bottom.max_y;
	
	Range2_F32_Cut result;
	result.top    = top;
	result.bottom = bottom;
	
	return result;
}

internal Range2_F32_Cut
range2f32_cut_left(Range2_F32 rect, f32 cut_w)
{
	if (cut_w < 0) cut_w = 0; 
	Range2_F32 left  = rect;
	Range2_F32 right = rect;
	
	left.max_x = left.min_x + cut_w;
	right.min_x = left.max_x;
	
	Range2_F32_Cut result;
	result.left  = left;
	result.right = right;
	
	return result;
}

internal Range2_F32_Cut
range2f32_cut_right(Range2_F32 rect, f32 cut_w)
{
	if (cut_w < 0) cut_w = 0; 
	Range2_F32 left  = rect;
	Range2_F32 right = rect;
	
	right.min_x = right.max_x - cut_w;
	left .max_x = right.min_x;
	
	
	Range2_F32_Cut result;
	result.left  = left;
	result.right = right;
	
	
	return result;
}


internal inline b32
range2f32_is_inside(Range2_F32 range, V2_F32 p)
{
  b32 result = ((p.x >= range.minp.x && p.x < range.maxp.x) &&
    (p.y >= range.minp.y && p.y < range.maxp.y));
  return result;
}

internal inline b32
range2f32_check_intersect(Range2_F32 range1, Range2_F32 range2)
{
  b32 result = !(range1.max_x < range2.min_x ||
    range2.max_x < range1.min_x ||
    range1.max_y < range2.min_y ||
    range2.max_y < range1.min_y
  );
  return result;
}

internal Range2_F32
range2f32_intersection(Range2_F32 range1, Range2_F32 range2)
{
	Range2_F32 result;
	result.min_y = MAX(range1.min_y, range2.min_y);
	result.max_y = MIN(range1.max_y, range2.max_y);
	
	result.min_x = MAX(range1.min_x, range2.min_x);
	result.max_x = MIN(range1.max_x, range2.max_x);
	return result;
}

internal void 
accelerate_towards(V3_F32 *pos, V3_F32 *vel, V3_F32 target_pos, f32 freq, f32 zeta, f32 dt)
{
	f32 k1 = zeta / (PI32 * freq);
	f32 k2 = SQUARE(2 * PI32 * freq);
	
	V3_F32 accel = mul(sub(sub(target_pos, *pos),
			mul(*vel, k1)),
		k2);
	*vel = add(*vel, mul(accel, dt));
	*pos = add(*pos, 
		add(mul(*vel, dt),
			mul(accel, 0.5f * SQUARE(dt))));
}

