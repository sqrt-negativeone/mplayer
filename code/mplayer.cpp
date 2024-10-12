
#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

typedef Buffer Read_Entire_File(String8 file_path, Memory_Arena *arena);

struct MPlayer_Context
{
	Memory_Arena *main_arena;
	Memory_Arena *frame_arena;
	Render_Context *render_ctx;
	
	Flac_Stream flac_stream;
	Buffer flac_file_buffer;
	
	Read_Entire_File *read_entire_file;
};

internal void
mplayer_get_audio_samples(MPlayer_Context *mplayer, void *output_buf, u32 frame_count)
{
	Flac_Stream *flac_stream = &mplayer->flac_stream;
	if (flac_stream)
	{
		u32 channels_count = flac_stream->streaminfo.nb_channels;
		
		Memory_Checkpoint scratch = get_scratch(0, 0);
	Decoded_Samples streamed_samples = flac_read_samples(flac_stream, frame_count, scratch.arena);
memory_copy(output_buf, streamed_samples.samples, streamed_samples.frames_count * channels_count * sizeof(f32));
	}
}

internal void
mplayer_initialize(MPlayer_Context *mplayer)
{
	mplayer->flac_file_buffer = mplayer->read_entire_file(str8_lit("fear_inoculum.flac"), mplayer->main_arena);
	init_flac_stream(&mplayer->flac_stream, mplayer->flac_file_buffer);
	
}

internal void
mplayer_update_and_render(MPlayer_Context *mplayer)
{
	Render_Context *render_ctx = mplayer->render_ctx;
	
	push_clear_color(render_ctx, vec4(0.1f, 0.1f, 0.1f, 1));
}
