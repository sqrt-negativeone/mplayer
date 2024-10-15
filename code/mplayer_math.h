/* date = October 12th 2024 4:28 pm */

#ifndef MPLAYER_MATH_H
#define MPLAYER_MATH_H

#define USE_SSE 1

#include <xmmintrin.h>

#define CLAMP(a, x, b) (((x) <= (a))? (a) : (((b) <= (x))? (b):(x)))
#define MAX(a, b) (((a) >= (b))? (a):(b))
#define MIN(a, b) (((a) <= (b))? (a):(b))
#define ABS(a) (((a) >= 0)? (a) : -(a))

union V2_F32
{
	struct {f32 x, y;};
	struct {f32 width, height;};
};

union V3_F32
{
	struct
	{
		union
		{
			struct {f32 x, y;};
			V2_F32 xy;
		};
		f32 z;
	};
	struct {f32 width, height, depth;};
	struct {f32 r, g, b;};
};

union V4_F32
{
	struct {V2_F32 xy; V2_F32 zw;};
	struct 
	{
		union
		{
			V3_F32 xyz; 
			struct {f32 x, y, z;};
		}; 
		f32 w;
	};
	struct {f32 r, g, b, a;};
	f32 elements[4];
	
	#if USE_SSE
		__m128 SSE;
	#endif
		
	#ifdef LANG_CPP
		inline float &operator[](int index) { return elements[index]; }
  inline const float &operator[](int index) const { return elements[index]; }
	#endif
};

union M4
{
	f32 elements[4][4];
	V4_F32 columns[4];
	
	#ifdef LANG_CPP
		inline V4_F32 &operator[](int index) { return columns[index]; }
  inline const V4_F32 &operator[](int index) const { return columns[index]; }
	#endif
};

struct M4_Inv
{
	M4 mat;
	M4 inv;
};

union V2_I32
{
	struct {i32 x, y;};
	struct {i32 width, height;};
};

union Range1_F32
{
	struct {f32 min_x, max_x;};
};

union Range2_I32
{
	struct {V2_I32 minp, maxp;};
	struct {i32 min_x, min_y, max_x, max_y;};
};

union Range2_F32
{
	struct {V2_F32 minp, maxp;};
	struct {f32 min_x, min_y, max_x, max_y;};
};



internal V2_I32
vec2i(u32 x, u32 y)
{
	V2_I32 result;
	result.x = x;
	result.y = y;
	return result;
}

internal V2_F32
vec2()
{
	V2_F32 result = ZERO_STRUCT;
	return result;
}

internal V2_F32
vec2(f32 x, f32 y)
{
	V2_F32 result;
	result.x = x;
	result.y = y;
	return result;
}

internal V2_F32
vec2(f32 v)
{
	V2_F32 result = vec2(v, v);
	return result;
}

internal V2_F32
vec2(V2_I32 v)
{
	V2_F32 result = vec2(f32(v.x), f32(v.y));
	return result;
}

internal V3_F32
vec3()
{
	V3_F32 result = ZERO_STRUCT;
	return result;
}

internal V3_F32
vec3(f32 x, f32 y, f32 z)
{
	V3_F32 result;
	result.x = x;
	result.y = y;
	result.z = z;
	return result;
}

internal V3_F32
vec3(V2_F32 xy, f32 z)
{
	V3_F32 result;
	result.xy = xy;
	result.z = z;
	return result;
}

internal V4_F32
vec4(f32 x, f32 y, f32 z, f32 w)
{
	V4_F32 result;
	result.x = x;
	result.y = y;
	result.z = z;
	result.w = w;
	return result;
}

internal V4_F32
vec4(V2_F32 xy)
{
	V4_F32 result;
	result.xy = xy;
	result.z = 0;
	result.w = 0;
	return result;
}

internal inline i32
round_f32_i32(f32 x)
{
  i32 result = _mm_cvtss_si32(_mm_set_ss(x));
  return result;
}

internal M4
m4d(f32 diagonal)
{
	M4 result = ZERO_STRUCT;
	result.elements[0][0] = diagonal;
	result.elements[1][1] = diagonal;
	result.elements[2][2] = diagonal;
	result.elements[3][3] = diagonal;
	return result;
}

internal M4
m4_ident()
{
	M4 result = m4d(1);
	return result;
}

internal M4_Inv
m4_inv_ident()
{
	M4_Inv result;
	result.mat = m4_ident();
	result.inv = m4_ident();
	return result;
}

internal M4
m4_translate(V3_F32 t)
{
	M4 result = m4_ident();
	result[3].xyz = t;
	return result;
}

internal M4
m4_scale(V3_F32 s)
{
	M4 result = m4_ident();
	result.elements[0][0] = s.x;
	result.elements[1][1] = s.y;
	result.elements[2][2] = s.z;
	return result;
}

internal M4
m4_ortho3d(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far)
{
  M4 result = ZERO_STRUCT;
  
  result.elements[0][0] = 2.0f / (right - left);
  result.elements[1][1] = 2.0f / (top - bottom);
  result.elements[2][2] = 2.0f / (near - far);
  result.elements[3][3] = 1.0f;
  
  result.elements[3][0] = (left + right) / (left - right);
  result.elements[3][1] = (bottom + top) / (bottom - top);
  result.elements[3][2] = (near + far) / (near - far);
  
  return result;
}

internal inline M4
m4_inv_translate(M4 translate)
{
  M4 result = translate;
  result.elements[3][0] = -result.elements[3][0];
  result.elements[3][1] = -result.elements[3][1];
  result.elements[3][2] = -result.elements[3][2];
  
  return result;
}

// Returns an inverse for the given orthographic projection matrix. Works for all orthographic
// projection matrices, regardless of handedness or NDC convention.
internal inline M4 
m4_inv_ortho(M4 ortho)
{
  M4 result = {0};
  result.elements[0][0] = 1.0f / ortho.elements[0][0];
  result.elements[1][1] = 1.0f / ortho.elements[1][1];
  result.elements[2][2] = 1.0f / ortho.elements[2][2];
  result.elements[3][3] = 1.0f;
  
  result.elements[3][0] = -ortho.elements[3][0] * result.elements[0][0];
  result.elements[3][1] = -ortho.elements[3][1] * result.elements[1][1];
  result.elements[3][2] = -ortho.elements[3][2] * result.elements[2][2];
  
  return result;
}

// NOTE(fakhri): addition
internal inline V2_F32
add_v2(V2_F32 lhs, V2_F32 rhs)
{
	V2_F32 result = lhs;
	result.x += rhs.x;
	result.y += rhs.y;
	return result;
}

// NOTE(fakhri): addition
internal inline V2_F32
sub_v2(V2_F32 lhs, V2_F32 rhs)
{
	V2_F32 result = lhs;
	result.x -= rhs.x;
	result.y -= rhs.y;
	return result;
}

internal inline V2_F32
operator+(V2_F32 lhs, V2_F32 rhs)
{
	return add_v2(lhs, rhs);
}

internal inline V2_F32
operator-(V2_F32 lhs, V2_F32 rhs)
{
	return sub_v2(lhs, rhs);
}

// NOTE(fakhri): multiplication

internal inline V2_F32
mul_v2(V2_F32 lhs, V2_F32 rhs)
{
	V2_F32 result = lhs;
	result.x *= rhs.x;
	result.y *= rhs.y;
	return result;
}

internal inline V2_F32
mul_v2(V2_F32 lhs, f32 v)
{
	V2_F32 result = lhs;
	result.x *= v;
	result.y *= v;
	return result;
}

internal inline V3_F32
mul_v3(V3_F32 lhs, V3_F32 rhs)
{
	V3_F32 result = lhs;
	result.x *= rhs.x;
	result.y *= rhs.y;
	result.z *= rhs.z;
	return result;
}

internal inline V3_F32
mul_v3(V3_F32 lhs, f32 v)
{
	V3_F32 result = lhs;
	result.x *= v;
	result.y *= v;
	result.z *= v;
	return result;
}

internal inline V4_F32
linear_combine_v4m4(V4_F32 lhs, M4 rhs)
{
  V4_F32 result;
	#if USE_SSE
		result.SSE = _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0x00), rhs.columns[0].SSE);
  result.SSE = _mm_add_ps(result.SSE, _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0x55), rhs.columns[1].SSE));
  result.SSE = _mm_add_ps(result.SSE, _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0xaa), rhs.columns[2].SSE));
  result.SSE = _mm_add_ps(result.SSE, _mm_mul_ps(_mm_shuffle_ps(lhs.SSE, lhs.SSE, 0xff), rhs.columns[3].SSE));
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

internal inline V4_F32
mul_m4(M4 matrix, V4_F32 vector)
{
	V4_F32 result = linear_combine_v4m4(vector, matrix);
  return result;
}

internal inline M4
mul_m4(M4 lhs, M4 rhs)
{
  M4 result;
  result.columns[0] = linear_combine_v4m4(rhs.columns[0], lhs);
  result.columns[1] = linear_combine_v4m4(rhs.columns[1], lhs);
  result.columns[2] = linear_combine_v4m4(rhs.columns[2], lhs);
  result.columns[3] = linear_combine_v4m4(rhs.columns[3], lhs);
  
  return result;
}

internal inline V2_F32
operator*(V2_F32 lhs, V2_F32 rhs)
{
	return mul_v2(lhs, rhs);
}

internal inline V2_F32
operator*(V2_F32 lhs, f32 v)
{
	return mul_v2(lhs, v);
}

internal inline V2_F32
operator*(f32 v, V2_F32 rhs)
{
	return mul_v2(rhs, v);
}

internal inline V3_F32
operator*(V3_F32 lhs, V3_F32 rhs)
{
	return mul_v3(lhs, rhs);
}

internal inline V3_F32
operator*(V3_F32 lhs, f32 v)
{
	return mul_v3(lhs, v);
}

internal inline V3_F32
operator*(f32 v, V3_F32 rhs)
{
	return mul_v3(rhs, v);
}


internal inline M4
operator*(M4 lhs, M4 rhs)
{
	return mul_m4(lhs, rhs);
}

internal inline V4_F32
operator*(M4 lhs, V4_F32 rhs)
{
	return mul_m4(lhs, rhs);
}

internal inline Range2_F32
range_min_max(V2_F32 min_p, V2_F32 max_p)
{
	Range2_F32 result;
  result.minp = min_p;
  result.maxp = max_p;
  return result;
}

internal inline Range2_F32
range_center_half_dim(V2_F32 center, V2_F32 half_dim)
{
  Range2_F32 result = range_min_max(center - half_dim, 
    center + half_dim);
  return result;
}

internal inline Range2_F32 
range_center_dim(V2_F32 center, V2_F32 dim)
{
  Range2_F32 result = range_center_half_dim(center, dim * 0.5f);
  return result;
}

internal V2_I32
range_dim(Range2_I32 range)
{
	V2_I32 result;
	result = vec2i(ABS(range.max_x - range.min_x), 
		ABS(range.max_y - range.min_y)
	);
	return result;
}

internal V2_F32
range_dim(Range2_F32 range)
{
	V2_F32 result;
	result = vec2(ABS(range.max_x - range.min_x), 
		ABS(range.max_y - range.min_y)
	);
	return result;
}

internal V2_F32
range_center(Range2_F32 range)
{
	V2_F32 dim = range_dim(range);
	V2_F32 result = range.minp + 0.5f * dim;
	return result;
}

internal Range2_F32
rect_cut_top(Range2_F32 rect, f32 cut_h)
{
	Range2_F32 top = rect;
	top.min_y = top.max_y - cut_h;
	return top;
}

internal Range2_F32
rect_cut_bottom(Range2_F32 rect, f32 cut_h)
{
	Range2_F32 bottom = rect;
	bottom.max_y = bottom.min_y + cut_h;
	return bottom;
}

internal Range2_F32
rect_cut_left(Range2_F32 rect, f32 cut_w)
{
	Range2_F32 left = rect;
	left.max_x = left.min_x + cut_w;
	return left;
}

internal Range2_F32
rect_cut_right(Range2_F32 rect, f32 cut_w)
{
	Range2_F32 right = rect;
	right.min_x = right.max_x - cut_w;
	return right;
}

internal Range2_F32
rect_cut_top_percentage(Range2_F32 rect, f32 h_percent)
{
	assert(0 <= h_percent && h_percent <= 1);
	Range2_F32 top = rect_cut_top(rect, h_percent * range_dim(rect).height);
	return top;
}

internal Range2_F32
rect_cut_bottom_percentage(Range2_F32 rect, f32 h_percent)
{
	assert(0 <= h_percent && h_percent <= 1);
	Range2_F32 bottom = rect_cut_bottom(rect, h_percent * range_dim(rect).height);
	return bottom;
}

internal Range2_F32
rect_cut_left_percentage(Range2_F32 rect, f32 w_percent)
{
	assert(0 <= w_percent && w_percent <= 1);
	Range2_F32 left = rect_cut_left(rect, w_percent * range_dim(rect).width);
	return left;
}

internal Range2_F32
rect_cut_right_percentage(Range2_F32 rect, f32 w_percent)
{
	assert(0 <= w_percent && w_percent <= 1);
	Range2_F32 right = rect_cut_right(rect, w_percent * range_dim(rect).width);
	return right;
}


internal inline b32
is_in_range(Range2_F32 range, V2_F32 p)
{
  b32 result = ((p.x >= range.minp.x && p.x < range.maxp.x) &&
    (p.y >= range.minp.y && p.y < range.maxp.y));
  return result;
}

internal inline b32
range_intersect(Range2_F32 range1, Range2_F32 range2)
{
  b32 result = !(range1.max_x < range2.min_x ||
    range2.max_x < range1.min_x ||
    range1.max_y < range2.min_y ||
    range2.max_y < range1.min_y
  );
  return result;
}


internal inline f32
map_into_range_zo(f32 min, f32 value, f32 max)
{
  f32 result = 0;
  f32 width = (max - min);
  if (width != 0)
  {
    result = (value - min) / width;
  }
  return result;
}

internal inline f32
map_into_range_no(f32 min, f32 value, f32 max)
{
  f32 result = -1.0f + 2.0f * map_into_range_zo(min, value, max);
  return result;
}

internal inline f32
lerp(f32 a, f32 t, f32 b)
{
	f32 result = (1 - t) * a + t * b;
	return result;
}

#endif //MPLAYER_MATH_H
