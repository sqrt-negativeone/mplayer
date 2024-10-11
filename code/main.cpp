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

typedef int32_t b32;

#define internal      static
#define global        static
#define local_persist static

#if defined(_MSC_VER) 
# define COMPILER_CL 1
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
#define invalid_code_path(...) assert(!"invalid code path!!")
#define not_impemeneted(...) assert(!"not yet implemented!!")

#define ptr_from_int(i) (void*)(((u8*)0) + (i))
#define int_from_ptr(p) (u64)(((u8*)(p)) - 0)
#define align_forward(n, a) (((n) + (a) - 1) & ~((a) - 1))

#define kilbytes(x)  ((x) << 10)
#define megabytes(x) ((x) << 20)
#define gigabytes(x) ((x) << 30)

#define ZERO_STRUCT {}

#define memory_zero(p, s) memset(p, 0, s)
#define memory_zero_array(p) memory_zero(p, sizeof(p))
#define memory_zero_struct(p) memory_zero(p, sizeof(*(p)))

internal void *
sys_allocate_memory(u64 size)
{
	void *result = malloc(size);
	return result;
}

struct Memory_Arena
{
	void *base_memory;
	u64 capacity;
	u64 used_memory;
	u8 alignment;
};

#define m_arena_push_array(arena, T, count) (T *)m_arena_push(arena, sizeof(T) * (count))

internal Memory_Arena *
m_arena_make(u64 capacity, u8 alignment = 4)
{
	void *back_buffer = sys_allocate_memory(capacity + sizeof(Memory_Arena));
	
	Memory_Arena *arena = (Memory_Arena *)back_buffer;
	arena->base_memory = (void *)(arena + 1);
	arena->capacity = capacity;
	arena->used_memory = 0;
	arena->alignment = alignment;
	return arena;
}

internal void *
m_arena_push_aligned(Memory_Arena *arena, u64 size, u8 alignment)
{
	void *result = 0;
	
	u64 allocation_pos = int_from_ptr(arena->base_memory) + arena->used_memory;
	u64 alignment_size = align_forward(allocation_pos, alignment) - allocation_pos;
	u64 availabe_bytes = arena->capacity - (arena->used_memory + alignment_size);
	if (size <= availabe_bytes)
	{
		result = (u8*)arena->base_memory + arena->used_memory + alignment_size;
		arena->used_memory += size + alignment;
	}
return result;
}

internal void *
m_arena_push(Memory_Arena *arena, u64 size)
{
	void *result = m_arena_push_aligned(arena, size, arena->alignment);
		return result;
}

internal void *
m_arena_push_z(Memory_Arena *arena, u64 size)
{
	void *result = m_arena_push(arena, size);
	memory_zero(result, size);
		return result;
}

struct Buffer
{
	u8 *data;
	u64 size;
};

#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("%s <filename>\n", argv[0]);
		return 1;
	}
	
	return 0;
}