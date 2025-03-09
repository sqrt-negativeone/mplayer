/* date = December 26th 2024 1:52 pm */

#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

typedef struct Thread_Context Thread_Context;
struct Thread_Context
{
	b32 initialized;
	Memory_Arena arenas[2];
};

#define begin_scratch(conflicts, count) get_scratch(conflicts, count)
#define end_scratch(scratch) m_checkpoint_end(scratch)

internal void init_thread_context();
internal Memory_Checkpoint get_scratch(Memory_Arena **conflicts, u64 count);
internal Memory_Checkpoint get_scratch0();

#endif //THREAD_CONTEXT_H
