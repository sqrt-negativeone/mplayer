/* date = October 12th 2024 5:04 pm */

#ifndef MPLAYER_BUFFER_H
#define MPLAYER_BUFFER_H

struct Buffer
{
	u8 *data;
	u64 size;
};

internal Buffer
make_buffer(u8 *data, u64 size)
{
	Buffer result;
	result.data = data;
	result.size = size;
	return result;
}

internal Buffer
arena_push_buffer(Memory_Arena *arena, u64 size)
{
	Buffer result = make_buffer(m_arena_push_array(arena, u8, size), size);
	return result;
}




#endif //MPLAYER_BUFFER_H