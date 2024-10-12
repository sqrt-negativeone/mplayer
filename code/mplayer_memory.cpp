
struct Memory_Arena
{
	void *base_memory;
	u64 capacity;
	u64 used_memory;
	u8 alignment;
};

struct Memory_Checkpoint
{
	Memory_Arena *arena;
	u64 old_used_memory;
	
	#if LANG_CPP
		Memory_Checkpoint() = default;
	Memory_Checkpoint(Memory_Arena *arena)
	{
		this->arena = arena;
		this->old_used_memory = arena->used_memory;
	}
	
	~Memory_Checkpoint()
	{
		arena->used_memory = old_used_memory;
	}
	#endif
};

#define m_arena_push_array(arena, T, count) (T *)m_arena_push(arena, sizeof(T) * (count))
#define m_arena_push_struct(arena, T) m_arena_push_array(arena, T, 1)

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

internal void
m_arena_free_all(Memory_Arena *arena)
{
	arena->used_memory = 0;
}

internal Memory_Checkpoint
m_checkpoint_begin(Memory_Arena *arena)
{
	Memory_Checkpoint result;
	result.arena = arena;
	result.old_used_memory = arena->used_memory;
	return result;
}

internal void
m_checkpoint_end(Memory_Checkpoint checkpoint)
{
	assert(checkpoint.arena);
	checkpoint.arena->used_memory = checkpoint.old_used_memory;
}
