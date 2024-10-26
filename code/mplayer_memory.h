/* date = October 26th 2024 0:40 pm */

#ifndef MPLAYER_MEMORY_H
#define MPLAYER_MEMORY_H

struct Memory_Arena_Chunk
{
	u64 capacity;
	u64 used_memory;
	Memory_Arena_Chunk *prev;
};

struct Memory_Arena
{
	Memory_Arena_Chunk *current_chunk;
	u64 min_chunk_size;
};

struct Memory_Checkpoint
{
	Memory_Arena *arena;
	Memory_Arena_Chunk *chunk;
	u64 old_used_memory;
	
	Memory_Checkpoint() = default;
	
	Memory_Checkpoint(Memory_Arena *arena);
	~Memory_Checkpoint();
};

#define m_arena_push_array(arena, T, count) (T *)m_arena_push(arena, sizeof(T) * (count))
#define m_arena_push_struct(arena, T) m_arena_push_array(arena, T, 1)

#define m_arena_push_array_z(arena, T, count) (T *)m_arena_push_z(arena, sizeof(T) * (count))
#define m_arena_push_struct_z(arena, T) m_arena_push_array_z(arena, T, 1)

internal void *m_arena_push(Memory_Arena *arena, u64 size);
internal void *m_arena_push_z(Memory_Arena *arena, u64 size);

internal Memory_Checkpoint m_checkpoint_begin(Memory_Arena *arena);
internal void m_checkpoint_end(Memory_Checkpoint checkpoint);

#endif //MPLAYER_MEMORY_H
