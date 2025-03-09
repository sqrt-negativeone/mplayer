/* date = October 12th 2024 5:04 pm */

#ifndef HAZE_BUFFER_H
#define HAZE_BUFFER_H

typedef struct Buffer Buffer;
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

internal Buffer arena_push_buffer(Memory_Arena *arena, u64 size);
internal String8 to_string(Buffer buffer);
internal b32 is_valid(Buffer buffer);
internal Buffer clone_buffer(Memory_Arena *arena, Buffer buffer);
internal Buffer make_buffer_copy(Memory_Arena *arena, u8 *data, u64 size);

#endif //HAZE_BUFFER_H
