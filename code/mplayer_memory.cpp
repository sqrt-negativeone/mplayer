
#define M_ARENA_DEFAULT_CAPACITY megabytes(8)


internal Memory_Arena_Chunk *
m_arena_make_chunk(u64 capacity)
{
	capacity = capacity + sizeof(Memory_Arena_Chunk);
	void *back_buffer = platform->alloc(capacity);
	
	Memory_Arena_Chunk *arena = (Memory_Arena_Chunk *)back_buffer;
	arena->capacity    = capacity;
	arena->used_memory = sizeof(Memory_Arena_Chunk);
	return arena;
}

internal void *
m_arena_push_aligned(Memory_Arena *arena, u64 size, u8 alignment)
{
	void *result = 0;
	
	u64 allocation_size  = size;
	u64 alignment_offset = 0;
	
	if (arena->current_chunk)
	{
		u64 pos_address  = int_from_ptr(arena->current_chunk) + arena->current_chunk->used_memory;
		alignment_offset = align_forward(pos_address, alignment) - pos_address;
		allocation_size += alignment_offset;
	}
	
	if (!arena->current_chunk ||
		(arena->current_chunk->used_memory + allocation_size > arena->current_chunk->capacity))
	{
		if (!arena->min_chunk_size)
		{
			arena->min_chunk_size = M_ARENA_DEFAULT_CAPACITY;
		}
		
		u64 arena_chunk_capacity = MAX(allocation_size, arena->min_chunk_size);
		Memory_Arena_Chunk *new_chunk = m_arena_make_chunk(arena_chunk_capacity);
		new_chunk->prev = arena->current_chunk;
		arena->current_chunk = new_chunk;
	}
	
	assert(arena->current_chunk->used_memory + allocation_size <= arena->current_chunk->capacity);
	
	u8 *base_memory = (u8*)arena->current_chunk;
	result = base_memory + arena->current_chunk->used_memory + alignment_offset;
	arena->current_chunk->used_memory += allocation_size;
	
	return result;
}

internal void *
m_arena_push(Memory_Arena *arena, u64 size)
{
	void *result = m_arena_push_aligned(arena, size, 8);
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
m_arena_free_last_chunk(Memory_Arena *arena)
{
	if (arena->current_chunk)
	{
		Memory_Arena_Chunk *chunk = arena->current_chunk;
		arena->current_chunk = chunk->prev;
		platform->dealloc(chunk);
	}
}

internal void
m_arena_free_all(Memory_Arena *arena)
{
	for (;arena->current_chunk;)
	{
		m_arena_free_last_chunk(arena);
	}
}


#define m_arena_bootstrap_push(Type, arena_member) (Type*)_m_arena_bootstrap_push(sizeof(Type), member_offset(Type, arena_member))
internal void *
_m_arena_bootstrap_push(u64 struct_size, u64 offset_to_arena)
{
	Memory_Arena bootstrap = ZERO_STRUCT;
	void *struct_ptr = m_arena_push(&bootstrap, struct_size);
	*((Memory_Arena *)((u8*)struct_ptr + offset_to_arena)) = bootstrap;
	return struct_ptr;
}



internal _Memory_Checkpoint
m_checkpoint_begin(Memory_Arena *arena)
{
	_Memory_Checkpoint result;
	result.arena = arena;
	result.chunk = arena->current_chunk;
	result.old_used_memory = arena->current_chunk? arena->current_chunk->used_memory:0;
	return result;
}

internal void
m_checkpoint_end(_Memory_Checkpoint checkpoint)
{
	assert(checkpoint.arena);
	for (;checkpoint.chunk != checkpoint.arena->current_chunk;)
	{
		m_arena_free_last_chunk(checkpoint.arena);
	}
	
	if (checkpoint.chunk)
	{
		checkpoint.chunk->used_memory = checkpoint.old_used_memory;
	}
}


Memory_Checkpoint_Scoped::Memory_Checkpoint_Scoped(Memory_Arena *arena)
{
	this->checkpoint = m_checkpoint_begin(arena);
	this->arena = arena;
}


Memory_Checkpoint_Scoped::Memory_Checkpoint_Scoped(_Memory_Checkpoint checkpoint)
{
	this->checkpoint = checkpoint;
	this->arena = checkpoint.arena;
}

Memory_Checkpoint_Scoped::~Memory_Checkpoint_Scoped()
{
	m_checkpoint_end(checkpoint);
}