
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


global u32 flac_sample_rates[] = {
	88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000,
};

global u8 flac_block_channel_count[] = {
	1,2,3,4,5,6,7,8,2,2,2,0,0
};

global Flac_Stereo_Channel_Config flac_block_channel_config[] = {
	None, Left_Right, None, None, None, None, None, None, Left_Side, Side_Right, Mid_Side, None, None
};

global i16 flac_bits_depth[] = {
	-1, 8, 12, -1, 16, 20, 24, 32
};


#define flac_access(samples, sample_index) (samples)[(nb_channels) * (sample_index) + (channel_index)]
#define flac_access2(samples, nb_channels, channel_index, sample_index) (samples)[(nb_channels) * (sample_index) + (channel_index)]




internal void
flac_skip_coded_residuals(Bit_Stream *bitstream, u32 block_size, u32 order)
{
	u8 params_bits = 0;
	u8 escape_code = 0;
	
	switch (bitstream_read_bits_unsafe(bitstream, 2))
	{
		case 0: {
			params_bits = 4;
			escape_code = 0xF;
		} break;
		case 1: {
			params_bits = 5;
			escape_code = 0x1F;
		} break;
		default: {
			invalid_code_path("reserved");
		}
	}
	
	u64 partition_order = bitstream_read_bits_unsafe(bitstream, 4);
	u64 partition_count = 1ull << partition_order;
	for (u32 part_index = 0; part_index < partition_count; part_index += 1)
	{
		// NOTE(fakhri): we can't do each partition in parallel, if the partition is not an escape partition
		// then we can't know the size of it because it contains numbers encoded in unary, which have variable
		// size... BUT, since we know the size of the escape partition, we can have it be decoded in parallel,
		// and overlap that work with the non escape partition
		u64 residual_samples_count = (block_size >> partition_order) - u32((part_index == 0)? order:0);
		u8 paramter = u8(bitstream_read_bits_unsafe(bitstream, params_bits));
		if (paramter == escape_code)
		{
			// NOTE(fakhri): escape partition
			u8 residual_bits_precision = u8(bitstream_read_bits_unsafe(bitstream, 5));
			
			bitstream_skip_bits(bitstream, residual_bits_precision * residual_samples_count);
		}
		else
		{
			for (;residual_samples_count--;)
			{
				for (;bitstream_read_bits_unsafe(bitstream, 1) == 0;);
				bitstream_skip_bits(bitstream, paramter);
			}
		}
	}
}

internal void
flac_decode_coded_residuals(Bit_Stream *bitstream, i64 *samples, u32 nb_channels, u32 channel_index, u32 block_size, u32 order)
{
	u8 params_bits = 0;
	u8 escape_code = 0;
	
	switch (bitstream_read_bits_unsafe(bitstream, 2))
	{
		case 0: {
			params_bits = 4;
			escape_code = 0xF;
		} break;
		case 1: {
			params_bits = 5;
			escape_code = 0x1F;
		} break;
		default: {
			invalid_code_path("reserved");
		}
	}
	
	u64 partition_order = bitstream_read_bits_unsafe(bitstream, 4);
	u64 partition_count = 1ull << partition_order;
	u32 sample_index = order;
	for (u32 part_index = 0; part_index < partition_count; part_index += 1)
	{
		// NOTE(fakhri): we can't do each partition in parallel, if the partition is not an escape partition
		// then we can't know the size of it because it contains numbers encoded in unary, which have variable
		// size... BUT, since we know the size of the escape partition, we can have it be decoded in parallel,
		// and overlap that work with the non escape partition
		// TODO(fakhri): do escape partitions in parallel
		u64 residual_samples_count = (block_size >> partition_order) - u32((part_index == 0)? order:0);
		u8 paramter = u8(bitstream_read_bits_unsafe(bitstream, params_bits));
		if (paramter == escape_code)
		{
			// NOTE(fakhri): escape partition
			u8 residual_bits_precision = u8(bitstream_read_bits_unsafe(bitstream, 5));
			if (residual_bits_precision != 0)
			{
				for (;residual_samples_count--;)
				{
					flac_access(samples, sample_index) = bitstream_read_sample_unencoded(bitstream, residual_bits_precision, 0);
					sample_index += 1;
				}
			}
		}
		else
		{
			for (;residual_samples_count--;)
			{
				i64 msp = 0;
				for (;bitstream_read_bits_unsafe(bitstream, 1) == 0;)
				{
					msp += 1;
				}
				i64 lsp = i64(bitstream_read_bits_unsafe(bitstream, paramter));
				
				i64 sample_value = 0;
				i64 folded_sample_value = (msp << paramter) | lsp;
				sample_value = folded_sample_value >> 1;
				if ((folded_sample_value & 1) == 1)
				{
					sample_value = ~sample_value;
				}
				
				flac_access(samples, sample_index) = sample_value;
				sample_index += 1;
			}
		}
	}
}

internal Flac_Block_Info
flac_preprocess_block(Bit_Stream *bitstream, u8 nb_channels, u8 bits_per_sample, b32 first_block = 0)
{
	Flac_Block_Info block_info = ZERO_STRUCT;
	block_info.start_pos = bitstream->pos;
	assert(block_info.start_pos.bits_left == 8); // byte boundary
	
	if (bitstream_is_empty(bitstream))
	{
		return block_info;
	}
	
	block_info.nb_channels = nb_channels;
	
	// NOTE(fakhri): decode header
	{
		u64 sync_code = bitstream_read_bits_unsafe(bitstream, 15);
		assert(sync_code == 0x7ffc); // 0b111111111111100
		
		// TODO(fakhri): didn't test Variable_Size startegy yet!!
		block_info.block_strat = Flac_Block_Strategy(bitstream_read_bits_unsafe(bitstream, 1));
		
		u64 block_size_bits  = bitstream_read_bits_unsafe(bitstream, 4);
		u64 sample_rate_bits = bitstream_read_bits_unsafe(bitstream, 4);
		u64 channels_bits    = bitstream_read_bits_unsafe(bitstream, 4);
		u64 bit_depth_bits   = bitstream_read_bits_unsafe(bitstream, 3);
		
		if (bit_depth_bits == 0)
		{
			block_info.bits_depth = bits_per_sample;
		}
		else
		{
			assert(bit_depth_bits != 3); // reserved
			block_info.bits_depth = u8(flac_bits_depth[bit_depth_bits]);
		}
		
		bitstream_skip_bits(bitstream, 1);
		
		u8 coded_byte0 = bitstream_read_u8(bitstream);
		
		if (coded_byte0 <= 0x7F)
		{ // 0xxx_xxxx
		}
		else if (0xC0 <= coded_byte0 && coded_byte0 <= 0xDF)
		{ // 110x_xxxx 10xx_xxxx
			bitstream_skip_bytes(bitstream, 1);
		}
		else if (0xE0 <= coded_byte0 && coded_byte0 <= 0xEF)
		{ // 1110_xxxx 10xx_xxxx 10xx_xxxx
			bitstream_skip_bytes(bitstream, 2);
		}
		else if (0xF0 <= coded_byte0 && coded_byte0 <= 0xF7)
		{ // 1111_0xxx 10xx_xxxx 10xx_xxxx 10xx_xxxx
			bitstream_skip_bytes(bitstream, 3);
		}
		else if (0xF8 <= coded_byte0 && coded_byte0 <= 0xFB)
		{ // 1111_10xx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 
			bitstream_skip_bytes(bitstream, 4);
		}
		else if (0xFC <= coded_byte0 && coded_byte0 <= 0xFD)
		{ // 1111_110x 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 
			bitstream_skip_bytes(bitstream, 5);
		}
		else if (coded_byte0 ==  0xFE)
		{        // 1111_1110 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 
			bitstream_skip_bytes(bitstream, 6);
		}
		
		switch (block_size_bits)
		{
			case 0: {
				// NOTE(fakhri): reserved
				invalid_code_path("block size using reserved bit");
			} break;
			case 1: block_info.block_size = 192; break;
			case 2: case 3: case 4: case 5: block_info.block_size = 144 << block_size_bits; break;
			case 6: block_info.block_size = u32(bitstream_read_u8(bitstream)) + 1;   break;
			case 7: block_info.block_size = u32(bitstream_read_u16be(bitstream)) + 1; break;
			default: block_info.block_size = 1ull << block_size_bits;
		}
		
		switch (sample_rate_bits)
		{
			case 0xC:
			{
				bitstream_skip_bytes(bitstream, 1);
			} break;
			case 0xD:
			{
				bitstream_skip_bytes(bitstream, 2);
			} break;
			case 0xE:
			{
				bitstream_skip_bytes(bitstream, 2);
			} break;
			case 0xF:
			{
				invalid_code_path("forbidden sample rate bits pattern");
			} break;
		}
		
		block_info.nb_channels    = flac_block_channel_count[channels_bits];
		block_info.channel_config = flac_block_channel_config[channels_bits];
		bitstream_skip_bytes(bitstream, 1);
	}
	
	// NOTE(fakhri): decode subframes
	for (u32 channel_index = 0;
		channel_index < block_info.nb_channels;
		channel_index += 1)
	{
		Flac_Subframe_Info *subframe_info = block_info.subframes_info + channel_index;
		bitstream_skip_bits(bitstream, 1);
		
		u64 subframe_type_bits = bitstream_read_bits_unsafe(bitstream, 6);
		switch (subframe_type_bits)
		{
			case 0:
			{
				subframe_info->subframe_type.kind = Subframe_Constant;
			} break;
			case 1:
			{
				subframe_info->subframe_type.kind = Subframe_Verbatim;
			} break;
			default: 
			{
				if (0x8 <= subframe_type_bits && subframe_type_bits <= 0xC)
				{
					subframe_info->subframe_type.kind = Subframe_Fixed_Prediction;
					subframe_info->subframe_type.order = u8(subframe_type_bits) - 8;
				}
				else if (0x20 <= subframe_type_bits && subframe_type_bits <= 0x3F) 
				{
					subframe_info->subframe_type.kind = Subframe_Linear_Prediction;
					subframe_info->subframe_type.order = u8(subframe_type_bits) - 31;
				}
			} break;
		}
		
		if (bitstream_read_bits_unsafe(bitstream, 1) == 1)
		{ // NOTE(fakhri): has wasted bits
			subframe_info->wasted_bits = 1;
			
			for (;bitstream_read_bits_unsafe(bitstream, 1) != 1;)
			{
				subframe_info->wasted_bits += 1;
			}
		}
		
		u8 sample_bit_depth = block_info.bits_depth - u8(subframe_info->wasted_bits);
		
		switch (block_info.channel_config)
		{
			case Left_Side: case Mid_Side:
			{
				// NOTE(fakhri): increase bit depth by 1 in case this is a side channel
				sample_bit_depth += (channel_index == 1);
			} break;
			case Side_Right:
			{
				sample_bit_depth += (channel_index == 0);
			} break;
			
			case None: case Left_Right: default: break; // nothing
		}
		
		subframe_info->samples_start_pos = bitstream->pos;
		switch (subframe_info->subframe_type.kind)
		{
			case Subframe_Constant:
			{
				bitstream_skip_bits(bitstream, sample_bit_depth);
			} break;
			
			case Subframe_Verbatim:
			{
				bitstream_skip_bits(bitstream, sample_bit_depth * block_info.block_size);
			} break;
			
			case Subframe_Fixed_Prediction:
			{
				bitstream_skip_bits(bitstream, sample_bit_depth * subframe_info->subframe_type.order);
				flac_skip_coded_residuals(bitstream, u32(block_info.block_size), subframe_info->subframe_type.order);
			} break;
			
			case Subframe_Linear_Prediction:
			{
				bitstream_skip_bits(bitstream, sample_bit_depth * subframe_info->subframe_type.order);
				
				u64 predictor_coef_precision_bits = bitstream_read_bits_unsafe(bitstream, 4);
				assert(predictor_coef_precision_bits != 0xF);
				predictor_coef_precision_bits += 1;
				
				bitstream_skip_bits(bitstream, 5 + subframe_info->subframe_type.order * predictor_coef_precision_bits);
				flac_skip_coded_residuals(bitstream, u32(block_info.block_size), subframe_info->subframe_type.order);
			} break;
		}
	}
	
	// NOTE(fakhri): decode footer
	{
		bitstream_advance_to_next_byte_boundary(bitstream);
		bitstream_read_bits_unsafe(bitstream, 16);
	}
	
	block_info.success = true;
	return block_info;
}

internal void
flac_decode_one_block(Flac_Stream *flac_stream, b32 first_block = 0)
{
	Bit_Stream *bitstream = &flac_stream->bitstream;
	Flac_Stream_Info *streaminfo = &flac_stream->streaminfo;
	flac_stream->recent_block.frames_count = 0;
	flac_stream->remaining_frames_count = 0;
	
	if (bitstream_is_empty(bitstream))
	{
		return;
	}
	
	// TODO(fakhri): each frame can be decoded in parallel
	u8 block_crc = 0;
	Flac_Stereo_Channel_Config channel_config = ZERO_STRUCT;
	u8 nb_channels = streaminfo->nb_channels;
	u32 sample_rate = streaminfo->sample_rate;
	u8 bits_depth = 0;
	
	u64 coded_number = 0;
	Flac_Block_Strategy block_strat = ZERO_STRUCT;
	
	u64 block_size = 0;
	
	// NOTE(fakhri): decode header
	{
		u64 sync_code = bitstream_read_bits_unsafe(bitstream, 15);
		assert(sync_code == 0x7ffc); // 0b111111111111100
		
		// TODO(fakhri): didn't test Variable_Size startegy yet!!
		block_strat = Flac_Block_Strategy(bitstream_read_bits_unsafe(bitstream, 1));
		if (first_block)
		{
			flac_stream->fixed_blocks = (block_strat == Fixed_Size);
		}
		else
		{
			// NOTE(fakhri): make sure the value of the block strategy doesn't change throughout the stream
			assert(flac_stream->fixed_blocks == (block_strat == Fixed_Size));
		}
		
		u64 block_size_bits  = bitstream_read_bits_unsafe(bitstream, 4);
		u64 sample_rate_bits = bitstream_read_bits_unsafe(bitstream, 4);
		u64 channels_bits    = bitstream_read_bits_unsafe(bitstream, 4);
		u64 bit_depth_bits   = bitstream_read_bits_unsafe(bitstream, 3);
		
		if (bit_depth_bits == 0)
		{
			bits_depth = streaminfo->bits_per_sample;
		}
		else
		{
			assert(bit_depth_bits != 3); // reserved
			bits_depth = u8(flac_bits_depth[bit_depth_bits]);
		}
		
		assert(bitstream_read_bits_unsafe(bitstream, 1) == 0); // reserved bit must be 0
		
		u8 coded_byte0 = bitstream_read_u8(bitstream);
		
		if (coded_byte0 <= 0x7F)
		{ // 0xxx_xxxx
			coded_number = u64(coded_byte0);
		}
		else if (0xC0 <= coded_byte0 && coded_byte0 <= 0xDF)
		{ // 110x_xxxx 10xx_xxxx
			u8 coded_byte1 = bitstream_read_u8(bitstream);;
			assert((coded_byte1 & 0xC0) == 0x80);
			coded_number = (u64(coded_byte0 & 0x1F) << 6) | u64(coded_byte1 & 0x3F);
		}
		else if (0xE0 <= coded_byte0 && coded_byte0 <= 0xEF)
		{ // 1110_xxxx 10xx_xxxx 10xx_xxxx
			u8 coded_byte1 = bitstream_read_u8(bitstream);;
			u8 coded_byte2 = bitstream_read_u8(bitstream);;
			
			assert((coded_byte1 & 0xC0) == 0x80);
			assert((coded_byte2 & 0xC0) == 0x80);
			coded_number = (u64(coded_byte0 & 0x0F) << 12) | (u64(coded_byte1 & 0x3F) << 6) | u64(coded_byte2 & 0x3F);
		}
		else if (0xF0 <= coded_byte0 && coded_byte0 <= 0xF7)
		{ // 1111_0xxx 10xx_xxxx 10xx_xxxx 10xx_xxxx
			u8 coded_byte1 = bitstream_read_u8(bitstream);;
			u8 coded_byte2 = bitstream_read_u8(bitstream);;
			u8 coded_byte3 = bitstream_read_u8(bitstream);;
			
			assert((coded_byte1 & 0xC0) == 0x80);
			assert((coded_byte2 & 0xC0) == 0x80);
			assert((coded_byte3 & 0xC0) == 0x80);
			coded_number = (u64(coded_byte0 & 0x07) << 18) | (u64(coded_byte1 & 0x3F) << 12) | (u64(coded_byte2 & 0x3F) << 6) | u64(coded_byte3 & 0x3F);
		}
		else if (0xF8 <= coded_byte0 && coded_byte0 <= 0xFB)
		{ // 1111_10xx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 
			u8 coded_byte1 = bitstream_read_u8(bitstream);
			u8 coded_byte2 = bitstream_read_u8(bitstream);
			u8 coded_byte3 = bitstream_read_u8(bitstream);
			u8 coded_byte4 = bitstream_read_u8(bitstream);
			
			assert((coded_byte1 & 0xC0) == 0x80);
			assert((coded_byte2 & 0xC0) == 0x80);
			assert((coded_byte3 & 0xC0) == 0x80);
			assert((coded_byte4 & 0xC0) == 0x80);
			coded_number = ((u64(coded_byte0 & 0x03) << 24) | 
				(u64(coded_byte1 & 0x3F) << 18) | 
				(u64(coded_byte2 & 0x3F) << 12) | 
				(u64(coded_byte3 & 0x3F) << 6)  |
				u64(coded_byte4 & 0x3F));
		}
		else if (0xFC <= coded_byte0 && coded_byte0 <= 0xFD)
		{ // 1111_110x 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 
			u8 coded_byte1 = bitstream_read_u8(bitstream);;
			u8 coded_byte2 = bitstream_read_u8(bitstream);;
			u8 coded_byte3 = bitstream_read_u8(bitstream);;
			u8 coded_byte4 = bitstream_read_u8(bitstream);;
			u8 coded_byte5 = bitstream_read_u8(bitstream);;
			
			assert((coded_byte1 & 0xC0) == 0x80);
			assert((coded_byte2 & 0xC0) == 0x80);
			assert((coded_byte3 & 0xC0) == 0x80);
			assert((coded_byte4 & 0xC0) == 0x80);
			assert((coded_byte5 & 0xC0) == 0x80);
			
			coded_number = ((u64(coded_byte0 & 0x01) << 30) | 
				(u64(coded_byte1 & 0x3F) << 24) | 
				(u64(coded_byte2 & 0x3F) << 18) | 
				(u64(coded_byte3 & 0x3F) << 12) | 
				(u64(coded_byte4 & 0x3F) << 6)  | 
				u64(coded_byte5 & 0x3F));
		}
		else if (coded_byte0 ==  0xFE)
		{        // 1111_1110 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 
			u8 coded_byte1 = bitstream_read_u8(bitstream);;
			u8 coded_byte2 = bitstream_read_u8(bitstream);;
			u8 coded_byte3 = bitstream_read_u8(bitstream);;
			u8 coded_byte4 = bitstream_read_u8(bitstream);;
			u8 coded_byte5 = bitstream_read_u8(bitstream);;
			u8 coded_byte6 = bitstream_read_u8(bitstream);;
			
			assert(block_strat == Variable_Size);
			
			assert((coded_byte1 & 0xC0) == 0x80);
			assert((coded_byte2 & 0xC0) == 0x80);
			assert((coded_byte3 & 0xC0) == 0x80);
			assert((coded_byte4 & 0xC0) == 0x80);
			assert((coded_byte5 & 0xC0) == 0x80);
			assert((coded_byte6 & 0xC0) == 0x80);
			coded_number = ((u64(coded_byte1 & 0x3F) << 30) | 
				(u64(coded_byte2 & 0x3F) << 24) | 
				(u64(coded_byte3 & 0x3F) << 18) | 
				(u64(coded_byte4 & 0x3F) << 12) | 
				(u64(coded_byte5 & 0x3F) << 6) | 
				u64(coded_byte6 & 0x3F));
		}
		
		if (flac_stream->fixed_blocks)
		{
			u64 frame_number = coded_number;
			// TODO(fakhri): check if it equals the number of frames preceding this frame
			
		}
		else
		{
			u64 first_sample_number = coded_number;
			// TODO(fakhri): check if it equals the number of samples preceding this frame
		}
		
		switch (block_size_bits)
		{
			case 0: {
				// NOTE(fakhri): reserved
				invalid_code_path("block size using reserved bit");
			} break;
			case 1: block_size = 192; break;
			case 2: case 3: case 4: case 5: block_size = 144 << block_size_bits; break;
			case 6: block_size = u32(bitstream_read_u8(bitstream)) + 1;   break;
			case 7: block_size = u32(bitstream_read_u16be(bitstream)) + 1; break;
			default: block_size = 1ull << block_size_bits;
		}
		
		switch (sample_rate_bits)
		{
			case 0: break; // NOTE(fakhri): nothing 
			case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11:
			{
				sample_rate = flac_sample_rates[sample_rate_bits - 1];
			} break;
			case 0xC:
			{
				sample_rate = u32(bitstream_read_u8(bitstream)) * 1000;
			} break;
			case 0xD:
			{
				sample_rate = u32(bitstream_read_u16be(bitstream));
			} break;
			case 0xE:
			{
				sample_rate = 10 * u32(bitstream_read_u16be(bitstream));
			} break;
			case 0xF:
			{
				invalid_code_path("forbidden sample rate bits pattern");
			} break;
		}
		
		// NOTE(fakhri): make sure the sample rate in the file doesn't change
		assert(sample_rate == streaminfo->sample_rate);
		nb_channels    = flac_block_channel_count[channels_bits];
		channel_config = flac_block_channel_config[channels_bits];
		block_crc = bitstream_read_u8(bitstream);
	}
	
	Flac_Decoded_Block *decoded_block = &flac_stream->recent_block;
	decoded_block->channels_count = nb_channels;
	decoded_block->frames_count = block_size;
	decoded_block->samples = m_arena_push_array(&flac_stream->block_arena, f32, block_size * nb_channels);
	
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	i64 *decoded_samples = m_arena_push_array(scratch.arena, i64, block_size * nb_channels);
	
	// NOTE(fakhri): decode subframes
	for (u32 channel_index = 0;
		channel_index < nb_channels;
		channel_index += 1)
	{
		u8 wasted_bits = 0;
		Flac_Subframe_Type subframe_type = ZERO_STRUCT;
		
		if (bitstream_read_bits_unsafe(bitstream, 1) != 0)
		{
			invalid_code_path("first bit must start with 0");
		}
		
		u64 subframe_type_bits = bitstream_read_bits_unsafe(bitstream, 6);
		switch (subframe_type_bits)
		{
			case 0:
			{
				subframe_type.kind = Subframe_Constant;
			} break;
			case 1:
			{
				subframe_type.kind = Subframe_Verbatim;
			} break;
			default: 
			{
				if (0x8 <= subframe_type_bits && subframe_type_bits <= 0xC)
				{
					subframe_type.kind = Subframe_Fixed_Prediction;
					subframe_type.order = u8(subframe_type_bits) - 8;
				}
				else if (0x20 <= subframe_type_bits && subframe_type_bits <= 0x3F) 
				{
					subframe_type.kind = Subframe_Linear_Prediction;
					subframe_type.order = u8(subframe_type_bits) - 31;
				}
			} break;
		}
		
		if (bitstream_read_bits_unsafe(bitstream, 1) == 1)
		{ // NOTE(fakhri): has wasted bits
			wasted_bits = 1;
			
			for (;bitstream_read_bits_unsafe(bitstream, 1) != 1;)
			{
				wasted_bits += 1;
			}
		}
		
		u8 sample_bit_depth = bits_depth - u8(wasted_bits);
		
		switch (channel_config)
		{
			case Left_Side: case Mid_Side:
			{
				// NOTE(fakhri): increase bit depth by 1 in case this is a side channel
				sample_bit_depth += (channel_index == 1);
			} break;
			case Side_Right:
			{
				sample_bit_depth += (channel_index == 0);
			} break;
			
			case None: case Left_Right: default: break; // nothing
		}
		
		switch (subframe_type.kind)
		{
			case Subframe_Constant:
			{
				i64 sample_value = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				
				for (u32 i = 0; i < block_size; i += 1)
				{
					flac_access(decoded_samples, i) = sample_value;
				}
			} break;
			
			case Subframe_Verbatim:
			{
				for (u32 i = 0; i < block_size; i += 1)
				{
					flac_access(decoded_samples, i) = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				}
			} break;
			
			case Subframe_Fixed_Prediction:
			{
				for (u32 i = 0; i < subframe_type.order; i += 1)
				{
					flac_access(decoded_samples, i) = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				}
				
				flac_decode_coded_residuals(bitstream, decoded_samples, nb_channels, channel_index, u32(block_size), subframe_type.order);
				switch (subframe_type.order)
				{
					case 0: {
					} break;
					case 1: {
						for (u32 i = subframe_type.order; i < block_size; i += 1)
						{
							flac_access(decoded_samples, i) += flac_access(decoded_samples, i - 1);
						}
					} break;
					case 2: {
						for (u32 i = subframe_type.order; i < block_size; i += 1) 
						{
							flac_access(decoded_samples, i) += 2 * flac_access(decoded_samples, i - 1) - flac_access(decoded_samples, i - 2);
						}
					} break;
					case 3: {
						for (u32 i = subframe_type.order; i < block_size; i += 1) 
						{
							flac_access(decoded_samples, i) += (3 * flac_access(decoded_samples, i - 1) - 
								3 * flac_access(decoded_samples, i - 2) +
								flac_access(decoded_samples, i - 3));
						}
					} break;
					case 4: {
						for (u32 i = subframe_type.order; i < block_size; i += 1) 
						{
							flac_access(decoded_samples, i) += (4 * flac_access(decoded_samples, i - 1) - 
								6 * flac_access(decoded_samples, i - 2) +
								4 * flac_access(decoded_samples, i - 3) -
								flac_access(decoded_samples, i - 4));
						}
					} break;
					default: {
						invalid_code_path("invalid order");
					} break;
				}
			} break;
			
			case Subframe_Linear_Prediction:
			{
				for (u32 i = 0; i < subframe_type.order; i += 1)
				{
					flac_access(decoded_samples, i) = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				}
				
				u64 predictor_coef_precision_bits = bitstream_read_bits_unsafe(bitstream, 4);
				assert(predictor_coef_precision_bits != 0xF);
				predictor_coef_precision_bits += 1;
				u64 right_shift = bitstream_read_bits_unsafe(bitstream, 5);
				
				i64 *coefficients = m_arena_push_array(&flac_stream->block_arena, i64, subframe_type.order);
				
				for (u32 i = 0; i < subframe_type.order; i += 1)
				{
					coefficients[i] = bitstream_read_sample_unencoded(bitstream, u8(predictor_coef_precision_bits));
				}
				
				flac_decode_coded_residuals(bitstream, decoded_samples, nb_channels, channel_index, u32(block_size), subframe_type.order);
				for (u32 i = subframe_type.order; i < block_size; i += 1) 
				{
					i64 predictor_value = 0;
					for (u32 j = 0; j < subframe_type.order; j += 1)
					{
						i64 c = coefficients[j];
						i64 sample_val = flac_access(decoded_samples, i - j - 1);
						predictor_value += c * sample_val;
					}
					predictor_value >>= right_shift;
					
					flac_access(decoded_samples, i) += predictor_value;
				}
			} break;
		}
		
		if (wasted_bits != 0)
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				flac_access(decoded_samples, i) <<= wasted_bits;
			}
		}
	}
	
	// NOTE(fakhri): undo inter-channel  decorrelation
	switch (channel_config)
	{
		case Left_Side:
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				i64 left = flac_access2(decoded_samples, 2, 0, i);
				i64 side = flac_access2(decoded_samples, 2, 1, i);
				
				flac_access2(decoded_samples, 2, 1, i) = left - side;
			}
		} break;
		case Side_Right:
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				i64 side  = flac_access2(decoded_samples, 2, 0, i);
				i64 right = flac_access2(decoded_samples, 2, 1, i);
				
				flac_access2(decoded_samples, 2, 0, i) = side + right;
			}
		} break;
		case Mid_Side:
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				i64 mid  = flac_access2(decoded_samples, 2, 0, i);
				i64 side = flac_access2(decoded_samples, 2, 1, i);
				
				mid = (mid << 1) + (side & 1);
				
				flac_access2(decoded_samples, 2, 0, i) = (mid + side) >> 1;
				flac_access2(decoded_samples, 2, 1, i) = (mid - side) >> 1;
			}
		} break;
		
		case None: case Left_Right: default: break; // nothing
	}
	
	u64 resample_factor = (1ull << (bits_depth - 1));
	for (u32 i = 0; i < block_size * nb_channels; i += 1)
	{
		decoded_block->samples[i] = (f32)decoded_samples[i] / f32(resample_factor);
	}
	
	// NOTE(fakhri): decode footer
	{
		bitstream_advance_to_next_byte_boundary(bitstream);
		bitstream_read_bits_unsafe(bitstream, 16);
	}
	return;
}


WORK_SIG(flac_build_seek_table_threaded)
{
	Seek_Table_Work_Data *build_seektable_work_data = (Seek_Table_Work_Data *)input;
	
	Flac_Seek_Point *seek_table = build_seektable_work_data->seek_table;
	u32 seek_points_count       = build_seektable_work_data->seek_points_count;
	Bit_Stream bitstream        = build_seektable_work_data->bitstream;
	u8 nb_channels              = build_seektable_work_data->nb_channels;
	u8 bits_per_sample          = build_seektable_work_data->bits_per_sample;
	u64 first_block_offset      = build_seektable_work_data->first_block_offset;
	u64 samples_count           = build_seektable_work_data->samples_count;
	Flac_Stream *flac_stream    = build_seektable_work_data->flac_stream;
	
	u64 samples_resolution = samples_count / seek_points_count; // 1% resolution
	
	u64 next_seek_sample = 0;
	Flac_Seek_Point *curr_seek_point = seek_table;
	Flac_Seek_Point *opl_seek_point  = seek_table + seek_points_count;
	
	u64 current_sample_number = 0;
	// NOTE(fakhri): build custom seek table
	for (;curr_seek_point != opl_seek_point && !build_seektable_work_data->cancel_req;)
	{
		Flac_Block_Info block_info = flac_preprocess_block(&bitstream, nb_channels, bits_per_sample);
		if (!block_info.success)
		{
			break;
		}
		
		if (current_sample_number >= next_seek_sample)
		{
			curr_seek_point->sample_number = current_sample_number;
			curr_seek_point->byte_offset   = block_info.start_pos.byte_index - flac_stream->first_block_pos.byte_index;
			curr_seek_point->samples_count = u16(block_info.block_size);
			
			next_seek_sample += samples_resolution;
			curr_seek_point++;
		}
		
		current_sample_number += block_info.block_size;
	}
	
	
	flac_stream->seek_table      = seek_table;
	CompletePreviousWritesBeforeFutureWrites();
	flac_stream->seek_table_size = seek_points_count;
	
	CompletePreviousWritesBeforeFutureWrites();
	build_seektable_work_data->running = false;
}


internal void
flac_build_seek_table(Flac_Stream *flac_stream, Memory_Arena *arena, Seek_Table_Work_Data *seektable_work_data)
{
	Bit_Stream bitstream = flac_stream->bitstream;
	bitstream.pos = flac_stream->first_block_pos;
	
	flac_stream->seek_table_size = 0;
	u64 samples_count = flac_stream->streaminfo.samples_count;
	
	// NOTE(fakhri): assume we always have the samples count
	assert(samples_count);
	
	
	u32 seek_points_count = 100;
	Flac_Seek_Point *seek_table = m_arena_push_array(arena, Flac_Seek_Point, seek_points_count);
	assert(seek_table);
	
	seektable_work_data->seek_table         = seek_table;
	seektable_work_data->seek_points_count  = seek_points_count;
	seektable_work_data->bitstream          = bitstream;
	seektable_work_data->nb_channels        = flac_stream->streaminfo.nb_channels;
	seektable_work_data->bits_per_sample    = flac_stream->streaminfo.bits_per_sample;
	seektable_work_data->first_block_offset = flac_stream->first_block_pos.byte_index;
	seektable_work_data->samples_count      = samples_count;
	seektable_work_data->flac_stream        = flac_stream;
	seektable_work_data->running            = true;
	seektable_work_data->cancel_req         = false;
	
	platform->push_work(flac_build_seek_table_threaded, seektable_work_data);
}

internal void
uninit_flac_stream(Flac_Stream *flac_stream)
{
	//if (flac_stream)
	{
		m_arena_free_all(&flac_stream->block_arena);
		if (flac_stream->src_ctx)
		{
			src_delete(flac_stream->src_ctx);
			flac_stream->src_ctx = 0;
		}
	}
}

internal b32
flac_process_metadata(Flac_Stream *flac_stream, Memory_Arena *arena)
{
	b32 result = 1;
	
	Bit_Stream *bitstream = &flac_stream->bitstream;
	Flac_Stream_Info *streaminfo = &flac_stream->streaminfo;
	
	u32 md_blocks_count = 0;
	b32 is_last_md_block = false;
	
	// fLaC marker
	// STREAMINFO block
	// ... metadata blocks (127 different kinds of metadata blocks)
	// audio frames
	// TODO(fakhri): this is just a hack for now, maybe we should just refuse to deal with files that 
	// don't have flac marker?
	if (bitstream_read_u32be(bitstream) != 0x664c6143)
	{
		// NOTE(fakhri): try to decode anyways?
		bitstream->pos.byte_index -= 4;
		//assert(bitstream_read_u32be(bitstream) == 0x664c6143); // "fLaC" marker
	}
	
	// NOTE(fakhri): parse meta data blocks
	for (;!is_last_md_block;)
	{
		md_blocks_count += 1;
		
		is_last_md_block = b32(bitstream_read_bits_unsafe(bitstream, 1));
		u8 md_type = u8(bitstream_read_bits_unsafe(bitstream, 7));
		
		// NOTE(fakhri): big endian
		u32 md_size = bitstream_read_u24be(bitstream);
		if (md_size == 0) break;
		
		assert(md_type != 127);
		if (md_blocks_count == 1 && md_type != 0)
		{
			result = 0;
			break;
		}
		
		switch (md_type)
		{
			case 0:
			{
				// NOTE(fakhri): streaminfo block
				#if 0
					assert(md_blocks_count == 1); // NOTE(fakhri): make sure we only have 1 streaminfo block
				#endif
					
					streaminfo->min_block_size = bitstream_read_u16be(bitstream);
				streaminfo->max_block_size = bitstream_read_u16be(bitstream);
				
				streaminfo->min_frame_size = bitstream_read_u24be(bitstream);
				streaminfo->max_frame_size = bitstream_read_u24be(bitstream);
				
				streaminfo->sample_rate     = u32(bitstream_read_bits_unsafe(bitstream, 20));
				streaminfo->nb_channels     = u8(bitstream_read_bits_unsafe(bitstream, 3)) + 1;
				streaminfo->bits_per_sample = u8(bitstream_read_bits_unsafe(bitstream, 5)) + 1;
				streaminfo->samples_count   = bitstream_read_bits_unsafe(bitstream, 36);
				
				// TODO(fakhri): handle md5 checksum
				bitstream_skip_bytes(bitstream, 16);
				
				// NOTE(fakhri): streaminfo checks
				#if 0
				{
					assert(streaminfo->min_block_size >= 16);
					assert(streaminfo->max_block_size >= streaminfo->min_block_size);
				}
				#endif
			} break;
			case 1:
			{
				// NOTE(fakhri): padding
				bitstream_skip_bytes(bitstream, md_size);
			} break;
			case 2:
			{
				// NOTE(fakhri): application
				bitstream_skip_bytes(bitstream, md_size);
			} break;
			case 3:
			{
				// NOTE(fakhri): seektable
				u64 seek_points_count = md_size / 18;
				flac_stream->seek_table = m_arena_push_array(arena, Flac_Seek_Point, seek_points_count);
				flac_stream->seek_table_size = seek_points_count;
				assert(flac_stream->seek_table);
				
				for (u32 i = 0; i < seek_points_count; i += 1)
				{
					Flac_Seek_Point *seek_point = flac_stream->seek_table + i;
					seek_point->sample_number = bitstream_read_u64be(bitstream);
					seek_point->byte_offset   = bitstream_read_u64be(bitstream);
					seek_point->samples_count = bitstream_read_u16be(bitstream);
				}
			} break;
			case 4:
			{
				// TODO(fakhri): make sure we only have one vorbis metadata block
				// vorbis comment
				u32 vendor_len = bitstream_read_u32le(bitstream);
				String8 vendor = to_string(bitstream_read_buffer(bitstream, vendor_len));
				u32 fields_count = bitstream_read_u32le(bitstream);
				for (u32  i = 0; i < fields_count; i += 1)
				{
					u32 field_len = bitstream_read_u32le(bitstream);
					String8 field = to_string(bitstream_read_buffer(bitstream, field_len));
					str8_list_push(arena, &flac_stream->vorbis_comments, field);
				}
			} break;
			case 5:
			{
				// NOTE(fakhri): cuesheet
				Buffer cuesheet_block = bitstream_read_buffer(bitstream, md_size);
			} break;
			case 6:
			{
				// NOTE(fakhri): Picture
				for (;md_size;)
				{
					Flac_Picture pic;
					
					pic.type = bitstream_read_u32be(bitstream);
					u32 media_type_string_length = bitstream_read_u32be(bitstream);
					pic.media_type_string = to_string(bitstream_read_buffer(bitstream, media_type_string_length));
					
					u32 description_length = bitstream_read_u32be(bitstream);
					pic.description = to_string(bitstream_read_buffer(bitstream, description_length));
					
					pic.dim.width      = bitstream_read_u32be(bitstream);
					pic.dim.height     = bitstream_read_u32be(bitstream);
					pic.color_depth    = bitstream_read_u32be(bitstream);
					pic.nb_colors_used = bitstream_read_u32be(bitstream);
					
					u32 picture_size = bitstream_read_u32be(bitstream);
					pic.buffer = bitstream_read_buffer(bitstream, picture_size);
					
					u32 picture_block_size = 8 * 4 + picture_size + description_length + media_type_string_length;
					assert(picture_block_size <= md_size);
					md_size -= 8 * 4 + picture_size + description_length + media_type_string_length;
					
					switch(pic.type)
					{
						case 3:
						{
							flac_stream->front_cover = m_arena_push_struct(arena, Flac_Picture);
							memory_copy_struct(flac_stream->front_cover, &pic);
						} break;
					}
				}
			} break;
			default:
			{
				bitstream_skip_bytes(bitstream, md_size);
				invalid_code_path();
			} break;
		}
	}
	
	return result;
}

internal b32 
init_flac_stream(Flac_Stream *flac_stream, Memory_Arena *arena, Buffer data)
{
	b32 result = 0;
	// NOTE(fakhri): init bitstream
	{
		flac_stream->bitstream.buffer = data;
		flac_stream->bitstream.pos.byte_index = 0;
		flac_stream->bitstream.pos.bits_left  = 8;
	}
	
	result = flac_process_metadata(flac_stream, arena);
	if (result)
	{
		flac_stream->first_block_pos = flac_stream->bitstream.pos;
		
		flac_stream->bitstream.pos = flac_stream->first_block_pos;
		flac_decode_one_block(flac_stream, 1);
	}
	
	return result;
}

struct Decoded_Samples
{
	f32 *samples;
	u64 frames_count;
	u64 channels_count;
};

internal Decoded_Samples
flac_read_samples(Flac_Stream *flac_stream, u64 requested_frames_count, u32 requested_sample_rate, Memory_Arena *arena)
{
	Decoded_Samples result = ZERO_STRUCT;
	u64 remaining_frames_count = requested_frames_count;
	result.samples = m_arena_push_array(arena, f32, requested_frames_count * flac_stream->streaminfo.nb_channels);
	result.channels_count = flac_stream->streaminfo.nb_channels;
	
	if (!flac_stream->src_ctx)
	{
		int error;
		flac_stream->src_ctx = src_new(SRC_SINC_BEST_QUALITY, int(result.channels_count), &error);
		assert(flac_stream->src_ctx);
	}
	
	SRC_STATE *src_ctx = flac_stream->src_ctx;
	
	for (;remaining_frames_count != 0;)
	{
		if (flac_stream->remaining_frames_count != 0)
		{
			u64 nb_channels = flac_stream->recent_block.channels_count;
			u64 in_offset = (flac_stream->recent_block.frames_count - flac_stream->remaining_frames_count) * nb_channels;
			f32 *in_samples = flac_stream->recent_block.samples + in_offset;
			
			u64 out_offset   = result.frames_count * nb_channels;
			f32 *out_samples = result.samples + out_offset;
			
			SRC_DATA data = {
				.data_in  = in_samples,
				.data_out = out_samples,
				.input_frames  = int(flac_stream->remaining_frames_count),
				.output_frames = int(remaining_frames_count),
				.end_of_input = 0,
				.src_ratio = f64(requested_sample_rate) / f64(flac_stream->streaminfo.sample_rate)
			};
			src_process(src_ctx, &data);
			
			flac_stream->next_sample_number     += data.input_frames_used;
			flac_stream->remaining_frames_count -= data.input_frames_used;
			
			remaining_frames_count -= data.output_frames_gen;
			result.frames_count    += data.output_frames_gen;
		}
		else
		{
			m_arena_free_all(&flac_stream->block_arena);
			flac_decode_one_block(flac_stream);
			flac_stream->remaining_frames_count = flac_stream->recent_block.frames_count;
			
			if (flac_stream->recent_block.frames_count == 0)
			{
				// TODO(fakhri): should we flush the src_ctx?
				// NOTE(fakhri): EOF
				break;
			}
		}
	}
	
	return result;
}

internal void
flac_seek_stream(Flac_Stream *flac_stream, u64 target_sample)
{
	if (flac_stream->seek_table)
	{
		Flac_Seek_Point *best_seek_point = flac_stream->seek_table;
		for (u32 i = 1; i < flac_stream->seek_table_size; i += 1)
		{
			Flac_Seek_Point *seek_point = flac_stream->seek_table + i;
			if (seek_point->sample_number > target_sample)
			{
				break;
			}
			best_seek_point = seek_point;
		}
		
		flac_stream->bitstream.pos = flac_stream->first_block_pos;
		flac_stream->bitstream.pos.byte_index += best_seek_point->byte_offset;
		flac_stream->next_sample_number = best_seek_point->sample_number;
	}
	
	if (flac_stream->src_ctx)
	{
		src_delete(flac_stream->src_ctx);
		flac_stream->src_ctx = 0;
	}
	
	for (;target_sample > flac_stream->next_sample_number;)
	{
		Flac_Block_Info block_info = flac_preprocess_block(&flac_stream->bitstream, flac_stream->streaminfo.nb_channels, flac_stream->streaminfo.bits_per_sample);;
		if (!block_info.success)
		{
			break;
		}
		
		if (flac_stream->next_sample_number + block_info.block_size >= target_sample)
		{
			Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
			if (target_sample > flac_stream->next_sample_number)
				flac_read_samples(flac_stream, target_sample - flac_stream->next_sample_number, flac_stream->streaminfo.sample_rate, scratch.arena);
		}
		else
		{
			flac_stream->next_sample_number += block_info.block_size;
		}
	}
}
