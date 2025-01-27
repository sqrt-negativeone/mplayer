/* date = October 12th 2024 4:28 pm */

#ifndef MPLAYER_MATH_H
#define MPLAYER_MATH_H

#define USE_SSE 1

#include <math.h>

#include <xmmintrin.h>

#define SQUARE(x) ((x) * (x))
#define CLAMP(a, x, b) (((x) <= (a))? (a) : (((b) <= (x))? (b):(x)))
#define MAX(a, b) (((a) >= (b))? (a):(b))
#define MIN(a, b) (((a) <= (b))? (a):(b))
#define ABS(a) (((a) >= 0)? (a) : -(a))

#define PI 3.14159265358979323846
#define PI32 3.14159265359f

union V2_F32
{
	struct {f32 x, y;};
	struct {f32 width, height;};
	f32 v[2];
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
	struct {V2_F32 rg; V2_F32 ba;};
	struct 
	{
		union
		{
			V3_F32 rgb; 
			struct {f32 r, g, b;};
		}; 
		f32 a;
	};
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

union Range2_F32_Cut
{
	struct { Range2_F32 left, right;};
	struct { Range2_F32 top, bottom;};
};

#define RANGE2_F32_EMPTY ZERO_STRUCT
#define RANGE2_F32_FULL_ZO Range2_F32{.min_x = 0, .min_y = 0, .max_x = 1, .max_y = 1}

internal inline f32
pow_f(f32 base, f32 exp)
{
	f32 result = powf(base, exp);
	return result;
}

internal inline f32
sin_f(f32 angle)
{
	f32 result = sinf(angle);
	return result;
}

internal V2_I32
vec2i(i32 x, i32 y)
{
	V2_I32 result;
	result.x = x;
	result.y = y;
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
vec2()
{
	V2_F32 result = vec2(0);
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
vec4(f32 d)
{
	V4_F32 result = vec4(d, d, d, d);
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

internal inline V2_F32
sub_v2(V2_F32 lhs, V2_F32 rhs)
{
	V2_F32 result = lhs;
	result.x -= rhs.x;
	result.y -= rhs.y;
	return result;
}


internal inline V4_F32
add_v4(V4_F32 lhs, V4_F32 rhs)
{
	V4_F32 result;
	#ifdef USE_SSE
		result.SSE = _mm_add_ps(lhs.SSE, rhs.SSE);
	#else
		result.x = lhs.x + rhs.x;
  result.y = lhs.y + rhs.y;
  result.z = lhs.z + rhs.z;
  result.w = lhs.w + rhs.w;
	#endif
		
		return result;
}

internal inline V4_F32
sub_v4(V4_F32 lhs, V4_F32 rhs)
{
	V4_F32 result;
	#ifdef USE_SSE
		result.SSE = _mm_sub_ps(lhs.SSE, rhs.SSE);
	#else
		result.x = lhs.x - rhs.x;
  result.y = lhs.y - rhs.y;
  result.z = lhs.z - rhs.z;
  result.w = lhs.w - rhs.w;
	#endif
		
		return result;
}

internal f32
length_v2(V2_F32 v)
{
	f32 result = sqrtf(v.x * v.x + v.y * v.y);
	return result;
}

internal b32
equal_v2(V2_F32 a, V2_F32 b)
{
	b32 result = a.x == b.x && a.y == b.y;
	return result;
}

internal inline V2_F32
operator+(V2_F32 lhs, V2_F32 rhs)
{
	return add_v2(lhs, rhs);
}

internal b32
operator==(V2_F32 lhs, V2_F32 rhs)
{
	return equal_v2(lhs, rhs);
}

internal b32
operator!=(V2_F32 lhs, V2_F32 rhs)
{
	return !equal_v2(lhs, rhs);
}

internal inline V2_F32 &
operator+=(V2_F32 &lhs, V2_F32 rhs)
{
	return lhs = lhs + rhs;
}

internal inline V2_F32
operator-(V2_F32 lhs, V2_F32 rhs)
{
	return sub_v2(lhs, rhs);
}

internal inline V2_F32 &
operator-=(V2_F32 &lhs, V2_F32 rhs)
{
	return lhs = lhs - rhs;
}


internal inline V4_F32
operator+(V4_F32 lhs, V4_F32 rhs)
{
	return add_v4(lhs, rhs);
}

internal inline V4_F32 &
operator+=(V4_F32 &lhs, V4_F32 rhs)
{
	return lhs = lhs + rhs;
}

internal inline V4_F32
operator-(V4_F32 lhs, V4_F32 rhs)
{
	return sub_v4(lhs, rhs);
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
mul_v4(V4_F32 lhs, f32 v)
{
  V4_F32 result;
  
	#ifdef USE_SSE
		__m128 scalar = _mm_set1_ps(v);
  result.SSE = _mm_mul_ps(lhs.SSE, scalar);
	#else
		result.x = Left.x * Right;
  result.y = Left.y * Right;
  result.z = Left.z * Right;
  result.w = Left.w * Right;
	#endif
		
		return result;
}

internal inline V4_F32
mul_v4(V4_F32 lhs, V4_F32 rhs)
{
  V4_F32 result;
  
	#ifdef USE_SSE
		result.SSE = _mm_mul_ps(lhs.SSE, rhs.SSE);
	#else
		result.x = lhs.x * rhs.x;
  result.y = lhs.y * rhs.y;
  result.z = lhs.z * rhs.z;
  result.w = lhs.w * rhs.w;
	#endif
		
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

internal inline V4_F32
operator*(V4_F32 lhs, V4_F32 rhs)
{
	return mul_v4(lhs, rhs);
}

internal inline V4_F32
operator*(V4_F32 lhs, f32 v)
{
	return mul_v4(lhs, v);
}

internal inline V4_F32
operator*(f32 v, V4_F32 rhs)
{
	return mul_v4(rhs, v);
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
range(f32 min_x, f32 max_x, f32 min_y, f32 max_y)
{
	Range2_F32 result;
	result.min_x = min_x;
	result.max_x = max_x;
	result.min_y = min_y;
	result.max_y = max_y;
	return result;
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

internal inline Range2_F32 
range_topleft_dim(V2_F32 topleft, V2_F32 dim)
{
  Range2_F32 result = range_min_max(vec2(topleft.x, topleft.y - dim.height), vec2(topleft.x + dim.width, topleft.y));
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



internal Range2_F32_Cut
range_cut_top(Range2_F32 rect, f32 cut_h)
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
range_cut_bottom(Range2_F32 rect, f32 cut_h)
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
range_cut_left(Range2_F32 rect, f32 cut_w)
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
range_cut_right(Range2_F32 rect, f32 cut_w)
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

internal Range2_F32_Cut
range_cut_top_percentage(Range2_F32 rect, f32 h_percent)
{
	assert(0 <= h_percent && h_percent <= 1);
	Range2_F32_Cut cut = range_cut_top(rect, h_percent * range_dim(rect).height);
	return cut;
}

internal Range2_F32_Cut
range_cut_bottom_percentage(Range2_F32 rect, f32 h_percent)
{
	assert(0 <= h_percent && h_percent <= 1);
	Range2_F32_Cut cut = range_cut_bottom(rect, h_percent * range_dim(rect).height);
	return cut;
}

internal Range2_F32_Cut
range_cut_left_percentage(Range2_F32 rect, f32 w_percent)
{
	assert(0 <= w_percent && w_percent <= 1);
	Range2_F32_Cut cut = range_cut_left(rect, w_percent * range_dim(rect).width);
	return cut;
}

internal Range2_F32_Cut
range_cut_right_percentage(Range2_F32 rect, f32 w_percent)
{
	assert(0 <= w_percent && w_percent <= 1);
	Range2_F32_Cut cut = range_cut_right(rect, w_percent * range_dim(rect).width);
	return cut;
}


internal inline b32
is_in_range(Range2_F32 range, V2_F32 p)
{
  b32 result = ((p.x >= range.minp.x && p.x < range.maxp.x) &&
    (p.y >= range.minp.y && p.y < range.maxp.y));
  return result;
}

internal inline b32
is_range_intersect(Range2_F32 range1, Range2_F32 range2)
{
  b32 result = !(range1.max_x < range2.min_x ||
    range2.max_x < range1.min_x ||
    range1.max_y < range2.min_y ||
    range2.max_y < range1.min_y
  );
  return result;
}

internal Range2_F32
range_intersection(Range2_F32 range1, Range2_F32 range2)
{
	Range2_F32 result;
	result.min_y = MAX(range1.min_y, range2.min_y);
	result.max_y = MIN(range1.max_y, range2.max_y);
	
	result.min_x = MAX(range1.min_x, range2.min_x);
	result.max_x = MIN(range1.max_x, range2.max_x);
	return result;
}

internal Range2_F32
range_union(Range2_F32 range1, Range2_F32 range2)
{
	Range2_F32 result;
	result.min_y = MAX(range1.min_y, range2.min_y);
	result.max_y = MIN(range1.max_y, range2.max_y);
	
	result.min_x = MAX(range1.min_x, range2.min_x);
	result.max_x = MIN(range1.max_x, range2.max_x);
	return result;
}

internal inline f32
map_into_range_zo(f32 min, f32 value, f32 max)
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
map_into_range_zo(f64 min, f64 value, f64 max)
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
map_into_range_no(f32 min, f32 value, f32 max)
{
  f32 result = -1.0f + 2.0f * map_into_range_zo(min, value, max);
  return result;
}

internal inline f32
map_into_range(f32 value, f32 old_min, f32 old_max, f32 new_min, f32 new_max)
{
	value = CLAMP(old_min, value, old_max);
	f32 result = new_min + map_into_range_zo(old_min, value, old_max) * (new_max - new_min);
	return result;
}

internal inline f32
lerp(f32 a, f32 t, f32 b)
{
	f32 result = (1 - t) * a + t * b;
	return result;
}

internal inline V4_F32
lerp(V4_F32 a, f32 t, V4_F32 b)
{
	V4_F32 result = (1 - t) * a + t * b;
	return result;
}

internal inline V2_F32
lerp(V2_F32 a, f32 t, V2_F32 b)
{
	V2_F32 result = (1 - t) * a + t * b;
	return result;
}

#endif //MPLAYER_MATH_H
