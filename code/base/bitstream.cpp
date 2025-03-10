

internal b32
bitstream_is_empty(Bit_Stream *bitstream)
{
	b32 result = bitstream->pos.byte_index == bitstream->buffer.size;
	return result;
}

internal void
bitstream_advance_to_next_byte_boundary(Bit_Stream *bitstream) {
	if (bitstream->pos.bits_left < 8)
	{
		bitstream->pos.byte_index += 1;
		bitstream->pos.bits_left = 8;
	}
}

internal u64
bitstream_read_bits_unsafe(Bit_Stream *bitstream, u8 bits)
{
	u64 result = 0;
	assert(bits <= 64);
	
	// NOTE(fakhri): align to byte boundary
	if (bitstream->pos.bits_left != 8 && bits > bitstream->pos.bits_left)
	{
		u8 bits_to_read = bitstream->pos.bits_left;
		result = bitstream_read_bits_unsafe(bitstream, bits_to_read);
		bits -= bits_to_read;
		assert(bitstream->pos.bits_left == 8);
	}
	
	for (;bits >= 8;)
	{
		result <<= 8;
		result |= (u64)(bitstream->buffer.data[bitstream->pos.byte_index]);
		bitstream->pos.byte_index += 1;
		bits -= 8;
	}
	
	if (bits != 0 && bits <= bitstream->pos.bits_left)
	{
		result <<= bits;
		result |= (u64)((bitstream->buffer.data[bitstream->pos.byte_index] >> (bitstream->pos.bits_left - bits)) & ((1 << bits) - 1));
		bitstream->pos.bits_left -= bits;
		if (bitstream->pos.bits_left == 0)
		{
			bitstream->pos.bits_left = 8;
			bitstream->pos.byte_index += 1;
		}
	}
	
	return result;
}

internal i64
sign_extend(i64 value, u8 bits_width)
{
	i64 value_sign_bit = (i64)(value & (1ull << (bits_width - 1)));
	i64 mask_bits = (i64)(value_sign_bit << (64 - bits_width)) >> (64 - bits_width);
	i64 result = value | mask_bits;
	return result;
}

internal i64
bitstream_read_sample_unencoded(Bit_Stream *bitstream, u8 sample_bit_depth, u8 shift_bits = 0)
{
	i64 sample_value = (i64)(bitstream_read_bits_unsafe(bitstream, sample_bit_depth) << shift_bits);
	u8 bits_width = sample_bit_depth + shift_bits;
	sample_value = sign_extend(sample_value, bits_width);
	return sample_value;
}

internal u8
bitstream_read_u8(Bit_Stream *bitstream)
{
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u8 result = bitstream->buffer.data[bitstream->pos.byte_index];
	bitstream->pos.byte_index += 1;
	return result;
}

internal u16
bitstream_read_u16be(Bit_Stream *bitstream)
{
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u16 result = ((u16)(bitstream->buffer.data[bitstream->pos.byte_index]) << 8) | (u16)(bitstream->buffer.data[bitstream->pos.byte_index + 1]);
	bitstream->pos.byte_index += 2;
	return result;
}

internal u16
bitstream_read_u16le(Bit_Stream *bitstream)
{
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u16 result = ((u16)(bitstream->buffer.data[bitstream->pos.byte_index + 1]) << 8) | (u16)(bitstream->buffer.data[bitstream->pos.byte_index]);
	bitstream->pos.byte_index += 2;
	return result;
}

internal u32
bitstream_read_u24be(Bit_Stream *bitstream)
{
	Buffer *buffer = &bitstream->buffer;
	u64 byte_index = bitstream->pos.byte_index;
	
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u32 result = ((u32)(buffer->data[byte_index]) << 16) | ((u32)(buffer->data[byte_index + 1]) << 8) | (u32)(buffer->data[byte_index + 2]);
	bitstream->pos.byte_index += 3;
	return result;
}

internal u32
bitstream_read_u32be(Bit_Stream *bitstream)
{
	Buffer *buffer = &bitstream->buffer;
	u64 byte_index = bitstream->pos.byte_index;
	
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u32 result = (((u32)(buffer->data[byte_index]) << 24)    | 
		((u32)(buffer->data[byte_index + 1]) << 16)|
		((u32)(buffer->data[byte_index + 2]) << 8) |
		(u32)(buffer->data[byte_index + 3]));
	bitstream->pos.byte_index += 4;
	return result;
}

internal u32
bitstream_read_u32le(Bit_Stream *bitstream)
{
	Buffer *buffer = &bitstream->buffer;
	u64 byte_index = bitstream->pos.byte_index;
	
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u32 result = (((u32)(buffer->data[byte_index + 3]) << 24) | 
		((u32)(buffer->data[byte_index + 2]) << 16) |
		((u32)(buffer->data[byte_index + 1]) << 8)  |
		((u32)(buffer->data[byte_index + 0]) << 0));
	bitstream->pos.byte_index += 4;
	
	return result;
}

internal u64
bitstream_read_u64be(Bit_Stream *bitstream)
{
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	u64 result = (((u64)bitstream_read_u32be(bitstream) << 32) | (u64)bitstream_read_u32be(bitstream));
	
	return result;
}


internal void
bitstream_skip_bytes(Bit_Stream *bitstream, u64 bytes_count)
{
	bitstream->pos.byte_index += bytes_count;
	return;
}

internal void
bitstream_skip_bits(Bit_Stream *bitstream, u64 bits)
{
	bitstream_skip_bytes(bitstream, bits / 8);
	bits &= 7;
	
	if (bits >= bitstream->pos.bits_left)
	{
		bits -= bitstream->pos.bits_left;
		bitstream->pos.byte_index += 1;
		bitstream->pos.bits_left = 8;
	}
	
	bitstream->pos.bits_left -= (u8)(bits);
}

internal Buffer
bitstream_read_buffer(Bit_Stream *bitstream, u64 length)
{
	// NOTE(fakhri): make sure we are at byte boundary
	assert(bitstream->pos.bits_left == 8);
	Buffer result = make_buffer(bitstream->buffer.data + bitstream->pos.byte_index, length);
	bitstream_skip_bytes(bitstream, length);
	return result;
}