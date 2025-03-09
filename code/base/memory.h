/* date = October 26th 2024 0:40 pm */

#ifndef HAZE_MEMORY_H
#define HAZE_MEMORY_H

typedef struct Memory_Arena_Chunk Memory_Arena_Chunk;
struct Memory_Arena_Chunk
{
	u64 capacity;
	u64 used_memory;
	Memory_Arena_Chunk *prev;
};

typedef struct Memory_Arena Memory_Arena;
struct Memory_Arena
{
	Memory_Arena_Chunk *current_chunk;
	u64 min_chunk_size;
};

typedef struct Memory_Checkpoint Memory_Checkpoint;
struct Memory_Checkpoint
{
	Memory_Arena *arena;
	Memory_Arena_Chunk *chunk;
	u64 old_used_memory;
};

#if LANG_CPP
struct Memory_Checkpoint_Scoped
{
	Memory_Checkpoint checkpoint;
	Memory_Arena *arena;
	
	Memory_Checkpoint_Scoped() = default;
	
	Memory_Checkpoint_Scoped(Memory_Checkpoint checkpoint);
	Memory_Checkpoint_Scoped(Memory_Arena *arena);
	~Memory_Checkpoint_Scoped();
};
#endif

#define m_arena_push_array(arena, T, count) (T *)m_arena_push(arena, sizeof(T) * (count))
#define m_arena_push_array_aligned(arena, T, count, alignment) (T *)m_arena_push_aligned(arena, sizeof(T) * (count), alignment)
#define m_arena_push_struct(arena, T) m_arena_push_array(arena, T, 1)
#define m_arena_push_struct_aligned(arena, T, alignment) m_arena_push_array_aligned(arena, T, 1, alignment)

#define m_arena_push_array_z(arena, T, count) (T *)m_arena_push_z(arena, sizeof(T) * (count))
#define m_arena_push_struct_z(arena, T) m_arena_push_array_z(arena, T, 1)

internal void *m_arena_push_aligned(Memory_Arena *arena, u64 size, u8 alignment);
internal void *m_arena_push(Memory_Arena *arena, u64 size);
internal void *m_arena_push_z(Memory_Arena *arena, u64 size);
internal void m_arena_free_all(Memory_Arena *arena);

#define m_arena_bootstrap_push(Type, arena_member) (Type*)_m_arena_bootstrap_push(sizeof(Type), member_offset(Type, arena_member))
internal void *_m_arena_bootstrap_push(u64 struct_size, u64 offset_to_arena);

internal Memory_Checkpoint m_checkpoint_begin(Memory_Arena *arena);
internal void m_checkpoint_end(Memory_Checkpoint checkpoint);
#endif //HAZE_MEMORY_H
