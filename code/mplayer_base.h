/* date = October 11th 2024 9:01 pm */

#ifndef MPLAYER_BASE_H
#define MPLAYER_BASE_H

#include <stdio.h>
#include <stdint.h>

#include <string.h>
#include <stdlib.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float  f32;
typedef double f64;
typedef int32_t b32;
typedef size_t umem;
typedef void void_function();

#define internal      static
#define global        static
#define local_persist static

#define fallthrough /*fallthrough*/

#if defined(_MSC_VER) 
# define COMPILER_CL 1
#endif

#if defined(_WIN32) 
# define OS_WINDOWS 1
#endif

#if defined(__cplusplus)
# define LANG_CPP 1
#endif

#ifndef OS_WINDOWS
# define OS_WINDOWS 0
#endif

#ifndef LANG_CPP
# define LANG_CPP 0
#endif

#ifndef COMPILER_CL
# define COMPILER_CL 0
#endif

#if COMPILER_CL
# define assert_break __debugbreak()
#else 
# define assert_break *((u32*)0) = (u32)0xdeadbeef
#endif

#define assert(cond) do {if (!(cond)) { assert_break; }} while(0)
#define invalid_default_case default: invalid_code_path()
#define invalid_code_path(...) assert(!"invalid code path!!")
#define not_impemeneted(...) assert(!"not yet implemented!!")

#define ptr_from_int(i) (void*)(((u8*)0) + (i))
#define int_from_ptr(p) (u64)(((u8*)(p)) - 0)
#define align_forward(n, a) (((n) + (a) - 1) & ~((a) - 1))


#define kilobytes(x) (u64(x) << 10)
#define megabytes(x) (u64(x) << 20)
#define gigabytes(x) (u64(x) << 30)

#define ZERO_STRUCT {}

#define memory_set(p, v, len) memset(p, v, len)
#define memory_copy(dst, src, sz) memcpy(dst, src, sz)
#define memory_zero(p, s) memory_set(p, 0, s)
#define memory_zero_array(p) memory_zero(p, sizeof(p))
#define memory_zero_struct(p) memory_zero(p, sizeof(*(p)))

#define array_count(a) (sizeof(a) / sizeof((a)[0]))
#define has_flag(flags, f) ((flags) & (1 << (f)))
#define set_flag(flags, f) ((flags) |= (1 << (f)))

#define member(T, m) (((T*)0)->m)
#define member_offset(T, m) int_from_ptr(&member(T, m))
#define cast_from_member(ptr, T, m) (T*)(((u8*)(ptr)) - member_offset(T, m))

#define SWAP(T,a,b) do {T t__ = a; a = b; b = t__;} while(0)

#define ENABLE_LOGS 1

#if ENABLE_LOGS
#define Log(...)         _debug_log(0,           __FILE__, __LINE__, __VA_ARGS__)
#define LogWarning(...)  _debug_log(Log_Warning, __FILE__, __LINE__, __VA_ARGS__)
#define LogError(...)    _debug_log(Log_Error,   __FILE__, __LINE__, __VA_ARGS__)
#else
#define Log(...)
#define LogWarning(...)
#define LogError(...)
#endif

#define Log_Warning (1<<0)
#define Log_Error   (1<<1)

internal void _debug_log(i32 flags, const char *file, int line, const char *format, ...);
#endif //MPLAYER_BASE_H
