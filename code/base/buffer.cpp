
internal Buffer
arena_push_buffer(Memory_Arena *arena, u64 size)
{
	Buffer result = make_buffer(m_arena_push_array(arena, u8, size), size);
	return result;
}

internal Buffer
to_buffer(String8 string)
{
	Buffer result = ZERO_STRUCT;
	result.data = string.str;
	result.size = string.len;
	return result;
}

internal String8
to_string(Buffer buffer)
{
	String8 result = str8(buffer.data, buffer.size);
	return result;
}

internal b32
is_valid(Buffer buffer)
{
	b32 result = buffer.size != 0 && buffer.data;
	return result;
}

internal Buffer
clone_buffer(Memory_Arena *arena, Buffer buffer)
{
	Buffer result = arena_push_buffer(arena, buffer.size);
	if (result.size)
	{
		memory_copy(result.data, buffer.data, result.size);
	}
	return result;
}

internal Buffer
make_buffer_copy(Memory_Arena *arena, u8 *data, u64 size)
{
	Buffer result = clone_buffer(arena, make_buffer(data, size));
	return result;
}
