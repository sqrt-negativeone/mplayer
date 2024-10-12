
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


struct Flac_Channel_Samples
{
	i64 *samples;
	u64 count;
};

struct Flac_Decoded_Block
{
	Flac_Channel_Samples samples_per_channel[FLAC_MAX_CHANNEL_COUNT];
	u32 channels_count;
	u64 interchannel_sample_count;
};


struct Flac_Stream 
{
	b32 done;
	Bit_Stream bitstream;
	Flac_Stream_Info streaminfo;
	Memory_Arena *block_arena;
	Flac_Decoded_Block recent_block;
	u64 remaining_frames_count;
};



internal void
init_flac_stream(Flac_Stream *flac_stream, Buffer data)
{
	// NOTE(fakhri): init bitstream
	{
		flac_stream->bitstream.buffer = data;
		flac_stream->bitstream.byte_index = 0;
		flac_stream->bitstream.bits_left  = 8;
	}
	
	flac_stream->block_arena = m_arena_make(megabytes(8));
	
	Bit_Stream *bitstream = &flac_stream->bitstream;
	Flac_Stream_Info *streaminfo = &flac_stream->streaminfo;
	
	u32 md_blocks_count = 0;
	b32 is_last_md_block = false;
	
	// fLaC marker
	// STREAMINFO block
	// ... metadata blocks (127 different kinds of metadata blocks)
	// audio frames
	assert(bitstream_read_u32be(bitstream) == 0x664c6143); // "fLaC" marker
	// NOTE(fakhri): parse meta data blocks
	for (;!is_last_md_block;)
	{
		md_blocks_count += 1;
		
		is_last_md_block = b32(bitstream_read_bits_unsafe(bitstream, 1));
		u8 md_type = u8(bitstream_read_bits_unsafe(bitstream, 7));
		
		// NOTE(fakhri): big endian
		u32 md_size = bitstream_read_u24be(bitstream);
		
		assert(md_type != 127);
		if (md_blocks_count == 1)
		{
			// NOTE(fakhri): make sure the first meta data block is a streaminfo block
			// as per specification
			assert(md_type == 0);
		}
		
		switch (md_type)
		{
			case 0:
			{
				// NOTE(fakhri): streaminfo block
				assert(md_blocks_count == 1); // NOTE(fakhri): make sure we only have 1 streaminfo block
				
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
				{
					assert(streaminfo->min_block_size >= 16);
					assert(streaminfo->max_block_size >= streaminfo->min_block_size);
				}
			} break;
			case 1:
			{
				// NOTE(fakhri): padding
				bitstream_skip_bytes(bitstream, int(md_size));
			} break;
			case 2:
			{
				// NOTE(fakhri): application
				bitstream_skip_bytes(bitstream, int(md_size));
			} break;
			case 3:
			{
				// NOTE(fakhri): seektable
				bitstream_skip_bytes(bitstream, int(md_size));
			} break;
			case 4:
			{
				// vorbis comment
				bitstream_skip_bytes(bitstream, int(md_size));
			} break;
			case 5:
			{
				// NOTE(fakhri): cuesheet
				bitstream_skip_bytes(bitstream, int(md_size));
			} break;
			case 6:
			{
				// NOTE(fakhri): Picture
				bitstream_skip_bytes(bitstream, int(md_size));
			} break;
			default:
			{
				bitstream_skip_bytes(bitstream, int(md_size));
				invalid_code_path();
			} break;
		}
	}
	
	return;
}

internal void
flac_decode_coded_residuals(Bit_Stream *bitstream, Flac_Channel_Samples *block_samples, u32 block_size, u32 order)
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
					block_samples->samples[sample_index] = bitstream_read_sample_unencoded(bitstream, residual_bits_precision, 0);
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
				
				block_samples->samples[sample_index] = sample_value;
				sample_index += 1;
			}
		}
	}
}

internal void
flac_decode_one_block(Flac_Stream *flac_stream)
{
	Bit_Stream *bitstream = &flac_stream->bitstream;
	Flac_Stream_Info *streaminfo = &flac_stream->streaminfo;
	flac_stream->recent_block.interchannel_sample_count = 0;
	flac_stream->remaining_frames_count = 0;
	
	if (bitstream_is_empty(bitstream))
	{
		flac_stream->done = true;
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
		// TODO(fakhri): make sure that this value doesn't change through the stream
		block_strat = Flac_Block_Strategy(bitstream_read_bits_unsafe(bitstream, 1));
		
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
		
		nb_channels    = flac_block_channel_count[channels_bits];
		channel_config = flac_block_channel_config[channels_bits];
		block_crc = bitstream_read_u8(bitstream);
	}
	
	Flac_Decoded_Block *decoded_block = &flac_stream->recent_block;
	decoded_block->channels_count = nb_channels;
	decoded_block->interchannel_sample_count = block_size;
	
	// NOTE(fakhri): decode subframes
	for (u32 channel_index = 0;
		channel_index < nb_channels;
		channel_index += 1)
	{
		Flac_Channel_Samples *block_channel_samples = decoded_block->samples_per_channel + channel_index;
		block_channel_samples->samples = m_arena_push_array(flac_stream->block_arena, i64, block_size);
		block_channel_samples->count = block_size;
		assert(block_channel_samples->samples);
		
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
			default: break; // nothing
		}
		
		switch (subframe_type.kind)
		{
			case Subframe_Constant:
			{
				i64 sample_value = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				
				for (u32 i = 0; i < block_size; i += 1)
				{
					block_channel_samples->samples[i] = sample_value;
				}
			} break;
			
			case Subframe_Verbatim:
			{
				for (u32 i = 0; i < block_size; i += 1)
				{
					block_channel_samples->samples[i] = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				}
			} break;
			
			case Subframe_Fixed_Prediction:
			{
				for (u32 i = 0; i < subframe_type.order; i += 1)
				{
					block_channel_samples->samples[i] = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				}
				
				flac_decode_coded_residuals(bitstream, block_channel_samples, u32(block_size), subframe_type.order);
				i64 *samples = block_channel_samples->samples;
				switch (subframe_type.order)
				{
					case 0: {
					} break;
					case 1: {
						for (u32 i = subframe_type.order; i < block_size; i += 1)
							samples[i] += samples[i - 1];
					} break;
					case 2: {
						for (u32 i = subframe_type.order; i < block_size; i += 1) 
							samples[i] += 2 * samples[i - 1] - samples[i - 2];
					} break;
					case 3: {
						for (u32 i = subframe_type.order; i < block_size; i += 1) 
							samples[i] += 3 * samples[i - 1] - 3 * samples[i - 2] + samples[i - 3];
					} break;
					case 4: {
						for (u32 i = subframe_type.order; i < block_size; i += 1) 
							samples[i] += 4 * samples[i - 1] - 6 * samples[i - 2] + 4 * samples[i - 3] - samples[i - 4];
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
					block_channel_samples->samples[i] = bitstream_read_sample_unencoded(bitstream, sample_bit_depth);
				}
				
				u64 predictor_coef_precision_bits = bitstream_read_bits_unsafe(bitstream, 4);
				assert(predictor_coef_precision_bits != 0xF);
				predictor_coef_precision_bits += 1;
				u64 right_shift = bitstream_read_bits_unsafe(bitstream, 5);
				
				i64 *coefficients = m_arena_push_array(flac_stream->block_arena, i64, subframe_type.order);
				
				for (u32 i = 0; i < subframe_type.order; i += 1)
				{
					coefficients[i] = bitstream_read_sample_unencoded(bitstream, u8(predictor_coef_precision_bits));
				}
				
				flac_decode_coded_residuals(bitstream, block_channel_samples, u32(block_size), subframe_type.order);
				i64 *samples = block_channel_samples->samples;
				
				for (u32 i = subframe_type.order; i < block_size; i += 1) 
				{
					i64 predictor_value = 0;
					for (u32 j = 0; j < subframe_type.order; j += 1)
					{
						i64 c = coefficients[j];
						i64 sample_val = samples[i - j - 1];
						predictor_value += c * sample_val;
					}
					predictor_value >>= right_shift;
					samples[i] += predictor_value;
				}
			} break;
		}
		
		if (wasted_bits != 0)
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				block_channel_samples->samples[i] <<= wasted_bits;
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
				i64 left = decoded_block->samples_per_channel[0].samples[i]; 
				i64 side = decoded_block->samples_per_channel[1].samples[i];
				
				decoded_block->samples_per_channel[1].samples[i] = left - side;
			}
		} break;
		case Side_Right:
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				i64 side  = decoded_block->samples_per_channel[0].samples[i]; 
				i64 right = decoded_block->samples_per_channel[1].samples[i];
				
				decoded_block->samples_per_channel[0].samples[i] = side + right;
			}
		} break;
		case Mid_Side:
		{
			for (u32 i = 0; i < block_size; i += 1)
			{
				i64 mid  = decoded_block->samples_per_channel[0].samples[i]; 
				i64 side = decoded_block->samples_per_channel[1].samples[i];
				mid = (mid << 1) + (side & 1);
				
				decoded_block->samples_per_channel[0].samples[i] = (mid + side) >> 1;
				decoded_block->samples_per_channel[1].samples[i] = (mid - side) >> 1;
			}
		} break;
		default: break; // nothing
	}
	
	// NOTE(fakhri): decode footer
	{
		bitstream_advance_to_next_byte_boundary(bitstream);
		bitstream_read_bits_unsafe(bitstream, 16);
	}
	return;
}

struct Decoded_Samples
{
	f32 *samples;
	u64 frames_count;
	u64 channels_count;
};

internal Decoded_Samples
flac_read_samples(Flac_Stream *flac_stream, u64 requested_frames_count, Memory_Arena *arena)
{
	Decoded_Samples result = ZERO_STRUCT;
	u64 remaining_frames_count = requested_frames_count;
	result.samples = m_arena_push_array(arena, f32, requested_frames_count * flac_stream->streaminfo.nb_channels);
	result.channels_count = flac_stream->streaminfo.nb_channels;
	
	u64 resample_factor = (1ull << (flac_stream->streaminfo.bits_per_sample - 1));
	
	for (;remaining_frames_count != 0;)
	{
		if (flac_stream->remaining_frames_count != 0)
		{
			u64 frames_to_copy = MIN(flac_stream->remaining_frames_count, remaining_frames_count);
			u64 offset = flac_stream->recent_block.interchannel_sample_count - flac_stream->remaining_frames_count;
			u64 nb_channels = flac_stream->recent_block.channels_count;
			for (u32 index = 0; index < frames_to_copy; index += 1)
			{
				for (u32 c_index = 0; c_index < nb_channels; c_index += 1)
				{
					Flac_Channel_Samples *channel_samples = flac_stream->recent_block.samples_per_channel + c_index;
					u64 output_offset = (index + result.frames_count) * nb_channels + c_index;
					result.samples[output_offset] = f32(channel_samples->samples[offset + index]) / f32(resample_factor);
				}
			}
			flac_stream->remaining_frames_count -= frames_to_copy;
			remaining_frames_count -= frames_to_copy;
			result.frames_count += frames_to_copy;
		}
		else {
			m_arena_free_all(flac_stream->block_arena);
			flac_decode_one_block(flac_stream);
			flac_stream->remaining_frames_count = flac_stream->recent_block.interchannel_sample_count;
			
			if (flac_stream->recent_block.interchannel_sample_count == 0) {
				// NOTE(fakhri): EOF
				break;
			}
		}
	}
	return result;
}
