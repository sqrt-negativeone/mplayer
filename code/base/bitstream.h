/* date = November 18th 2024 4:16 pm */

#ifndef HAZE_BITSTREAM_H
#define HAZE_BITSTREAM_H


typedef struct Bit_Stream_Pos Bit_Stream_Pos;
struct Bit_Stream_Pos
{
	u64 byte_index;
	u8 bits_left;
};

typedef struct Bit_Stream Bit_Stream;
struct Bit_Stream
{
	Buffer buffer;
	Bit_Stream_Pos pos;
};

internal b32 bitstream_is_empty(Bit_Stream *bitstream);

#endif //HAZE_BITSTREAM_H
