/* date = November 18th 2024 4:15 pm */

#ifndef MPLAYER_FLAC_H
#define MPLAYER_FLAC_H

#include "third_party/samplerate.h"

#define FLAC_MAX_CHANNEL_COUNT 8

struct Flac_Stream_Info
{
	u16 min_block_size; // 16 bits
	u16 max_block_size; // 16 bits
	u32 min_frame_size; // 24 bits
	u32 max_frame_size; // 24 bits
	u32 sample_rate;    // 20 bits
	u8 nb_channels;     // 3 bits
	u8 bits_per_sample; // 5 bits
	u64 samples_count;  // 36 bits
};


enum Flac_Block_Strategy
{
	Fixed_Size    = 0,
	Variable_Size = 1,
};

enum Flac_Stereo_Channel_Config
{
	None,
	Left_Right,
	Left_Side,
	Side_Right,
	Mid_Side
};

struct Flac_Decoded_Block
{
	f32 *samples; // interleaved
	u64 frames_count;
	u32 channels_count;
};


enum Flac_Subframe_Kind
{
	Subframe_Constant,
	Subframe_Verbatim,
	Subframe_Fixed_Prediction,
	Subframe_Linear_Prediction,
};

struct Flac_Subframe_Type
{
	Flac_Subframe_Kind kind;
	u8 order;
};

struct Flac_Subframe_Info
{
	Bit_Stream_Pos samples_start_pos;
	Flac_Subframe_Type subframe_type;
	u8 wasted_bits;
};

struct Flac_Block_Info
{
	b32 success;
	Flac_Stereo_Channel_Config channel_config;
	u8 nb_channels;
	u8 bits_depth;
	
	Flac_Block_Strategy block_strat;
	u64 block_size;
	Bit_Stream_Pos start_pos;
	Flac_Subframe_Info subframes_info[FLAC_MAX_CHANNEL_COUNT];
};

struct Flac_Seek_Point
{
	u64 sample_number; // sample number of the first sample in the target frame
	u64 byte_offset;   // offset from the first byte of the first frame header to the first byte of the taret frame's header
	u16 samples_count; // number of samples in the target frame
};

struct Flac_Picture
{
	u32 type;
	String8 media_type_string;
	String8 description;
	V2_I32 dim;
	u32 color_depth;
	u32 nb_colors_used;
	Buffer buffer;
};

struct Seek_Table_Work_Data
{
	volatile b32 cancel_req;
	volatile b32 running;
	u32 seek_points_count;
	Flac_Seek_Point *seek_table;
	struct Flac_Stream *flac_stream;
	u64 samples_count;
	u64 first_block_offset;
	Bit_Stream bitstream;
	u8 nb_channels;
	u8 bits_per_sample;
};

struct Flac_Stream 
{
	Bit_Stream bitstream;
	Flac_Stream_Info streaminfo;
	Memory_Arena block_arena;
	Flac_Decoded_Block recent_block;
	u64 remaining_frames_count;
	
	Bit_Stream_Pos first_block_pos;
	b32 fixed_blocks;
	u64 next_sample_number;
	
	Flac_Seek_Point *seek_table;
	u64 seek_table_size;
	Flac_Picture *front_cover;
	String8_List vorbis_comments;
	
	SRC_STATE *src_ctx;
};

#endif //MPLAYER_FLAC_H
