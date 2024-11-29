/* date = November 26th 2024 7:13 pm */

#ifndef MPLAYER_BYTE_STREAM_H
#define MPLAYER_BYTE_STREAM_H

struct Byte_Stream
{
	u8 *start;
	u8 *end;
	u8 *current;
};

internal Byte_Stream
make_byte_stream(Buffer buffer)
{
	Byte_Stream bs = ZERO_STRUCT;
	bs.start = buffer.data;
	bs.end = buffer.data + buffer.size;
	bs.current = bs.start;
	return bs;
}

internal u64
byte_stream_available_bytes(Byte_Stream bs)
{
	u64 result = (bs.end - bs.current);
	return result;
}

internal void
byte_stream_advance(Byte_Stream *bs, u64 advance)
{
	bs->current += advance;
	if (bs->current > bs->end)
	{
		bs->current = bs->end;
	}
}

internal void
byte_stream_read(Byte_Stream *bs, u32 *dst)
{
	if (byte_stream_available_bytes(*bs) >= 4)
	{
		memory_copy(dst, bs->current, sizeof(*dst));
		byte_stream_advance(bs, sizeof(*dst));
	}
}

internal void
byte_stream_read(Byte_Stream *bs, b32 *dst)
{
	if (byte_stream_available_bytes(*bs) >= 4)
	{
		memory_copy(dst, bs->current, sizeof(*dst));
		byte_stream_advance(bs, sizeof(*dst));
	}
}

internal void
byte_stream_read(Byte_Stream *bs, u64 *dst)
{
	if (byte_stream_available_bytes(*bs) >= 4)
	{
		memory_copy(dst, bs->current, sizeof(*dst));
		byte_stream_advance(bs, sizeof(*dst));
	}
}

internal void
byte_stream_read(Byte_Stream *bs, String8 *string)
{
	byte_stream_read(bs, &string->len);
	
	if (byte_stream_available_bytes(*bs) >= string->len)
	{
		string->str = bs->current;
		byte_stream_advance(bs, string->len);
	}
}

internal void
byte_stream_read(Byte_Stream *bs, Buffer *buffer)
{
	byte_stream_read(bs, &buffer->size);
	
	if (byte_stream_available_bytes(*bs) >= buffer->size)
	{
		buffer->data = bs->current;
		byte_stream_advance(bs, buffer->size);
	}
}

internal void
byte_stream_read(Byte_Stream *bs, Mplayer_Items_Array *items_array)
{
	byte_stream_read(bs, &items_array->count);
	
	if (byte_stream_available_bytes(*bs) >= items_array->count * sizeof(Mplayer_Item_ID))
	{
		items_array->items= (Mplayer_Item_ID*)bs->current;
		byte_stream_advance(bs, items_array->count * sizeof(Mplayer_Item_ID));
	}
}

internal void
byte_stream_read(Byte_Stream *bs, Mplayer_Item_Image *image)
{
	byte_stream_read(bs, &image->in_disk);
	if (image->in_disk)
	{
		byte_stream_read(bs, &image->path);
	}
	else
	{
		byte_stream_read(bs, &image->texture_data);
	}
}

#endif //MPLAYER_BYTE_STREAM_H
