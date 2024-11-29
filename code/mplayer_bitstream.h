/* date = November 18th 2024 4:16 pm */

#ifndef MPLAYER_BITSTREAM_H
#define MPLAYER_BITSTREAM_H


struct Bit_Stream_Pos
{
	u64 byte_index;
	u8 bits_left;
};

struct Bit_Stream
{
	Buffer buffer;
	Bit_Stream_Pos pos;
};

internal b32 bitstream_is_empty(Bit_Stream *bitstream);

#endif //MPLAYER_BITSTREAM_H
