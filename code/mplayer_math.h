/* date = October 12th 2024 4:28 pm */

#ifndef MPLAYER_MATH_H
#define MPLAYER_MATH_H

#include <xmmintrin.h>

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
vec2(V2_I32 v)
{
	V2_F32 result;
	result.x = f32(v.x);
	result.y = f32(v.y);
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

internal V2_I32
range_dim(Range2_I32 range)
{
	V2_I32 result;
	result = vec2i(ABS(range.max_x - range.min_x), 
		ABS(range.max_y - range.min_y)
	);
	return result;
}

internal inline i32
round_f32_i32(f32 x)
{
  i32 result = _mm_cvtss_si32(_mm_set_ss(x));
  return result;
}


#endif //MPLAYER_MATH_H
