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
byte_stream_read(Byte_Stream *bs, Mplayer_Track_ID *dst)
{
	if (byte_stream_available_bytes(*bs) >= sizeof(Mplayer_Track_ID))
	{
		memory_copy(dst, bs->current, sizeof(*dst));
		byte_stream_advance(bs, sizeof(*dst));
	}
}
internal void
byte_stream_read(Byte_Stream *bs, Mplayer_Album_ID *dst)
{
	if (byte_stream_available_bytes(*bs) >= sizeof(Mplayer_Album_ID))
	{
		memory_copy(dst, bs->current, sizeof(*dst));
		byte_stream_advance(bs, sizeof(*dst));
	}
}
internal void
byte_stream_read(Byte_Stream *bs, Mplayer_Artist_ID *dst)
{
	if (byte_stream_available_bytes(*bs) >= sizeof(Mplayer_Artist_ID))
	{
		memory_copy(dst, bs->current, sizeof(*dst));
		byte_stream_advance(bs, sizeof(*dst));
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
byte_stream_read(Byte_Stream *bs, Mplayer_Track_List *track_list)
{
	not_impemeneted();
	#if 0
		byte_stream_read(bs, &tracks->count);
	
	if (byte_stream_available_bytes(*bs) >= tracks->count * sizeof(Mplayer_Track_ID))
	{
		Mplayer_Track_ID *tracks_array = (Mplayer_Track_ID*)bs->current;
		for(u32 i = 0; i < count; i += 1)
		{
			Mplayer_Track_List_Entry *entry = 0;
		}
		byte_stream_advance(bs, tracks_array->count * sizeof(Mplayer_Track_ID));
	}
	#endif
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
