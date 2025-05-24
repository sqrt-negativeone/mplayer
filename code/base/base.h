/* date = October 11th 2024 9:01 pm */

#ifndef HAZE_BASE_H
#define HAZE_BASE_H

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
typedef int16_t b16;
typedef int32_t b32;
typedef int8_t  b8;
typedef size_t umem;
typedef void *Address;
typedef void void_function();
struct u128 {u64 v[2];};

//- NOTE(fakhri): compilaitn context detection

#if defined(__clang__)
# define COMPILER_CLANG 1

# if defined(__APPLE__) && defined(__MACH__)
#  define OS_MAC 1
# elif defined(__gnu_linux__)
#  define OS_LINUX 1
# elif defined(__EMSCRIPTEN__)
#  define OS_WASM 1
# elif defined(_WIN32)
#  define OS_WINDOWS 1
#  include <windows.h>
#  undef near
#  undef far
# else
#  error compiler/platform combo is not supported yet
# endif

# if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#  define ARCH_X64 1
# elif defined(i386) || defined(__i386) || defined(__i386__)
#  define ARCH_X86 1
# elif defined(__aarch64__)
#  define ARCH_ARM64 1
# elif defined(__arm__)
#  define ARCH_ARM32 1
# elif OS_WASM
#  define ARCH_WEBASSMEBLY 1
# else
#  error architecture not supported yet
# endif

#elif defined(_MSC_VER)
# define COMPILER_CL 1

# if defined(_WIN32)
#  define OS_WINDOWS 1
#  include <windows.h>
#  undef near
#  undef far
# else
#  error compiler/platform combo is not supported yet
# endif

# if defined(_M_AMD64)
#  define ARCH_X64 1
# elif defined(_M_IX86)
#  define ARCH_X86 1
# elif defined(_M_ARM64)
#  define ARCH_ARM64 1
# elif defined(_M_ARM)
#  define ARCH_ARM32 1
# else
#  error architecture not supported yet
# endif

#if _MSC_VER >= 1920
#define COMPILER_CL_YEAR 2019
#elif _MSC_VER >= 1910
#define COMPILER_CL_YEAR 2017
#elif _MSC_VER >= 1900
#define COMPILER_CL_YEAR 2015
#elif _MSC_VER >= 1800
#define COMPILER_CL_YEAR 2013
#elif _MSC_VER >= 1700
#define COMPILER_CL_YEAR 2012
#elif _MSC_VER >= 1600
#define COMPILER_CL_YEAR 2010
#elif _MSC_VER >= 1500
#define COMPILER_CL_YEAR 2008
#elif _MSC_VER >= 1400
#define COMPILER_CL_YEAR 2005
#else
#define COMPILER_CL_YEAR 0
#endif

#elif defined(__GNUC__) || defined(__GNUG__)

# define COMPILER_GCC 1
# if defined(__gnu_linux__)
#  define OS_LINUX 1
# else
#  error compiler/platform combo is not supported yet
# endif

# if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#  define ARCH_X64 1
# elif defined(i386) || defined(__i386) || defined(__i386__)
#  define ARCH_X86 1
# elif defined(__aarch64__)
#  define ARCH_ARM64 1
# elif defined(__arm__)
#  define ARCH_ARM32 1
# else
#  error architecture not supported yet
# endif

#else
# error compiler is not supported yet
#endif

#if defined(ARCH_X64)
# define ARCH_64BIT 1
#elif defined(ARCH_X86)
# define ARCH_32BIT 1

#endif

#if defined(__cplusplus)
#define LANG_CPP 1
#else
#define LANG_C 1
#endif

// zeroify

#if !defined(ARCH_32BIT)
#define ARCH_32BIT 0
#endif
#if !defined(ARCH_64BIT)
#define ARCH_64BIT 0
#endif
#if !defined(ARCH_X64)
#define ARCH_X64 0
#endif
#if !defined(ARCH_X86)
#define ARCH_X86 0
#endif
#if !defined(ARCH_ARM64)
#define ARCH_ARM64 0
#endif
#if !defined(ARCH_ARM32)
#define ARCH_ARM32 0
#endif
#if !defined(ARCH_WEBASSMEBLY)
#define ARCH_WEBASSMEBLY 0
#endif
#if !defined(COMPILER_CL)
#define COMPILER_CL 0
#endif
#if !defined(COMPILER_GCC)
#define COMPILER_GCC 0
#endif
#if !defined(COMPILER_CLANG)
#define COMPILER_CLANG 0
#endif
#if !defined(OS_WINDOWS)
#define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
#define OS_LINUX 0
#endif
#if !defined(OS_MAC)
#define OS_MAC 0
#endif
#if !defined(OS_WASM)
#define OS_WASM 0
#endif
#if !defined(LANG_CPP)
#define LANG_CPP 0
#endif
#if !defined(LANG_C)
#define LANG_C 0
#endif
#if !defined(DEBUG_BUILD)
#define DEBUG_BUILD 0
#endif

#define internal      static
#define global        static
#define local_persist static
//#define fallthrough /*fallthrough*/

#define _defer_loop(begin, end) for(i32 __i__ = ((begin), 0); !__i__; (__i__ += 1, (end)))
#define _defer_loop_checked(begin, end) for(i32 __i__ = 2 * !(begin); (__i__ == 2? ((end), 0): !__i__); (__i__ += 1, (end)))
#define defer(exp) _defer_loop(0, exp)

#if COMPILER_CL
# define assert_break __debugbreak()
#elif COMPILER_CLANG
# if __has_builtin(__builtin_debugtrap)
#  define assert_break __builtin_debugtrap()
# else
#  define assert_break *((volatile u32*)0) = (u32)0xdeadbeef
# endif
#else 
# define assert_break *((volatile u32*)0) = (u32)0xdeadbeef
#endif

#define assert(cond) do {if (!(cond)) { assert_break; }} while(0)
#define invalid_default_case default: invalid_code_path()
#define invalid_code_path(...) assert(!"invalid code path!!")
#define not_impemeneted(...) assert(!"not yet implemented!!")

#define unused_variable(name) (void)name

#define ptr_from_int(i) (void*)(((u8*)0) + (i))
#define int_from_ptr(p) (u64)(((u8*)(p)) - 0)
#define align_forward(n, a) (((n) + (a) - 1) & ~((a) - 1))


#define kilobytes(x) ((u64)(x) << 10)
#define megabytes(x) ((u64)(x) << 20)
#define gigabytes(x) ((u64)(x) << 30)


#if LANG_CPP
# define ZERO_STRUCT {}
#else
# define ZERO_STRUCT {0}
#endif

#define memory_set(p, v, len) memset(p, v, len)
#define memory_copy(dst, src, sz) memcpy(dst, src, sz)
#define memory_move(dst, src, sz) memmove(dst, src, sz)
#define memory_zero(p, s) memory_set(p, 0, s)

#define memory_copy_array(d, s) assert(sizeof(d) >= sizeof(s)); memory_copy(d, s, sizeof(s))
#define memory_copy_struct(d, s) memory_copy(d, s, sizeof(*(d)))

#define memory_zero_array(p) memory_zero(p, sizeof(p))
#define memory_zero_struct(p) memory_zero(p, sizeof(*(p)))

#define array_count(a) (sizeof(a) / sizeof((a)[0]))
#define has_flag(flags, f) ((flags) & (f))
#define set_flag(flags, f) ((flags) |= (f))
#define _make_flag(f) (1 << (f))

#define member(T, m) (((T*)0)->m)
#define member_offset(T, m) int_from_ptr(&member(T, m))
#define cast_from_member(ptr, T, m) (T*)(((u8*)(ptr)) - member_offset(T, m))

#define SWAP(T,a,b) do {T t__ = a; a = b; b = t__;} while(0)


////////////////////////////////
//~ NOTE(fakhri): Linked-List Macros
#define CheckNull(p) ((p)==0)
#define SetNull(p) ((p)=0)

#define QueuePush_NZ(f,l,n,next,zchk,zset) (zchk(f)?\
(((f)=(l)=(n)), zset((n)->next)):\
((l)->next=(n),(l)=(n),zset((n)->next)))
#define QueuePushFront_NZ(f,l,n,next,zchk,zset) (zchk(f) ? (((f) = (l) = (n)), zset((n)->next)) :\
((n)->next = (f)), ((f) = (n)))
#define QueuePop_NZ(f,l,next,zset) ((f)==(l)?\
(zset(f),zset(l)):\
((f)=(f)->next))

#define StackPush_N(f,n,next) ((n)->next=(f),(f)=(n))
#define StackPop_NZ(f,next,zchk) (zchk(f)?0:((f)=(f)->next))

#define DLLInsert_NPZ(f,l,p,n,next,prev,zchk,zset) \
(zchk(f) ? (((f) = (l) = (n)), zset((n)->next), zset((n)->prev)) :\
zchk(p) ? (zset((n)->prev), (n)->next = (f), (zchk(f) ? (0) : ((f)->prev = (n))), (f) = (n)) :\
((zchk((p)->next) ? (0) : (((p)->next->prev) = (n))), (n)->next = (p)->next, (n)->prev = (p), (p)->next = (n),\
((p) == (l) ? (l) = (n) : (0))))
#define DLLPushBack_NPZ(f,l,n,next,prev,zchk,zset) DLLInsert_NPZ(f,l,l,n,next,prev,zchk,zset)
#define DLLRemove_NPZ(f,l,n,next,prev,zchk,zset) (((f)==(n))?\
((f)=(f)->next, (zchk(f) ? (zset(l)) : zset((f)->prev))):\
((l)==(n))?\
((l)=(l)->prev, (zchk(l) ? (zset(f)) : zset((l)->next))):\
((zchk((n)->next) ? (0) : ((n)->next->prev=(n)->prev)),\
(zchk((n)->prev) ? (0) : ((n)->prev->next=(n)->next))))

#define DLLRemove_NP(f,l,n,next,prev)    DLLRemove_NPZ(f,l,n,next,prev,CheckNull,SetNull)
#define DLLPushBack_NP(f,l,n,next,prev)  DLLPushBack_NPZ(f,l,n,next,prev,CheckNull,SetNull)
#define DLLPushFront_NP(f,l,n,next,prev) DLLPushBack_NPZ(l,f,n,prev,next,CheckNull,SetNull)

#define QueuePush_Next(f,l,n, next)   QueuePush_NZ(f,l,n,next,CheckNull,SetNull)
#define QueuePush(f,l,n)         QueuePush_NZ(f,l,n,next,CheckNull,SetNull)
#define QueuePushFront(f,l,n)    QueuePushFront_NZ(f,l,n,next,CheckNull,SetNull)
#define QueuePop(f,l)            QueuePop_NZ(f,l,next,SetNull)
#define StackPush(f,n)           StackPush_N(f,n,next)
#define StackPop(f)              StackPop_NZ(f,next,CheckNull)
#define DLLPushBack(f,l,n)       DLLPushBack_NPZ(f,l,n,next,prev,CheckNull,SetNull)
#define DLLPushFront(f,l,n)      DLLPushBack_NPZ(l,f,n,prev,next,CheckNull,SetNull)
#define DLLInsert(f,l,p,n)       DLLInsert_NPZ(f,l,p,n,next,prev,CheckNull,SetNull)
#define DLLRemove(f,l,n)         DLLRemove_NPZ(f,l,n,next,prev,CheckNull,SetNull)


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

#if OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <windows.h>
# undef near
# undef far
# define CompletePreviousWritesBeforeFutureWrites() MemoryBarrier(); __faststorefence()
#endif


#if LANG_CPP
# if OS_WINDOWS
#  define exported extern "C" __declspec(dllexport)
# else
#  define exported extern "C"
# endif
#else
# if OS_WINDOWS
#  define exported __declspec(dllexport)
# else
#  define exported
# endif
#endif

#if LANG_CPP
# if OS_WINDOWS
#  define imported extern "C" __declspec(dllimport)
# else
#  define imported extern "C"
# endif
#else
# if OS_WINDOWS
#  define imported __declspec(dllimport)
# else
#  define imported
# endif
#endif

#if LANG_C
# if COMPILER_CL
#  define thread_local __declspec(thread)
# elif COMPILER_CLANG
#  define thread_local __thread
# elif COMPILER_GCC
#  define thread_local __thread
# endif
#endif

#endif //HAZE_BASE_H
