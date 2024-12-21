
struct Thread_Context
{
	b32 initialized;
	Memory_Arena arenas[2];
};

global thread_local Thread_Context tl_thread_ctx = ZERO_STRUCT;

#define begin_scratch(conflicts, count) get_scratch(conflicts, count)
#define end_scratch(scratch) m_checkpoint_end(scratch)

internal void
init_thread_context()
{
	if (!tl_thread_ctx.initialized)
	{
		tl_thread_ctx.initialized = true;
	}
}

internal _Memory_Checkpoint
get_scratch(Memory_Arena **conflicts, u64 count)
{
	_Memory_Checkpoint result = ZERO_STRUCT;
	
	for (u32 i = 0; i < array_count(tl_thread_ctx.arenas); i += 1)
	{
		Memory_Arena *arena = &tl_thread_ctx.arenas[i];
		b32 conflicting = false;
		for (u64 j = 0; j < count; j += 1)
		{
			if (conflicts[j] == arena)
			{
				conflicting = true;
				break;
			}
		}
		if (!conflicting)
		{
			result = m_checkpoint_begin(arena);
			break;
		}
	}
	
	return result;
}