/*
  HandmadeMh.h v2.0.0

  This is a single header file with a bunch of useful types and functions for
  games and graphics. Consider it a lightweight alternative to GLM that works
  both C and C++.

  =============================================================================
  CONFIG
  =============================================================================

  By default, all angles in Handmade Mh are specified in radians. However, it
  can be configured to use degrees or turns instead. Use one of the following
  defines to specify the default unit for angles:

    #define HANDMADE_MATH_USE_RADIANS
    #define HANDMADE_MATH_USE_DEGREES
    #define HANDMADE_MATH_USE_TURNS

  Regardless of the default angle, you can use the following functions to
  specify an angle in a particular unit:

    AngleRad(radians)
    AngleDeg(degrees)
    AngleTurn(turns)

  The definitions of these functions change depending on the default unit.

  -----------------------------------------------------------------------------

  Handmade Mh ships with SSE (SIMD) implementations of several common
  operations. To disable the use of SSE intrinsics, you must define
  HANDMADE_MATH_no_SSE before including this file:

    #define HANDMADE_MATH_no_SSE
    #include "HandmadeMh.h"

  -----------------------------------------------------------------------------

  To use Handmade Mh without the C runtime library, you must provide your own
  implementations of basic math functions. Otherwise, HandmadeMh.h will use
  the runtime library implementation of these functions.

  Define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS and provide your own
  implementations of SINF, COSF, TANF, ACOSF, and SQRTF
  before including HandmadeMh.h, like so:

    #define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS
    #define SINF MySinF
    #define COSF MyCosF
    #define TANF MyTanF
    #define ACOSF MyACosF
    #define SQRTF MySqrtF
    #include "HandmadeMh.h"

  By default, it is assumed that your math functions take radians. To use
  different units, you must define ANGLE_USER_TO_INTERNAL and
  ANGLE_INTERNAL_TO_USER. For example, if you want to use degrees in your
  code but your math functions use turns:

    #define ANGLE_USER_TO_INTERNAL(a) ((a)*DegToTurn)
    #define ANGLE_INTERNAL_TO_USER(a) ((a)*TurnToDeg)

  =============================================================================

  LICENSE

  This software is in the public domain. Where that dedication is not
  recognized, you are granted a perpetual, irrevocable license to copy,
  distribute, and modify this file as you see fit.

  =============================================================================

  CREDITS

  Originally written by Zakary Strange.

  Functionality:
   Zakary Strange (strangezak@protonmail.com && @strangezak)
   Mt Mascarenhas (@miblo_)
   Aleph
   FieryDrake (@fierydrake)
   Gingerbill (@TheGingerBill)
   Ben Visness (@bvisness)
   Trinton Bullard (@Peliex_Dev)
   @AntonDan
   Logan Forman (@dev_dwarf)

  Fixes:
   Jeroen van Rijn (@J_vanRijn)
   Kiljacken (@Kiljacken)
   Insofaras (@insofaras)
   Daniel Gibson (@DanielGibson)
*/

#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

#define SQUARE(x) ((x) * (x))
#define CLAMP(a, x, b) (((x) <= (a))? (a) : (((b) <= (x))? (b):(x)))
#define MAX(a, b) (((a) >= (b))? (a):(b))
#define MIN(a, b) (((a) <= (b))? (a):(b))
#define ABS(a) (((a) >= 0)? (a) : -(a))

#define PI 3.14159265358979323846
#define PI32 3.14159265359f

#ifdef HANDMADE_MATH_no_SSE
# warning "HANDMADE_MATH_no_SSE is deprecated, use HANDMADE_MATH_no_SIMD instead"
# define HANDMADE_MATH_no_SIMD
#endif 

/* let's figure out if SSE is really available (unless disabled anyway)
   (it isn't on non-x86/x86_64 platforms or even x86 without explicit SSE support)
   => only use "#ifdef HANDMADE_MATH__USE_SSE" to check for SSE support below this block! */
#ifndef HANDMADE_MATH_no_SIMD
# ifdef COMPILER_CL
/* MSVC supports SSE in amd64 mode or _M_IX86_FP >= 1 (2 means SSE2) */
#  if defined(ARCH_X64) || ( defined(_M_IX86_FP) && _M_IX86_FP >= 1 )
#   define HANDMADE_MATH__USE_SSE 1
#  endif
# else /* not MSVC, probably GCC, clang, icc or something that doesn't support SSE anyway */
#  ifdef __SSE__ /* they #define __SSE__ if it's supported */
#   define HANDMADE_MATH__USE_SSE 1
#  endif /*  __SSE__ */
# endif /*   */
# ifdef __ARM_NEON
#  define HANDMADE_MATH__USE_NEON 1
# endif /* NEON Supported */
#endif /* #ifndef HANDMADE_MATH_no_SIMD */

#if (LANG_C && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
# define HANDMADE_MATH__USE_C11_GENERICS 1
#endif

#ifdef HANDMADE_MATH__USE_SSE
# include <xmmintrin.h>
#endif

#ifdef HANDMADE_MATH__USE_NEON
# include <arm_neon.h>
#endif

#if COMPILER_GCC || COMPILER_CLANG
# define DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif COMPILER_CL
# define DEPRECATED(msg) __declspec(deprecated(msg))
#else
# define DEPRECATED(msg)
#endif

#if !defined(HANDMADE_MATH_USE_DEGREES) \
&& !defined(HANDMADE_MATH_USE_TURNS) \
&& !defined(HANDMADE_MATH_USE_RADIANS)
# define HANDMADE_MATH_USE_RADIANS
#endif

#define PI 3.14159265358979323846
#define PI32 3.14159265359f
#define DEG180 180.0
#define DEG18032 180.0f
#define TURNHALF 0.5
#define TURNHALF32 0.5f
#define RadToDeg ((f32)(DEG180/PI))
#define RadToTurn ((f32)(TURNHALF/PI))
#define DegToRad ((f32)(PI/DEG180))
#define DegToTurn ((f32)(TURNHALF/DEG180))
#define TurnToRad ((f32)(PI/TURNHALF))
#define TurnToDeg ((f32)(DEG180/TURNHALF))

#if defined(HANDMADE_MATH_USE_RADIANS)
# define AngleRad(a) (a)
# define AngleDeg(a) ((a)*DegToRad)
# define AngleTurn(a) ((a)*TurnToRad)
#elif defined(HANDMADE_MATH_USE_DEGREES)
# define AngleRad(a) ((a)*RadToDeg)
# define AngleDeg(a) (a)
# define AngleTurn(a) ((a)*TurnToDeg)
#elif defined(HANDMADE_MATH_USE_TURNS)
# define AngleRad(a) ((a)*RadToTurn)
# define AngleDeg(a) ((a)*DegToTurn)
# define AngleTurn(a) (a)
#endif

#if !defined(HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS)
# include <math.h>
# define SINF sinf
# define COSF cosf
# define TANF tanf
# define SQRTF sqrtf
# define ACOSF acosf
#endif

#if !defined(ANGLE_USER_TO_INTERNAL)
# define ANGLE_USER_TO_INTERNAL(a) (to_rad(a))
#endif

#if !defined(ANGLE_INTERNAL_TO_USER)
# if defined(HANDMADE_MATH_USE_RADIANS)
#  define ANGLE_INTERNAL_TO_USER(a) (a)
# elif defined(HANDMADE_MATH_USE_DEGREES)
#  define ANGLE_INTERNAL_TO_USER(a) ((a)*RadToDeg)
# elif defined(HANDMADE_MATH_USE_TURNS)
#  define ANGLE_INTERNAL_TO_USER(a) ((a)*RadToTurn)
# endif
#endif

typedef union V2_F32 V2_F32;
union V2_F32
{
  struct
  {
    f32 x, y;
  };
  
  struct
  {
    f32 u, v;
  };
  
  struct
  {
    f32 left, right;
  };
  
  struct
  {
    f32 width, height;
  };
  
  f32 elements[2];
  f32 e[2];
  
	#ifdef __cplusplus
		inline f32 &operator[](int Index) { return elements[Index]; }
  inline const f32& operator[](int Index) const { return elements[Index]; }
	#endif
};

typedef union V2_I32 V2_I32;
union V2_I32
{
  struct
  {
    i32 x, y;
  };
  
  struct
  {
    i32 u, v;
  };
  
  struct
  {
    i32 left, right;
  };
  
  struct
  {
    i32 width, height;
  };
  
  i32 elements[2];
  
	#ifdef __cplusplus
		inline i32 &operator[](int Index) { return elements[Index]; }
  inline const i32& operator[](int Index) const { return elements[Index]; }
	#endif
};

typedef union V3_F32 V3_F32;
union V3_F32
{
  struct
  {
    f32 x, y, z;
  };
  
  struct
  {
    f32 u, v, w;
  };
  
  struct
  {
    f32 r, g, b;
  };
  
  struct
  {
    V2_F32 xy;
    f32 _Ignored0;
  };
  
  struct
  {
    f32 _Ignored1;
    V2_F32 yz;
  };
  
  struct
  {
    V2_F32 uv;
    f32 _Ignored2;
  };
  
  struct
  {
    f32 _Ignored3;
    V2_F32 vw;
  };
  
	struct {f32 pitch, yaw, roll;};
	
	f32 elements[3];
  
	#ifdef __cplusplus
		inline f32 &operator[](int Index) { return elements[Index]; }
  inline const f32 &operator[](int Index) const { return elements[Index]; }
	#endif
};

typedef union V4_F32 V4_F32;
union V4_F32
{
  struct
  {
    union
    {
      V3_F32 xyz;
      struct
      {
        f32 x, y, z;
      };
    };
    
    f32 w;
  };
  struct
  {
    union
    {
      V3_F32 rgb;
      struct
      {
        f32 r, g, b;
      };
    };
    
    f32 a;
  };
  
  struct
  {
    V2_F32 xy;
    f32 _Ignored0;
    f32 _Ignored1;
  };
  
  struct
  {
    f32 _Ignored2;
    V2_F32 yz;
    f32 _Ignored3;
  };
  
  struct
  {
    f32 _Ignored4;
    f32 _Ignored5;
    V2_F32 zw;
  };
  
  f32 elements[4];
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSE;
	#endif
		
	#ifdef HANDMADE_MATH__USE_NEON
		f3232x4_t NEON;
	#endif 
		
	#ifdef __cplusplus
		inline f32 &operator[](int Index) { return elements[Index]; }
  inline const f32 &operator[](int Index) const { return elements[Index]; }
	#endif
};

typedef union M2 M2;
union M2
{
  f32 elements[2][2];
  V2_F32 columns[2];
  
	#ifdef __cplusplus
		inline V2_F32 &operator[](int Index) { return columns[Index]; }
  inline const V2_F32 &operator[](int Index) const { return columns[Index]; }
	#endif
};

typedef union M3 M3;
union M3
{
  f32 elements[3][3];
  V3_F32 columns[3];
  
	#ifdef __cplusplus
		inline V3_F32 &operator[](int Index) { return columns[Index]; }
  inline const V3_F32 &operator[](int Index) const { return columns[Index]; }
	#endif
};

typedef union M4 M4;
union M4
{
  f32 elements[4][4];
  V4_F32 columns[4];
  
	#ifdef __cplusplus
		inline V4_F32 &operator[](int Index) { return columns[Index]; }
  inline const V4_F32 &operator[](int Index) const { return columns[Index]; }
	#endif
};

struct M4_Inv
{
	M4 mat;
	M4 inv;
};

typedef union Quat Quat;
union Quat
{
  struct
  {
    union
    {
      V3_F32 xyz;
      struct
      {
        f32 x, y, z;
      };
    };
    
    f32 w;
  };
  
  f32 elements[4];
  
	#ifdef HANDMADE_MATH__USE_SSE
		__m128 SSE;
	#endif
	#ifdef HANDMADE_MATH__USE_NEON
		f3232x4_t NEON;
	#endif
};


typedef union Range1_F32 Range1_F32;
union Range1_F32
{
	struct {f32 min_x, max_x;};
};

typedef union Range2_I32 Range2_I32;
union Range2_I32
{
	struct {V2_I32 minp, maxp;};
	struct {i32 min_x, min_y, max_x, max_y;};
};

typedef union Range2_F32 Range2_F32;
union Range2_F32
{
	struct {V2_F32 minp, maxp;};
	struct {f32 min_x, min_y, max_x, max_y;};
};

typedef union Range2_F32_Cut Range2_F32_Cut;
union Range2_F32_Cut
{
	Range2_F32 e[2];
	struct { Range2_F32 min, max;};
	struct { Range2_F32 left, right;};
	struct { Range2_F32 top, bottom;};
};

#define RANGE2_F32_EMPTY ZERO_STRUCT
#define RANGE2_F32_FULL_ZO Range2_F32{.min_x = 0, .min_y = 0, .max_x = 1, .max_y = 1}

internal inline f32 to_rad(f32 Angle);
internal inline f32 to_deg(f32 Angle);
internal inline f32 to_turn(f32 Angle);
internal inline f32 sin_f(f32 Angle);
internal inline f32 cos_f(f32 Angle);
internal inline f32 tan_f(f32 Angle);
internal inline f32 acos_f(f32 Arg);
internal inline f32 pow_f(f32 base, f32 exp);
internal inline f32 sqrt_f(f32 Float);
internal inline f32 inv_sqrt_f(f32 Float);
internal void accelerate_towards(V3_F32 *pos, V3_F32 *vel, V3_F32 target_pos, f32 freq, f32 zeta, f32 dt);
internal inline f32 lerpf(f32 A, f32 Time, f32 B);
internal inline f32 clamp(f32 Min, f32 Value, f32 Max);
internal inline f32 map_into_range(f32 value, f32 old_min, f32 old_max, f32 new_min, f32 new_max);
internal inline f32 map_into_range01(f32 Min, f32 Value, f32 Max);
internal inline f64 map_into_range01_f64(f64 Min, f64 Value, f64 Max);
internal inline i32 round_f32_i32(f32 Real32);
internal inline u32 round_f32_u32(f32 Real32);
internal inline i32 trancuate_f32_i32(f32 Real32);


internal inline V2_F32 v2(f32 X, f32 Y);
internal inline V2_F32 v2d(f32 v);
internal inline V2_F32 v2vi(V2_I32 v);
internal inline V2_I32 v2i(i32 X, i32 Y);
internal inline V2_I32 v2iv(V2_F32 v);
internal inline V3_F32 v3(f32 x, f32 y, f32 z);
internal inline V3_F32 v3d(f32 v);
internal inline V3_F32 v3v(V2_F32 xy, f32 z);
internal inline V4_F32 v4(f32 X, f32 Y, f32 Z, f32 W);
internal inline V4_F32 v4d(f32 X, f32 Y, f32 Z, f32 W);
internal inline V4_F32 v4v(V3_F32 v, f32 w);


internal inline V2_F32 add_v2(V2_F32 Left, V2_F32 Right);
internal inline V3_F32 add_v3(V3_F32 Left, V3_F32 Right);
internal inline V4_F32 add_v4(V4_F32 Left, V4_F32 Right);
internal inline V2_F32 sub_v2(V2_F32 Left, V2_F32 Right);
internal inline V3_F32 sub_v3(V3_F32 Left, V3_F32 Right);
internal inline V4_F32 sub_v4(V4_F32 Left, V4_F32 Right);
internal inline V2_F32 mul_v2(V2_F32 Left, V2_F32 Right);
internal inline V2_F32 mul_v2f(V2_F32 Left, f32 Right);
internal inline V3_F32 mul_v3(V3_F32 Left, V3_F32 Right);
internal inline V3_F32 mul_v3f(V3_F32 Left, f32 Right);
internal inline V4_F32 mul_v4(V4_F32 Left, V4_F32 Right);
internal inline V4_F32 mul_v4f(V4_F32 Left, f32 Right);
internal inline V2_F32 div_v2(V2_F32 Left, V2_F32 Right);
internal inline V2_F32 div_v2f(V2_F32 Left, f32 Right);
internal inline V3_F32 div_v3(V3_F32 Left, V3_F32 Right);
internal inline V3_F32 div_v3f(V3_F32 Left, f32 Right);
internal inline V4_F32 div_v4(V4_F32 Left, V4_F32 Right);
internal inline V4_F32 div_v4f(V4_F32 Left, f32 Right);
internal inline b32 eq_v2(V2_F32 Left, V2_F32 Right);
internal inline b32 eq_v3(V3_F32 Left, V3_F32 Right);
internal inline b32 eq_v4(V4_F32 Left, V4_F32 Right);
internal inline f32 dotv2(V2_F32 Left, V2_F32 Right);
internal inline f32 dotv3(V3_F32 Left, V3_F32 Right);
internal inline f32 dotv4(V4_F32 Left, V4_F32 Right);
internal inline V3_F32 cross_v3(V3_F32 Left, V3_F32 Right);
internal inline f32 len_sqr_v2(V2_F32 A);
internal inline f32 len_sqr_v3(V3_F32 A);
internal inline f32 len_sqr_v4(V4_F32 A);
internal inline f32 len_v2(V2_F32 A);
internal inline f32 len_v3(V3_F32 A);
internal inline f32 len_v4(V4_F32 A);
internal inline V2_F32 v2_normalize(V2_F32 A);
internal inline V3_F32 v3_normalize(V3_F32 A);
internal inline V4_F32 v4_normalize(V4_F32 A);
internal inline V3_F32 compute_plane_normal(V3_F32 v1, V3_F32 v2, V3_F32 v3);
internal inline V2_F32 lerp_v2(V2_F32 A, f32 Time, V2_F32 B);
internal inline V3_F32 lerp_v3(V3_F32 A, f32 Time, V3_F32 B);
internal inline V4_F32 lerp_v4(V4_F32 A, f32 Time, V4_F32 B);
internal inline V4_F32 linear_combine_v4m4(V4_F32 Left, M4 Right);
internal inline M2 m2(void);
internal inline M2 m2d(f32 Diagonal);
internal inline M2 transpose_m2(M2 matrix);
internal inline M2 add_m2(M2 Left, M2 Right);
internal inline M2 sub_m2(M2 Left, M2 Right);
internal inline V2_F32 mul_m2v2(M2 matrix, V2_F32 Vector);
internal inline M2 mul_m2(M2 Left, M2 Right);
internal inline M2 mul_m2f(M2 matrix, f32 Scalar);
internal inline M2 div_m2f(M2 matrix, f32 Scalar);
internal inline f32 determinant_m2(M2 matrix);
internal inline M2 inv_general_m2(M2 matrix);
internal inline M3 m3(void);
internal inline M3 m3d(f32 Diagonal);
internal inline M3 transpose_m3(M3 matrix);
internal inline M3 add_m3(M3 Left, M3 Right);
internal inline M3 sub_m3(M3 Left, M3 Right);
internal inline V3_F32 mul_m3v3(M3 matrix, V3_F32 Vector);
internal inline M3 mul_m3(M3 Left, M3 Right);
internal inline M3 mul_m3f(M3 matrix, f32 Scalar);
internal inline M3 div_m3f(M3 matrix, f32 Scalar);
internal inline f32 determinant_m3(M3 matrix);
internal inline M3 inv_general_m3(M3 matrix);
internal inline M4 m4(void);
internal inline M4 m4d(f32 Diagonal);
internal inline M4 transpose_m4(M4 matrix);
internal inline M4 add_m4(M4 Left, M4 Right);
internal inline M4 sub_m4(M4 Left, M4 Right);
internal inline M4 mul_m4(M4 Left, M4 Right);
internal inline M4 mul_m4f(M4 matrix, f32 Scalar);
internal inline V4_F32 mul_m4v4(M4 matrix, V4_F32 Vector);
internal inline M4 div_m4f(M4 matrix, f32 Scalar);
internal inline f32 determinant_m4(M4 matrix);
internal inline M4 inv_general_m4(M4 matrix);
internal inline M4 m4_ortho3d_rh_no(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far);
internal inline M4 m4_ortho3d_rh_zo(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far);
internal inline M4 m4_ortho3d_lh_no(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far);
internal inline M4 m4_ortho3d_lh_zo(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far);
internal inline M4 m4_inv_ortho(M4 Orthomatrix);
internal inline M4 m4_perspective_rh_no(f32 FOV, f32 AspectRatio, f32 Near, f32 Far);
internal inline M4 m4_perspective_rh_zo(f32 FOV, f32 AspectRatio, f32 Near, f32 Far);
internal inline M4 m4_perspective_lh_no(f32 FOV, f32 AspectRatio, f32 Near, f32 Far);
internal inline M4 m4_perspective_lh_zo(f32 FOV, f32 AspectRatio, f32 Near, f32 Far);
internal inline M4 m4_inv_perspective_rh(M4 m4_perspectivematrix);
internal inline M4 m4_inv_perspective_lh(M4 m4_perspectivematrix);
internal inline M4 m4_translate(V3_F32 Translation);
internal inline M4 m4_inv_translate(M4 Translationmatrix);
internal inline M4 m4_rotate_rh(f32 Angle, V3_F32 Axis);
internal inline M4 m4_rotate_lh(f32 Angle, V3_F32 Axis);
internal inline M4 m4_inv_rotate(M4 Rotationmatrix);
internal inline M4 m4_scale_m(V3_F32 scale);
internal inline M4 m4_inv_scale(M4 scalematrix);
internal inline M4 _look_at(V3_F32 F,  V3_F32 S, V3_F32 U,  V3_F32 Eye);
internal inline M4 look_at_rh(V3_F32 Eye, V3_F32 Center, V3_F32 Up);
internal inline M4 look_at_lh(V3_F32 Eye, V3_F32 Center, V3_F32 Up);
internal inline M4 inv_look_at(M4 matrix);
internal inline Quat quat(f32 X, f32 Y, f32 Z, f32 W);
internal inline Quat quat_v4(V4_F32 Vector);
internal inline Quat add_q(Quat Left, Quat Right);
internal inline Quat sub_q(Quat Left, Quat Right);
internal inline Quat mul_q(Quat Left, Quat Right);
internal inline Quat mul_qf(Quat Left, f32 multiplicative);
internal inline Quat div_qf(Quat Left, f32 divnd);
internal inline f32 dot_q(Quat Left, Quat Right);
internal inline Quat inv_q(Quat Left);
internal inline Quat q_normalize(Quat q);
internal inline Quat _mix_q(Quat Left, f32 MixLeft, Quat Right, f32 MixRight);
internal inline Quat nlerp(Quat Left, f32 Time, Quat Right);
internal inline Quat slerp(Quat Left, f32 Time, Quat Right);
internal inline M4 q_to_m4(Quat Left);
internal inline Quat m4_to_q_rh(M4 M);
internal inline Quat m4_to_q_lh(M4 M);
internal inline Quat quat_from_axis_angle_rh(V3_F32 Axis, f32 Angle);
internal inline Quat quat_from_axis_angle_lh(V3_F32 Axis, f32 Angle);
internal inline V2_F32 rotateV2(V2_F32 V, f32 Angle);
internal inline V3_F32 rotate_v3_q(V3_F32 V, Quat Q);
internal inline V3_F32 rotatev3_axis_angle_lh(V3_F32 V, V3_F32 Axis, f32 Angle);
internal inline V3_F32 rotatev3_axis_angle_rh(V3_F32 V, V3_F32 Axis, f32 Angle);

internal inline Range2_F32 range2f32(f32 min_x, f32 max_x, f32 min_y, f32 max_y);
internal inline Range2_F32 range2f32_min_max(V2_F32 min_p, V2_F32 max_p);
internal inline Range2_F32 range2f32_center_dim(V2_F32 center, V2_F32 dim);
internal inline V2_I32 range2i32_dim(Range2_I32 range);
internal inline V2_F32 range2f32_dim(Range2_F32 range);
internal inline V2_F32 range2f32_center(Range2_F32 range);
internal Range2_F32_Cut range2f32_cut_top(Range2_F32 rect, f32 cut_h);
internal Range2_F32_Cut range2f32_cut_bottom(Range2_F32 rect, f32 cut_h);
internal Range2_F32_Cut range2f32_cut_left(Range2_F32 rect, f32 cut_w);
internal Range2_F32_Cut range2f32_cut_right(Range2_F32 rect, f32 cut_w);

internal inline b32 range2f32_is_inside(Range2_F32 range, V2_F32 p);
internal inline b32 range2f32_check_intersect(Range2_F32 range1, Range2_F32 range2);
internal Range2_F32 range2f32_intersection(Range2_F32 range1, Range2_F32 range2);


#endif /* HANDMADE_MATH_H */



