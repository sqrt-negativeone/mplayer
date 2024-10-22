

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"


#include "third_party/samplerate.h"

struct Sound_Config
{
	u32 sample_rate;
	u16 channels_count;
};

struct File_Iterator_Handle
{
	u8 opaque[kilobytes(1)];
};

enum File_Flag
{
	FileFlag_Directory,
	FileFlag_Valid,
};
typedef u32 File_Flags;

struct File_Info
{
	File_Flags flags;
	String8 path;
	String8 name;
	String8 extension;
	u64 size;
};

typedef b32       File_Iter_Begin_Proc(File_Iterator_Handle *it, String8 path);
typedef File_Info File_Iter_Next_Proc (File_Iterator_Handle *it, Memory_Arena *arena);
typedef void      File_Iter_End_Proc  (File_Iterator_Handle *it);

typedef Buffer Load_Entire_File(String8 file_path, Memory_Arena *arena);

#include "mplayer_renderer.cpp"
#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#include "mplayer_input.cpp"

struct Mplayer_Glyph
{
	V2_F32 uv_scale;
	V2_F32 uv_offset;
	V2_F32 dim;
	V2_F32 offset;
	f32 advance;
};

struct Mplayer_Font
{
	b32 monospaced;
	b32 loaded;
	Texture atlas_tex;
	Buffer pixels_buf;
	Mplayer_Glyph *glyphs;
	u32 first_glyph_index;
	u32 opl_glyph_index;
	f32 line_advance;
	f32 ascent;
	f32 descent;
};


struct Mplayer_UI_Interaction
{
	b32 hover;
	b32 pressed;
	b32 released;
	b32 clicked; // pressed widget and released inside the widget
};

struct Mplayer_UI
{
	Render_Group *group;
	Mplayer_Input *input;
	V2_F32 mouse_p;
	
	u64 active_widget_id;
	u64 hot_widget_id;
	
	f32 active_t;
	f32 active_dt;
	
	f32 hot_t;
	f32 hot_dt;
};

struct Mplayer_OS_Vtable
{
	File_Iter_Begin_Proc *file_iter_begin;
	File_Iter_Next_Proc  *file_iter_next;
	File_Iter_End_Proc   *file_iter_end;
	Load_Entire_File *load_entire_file;
};

struct Mplayer_Music_Track
{
	Mplayer_Music_Track *next;
	
	Memory_Arena arena;
	String8 path;
	String8 name;
	
	Memory_Arena transient_arena;
	Flac_Stream *flac_stream;
	b32 file_loaded;
	Buffer flac_file_buffer;
	
	Texture cover_texture;
};

enum Mplayer_Mode
{
	MODE_Library,
	MODE_Lyrics,
};

struct Mplayer_Context
{
	Memory_Arena main_arena;
	Memory_Arena frame_arena;
	Render_Context *render_ctx;
	Mplayer_UI ui;
	Mplayer_Input input;
	
	b32 play_track;
	
	Mplayer_Font timestamp_font;
	Mplayer_Font font;
	Mplayer_Font debug_font;
	f32 volume;
	f32 seek_percentage;
	
	String8 library_path;
	
	Mplayer_OS_Vtable os;
	
	Mplayer_Music_Track *first_music;
	Mplayer_Music_Track *last_music;
	u64 music_count;
	Mplayer_Mode mode;
	
	Mplayer_Music_Track *current_music;
	
	f32 music_view_scroll;
	f32 music_view_scroll_speed;
	f32 music_view_target_scroll;
};

internal Mplayer_Music_Track *
mplayer_make_music_track(Mplayer_Context *mplayer)
{
	Mplayer_Music_Track *result = 0;
	result = m_arena_push_struct_z(&mplayer->main_arena, Mplayer_Music_Track);
	assert(result);
	return result;
}

internal void
mplayer_push_music_track(Mplayer_Context *mplayer, Mplayer_Music_Track *music_track)
{
	mplayer->music_count += 1;
	if (!mplayer->last_music)
	{
		mplayer->last_music = mplayer->first_music = music_track;
	}
	else
	{
		mplayer->last_music->next = music_track;
		mplayer->last_music = music_track;
	}
}

internal void
mplayer_music_reset(Mplayer_Music_Track *music_track)
{
	if (music_track && music_track->flac_stream)
	{
		music_track->flac_stream->bitstream.pos = music_track->flac_stream->first_block_pos;
	}
}

internal Texture
mplayer_load_texture(Mplayer_Context *mplayer, Buffer buffer)
{
	Texture result = ZERO_STRUCT;
	int width;
	int height;
	int channels;
	u8 *pixels = stbi_load_from_memory(buffer.data, int(buffer.size), &width, &height, &channels, 3);
	assert(pixels);
	if (pixels)
	{
		result = reserve_texture_handle(mplayer->render_ctx, u16(width), u16(height));
		#if 1
			// TODO(fakhri): maybe we should have the channel count be stored in the texure handle?
			if (channels == 3)
		{
			set_flag(result.flags, TEXTURE_FLAG_RGB_BIT);
		}
		#endif
			
			Buffer pixels_buf = arena_push_buffer(&mplayer->frame_arena, width * height * channels);
		memory_copy(pixels_buf.data, pixels, pixels_buf.size);
		
		push_texture_upload_request(&mplayer->render_ctx->upload_buffer, result, pixels_buf);
		stbi_image_free(pixels);
	}
	return result;
}

internal void
mplayer_load_music_track(Mplayer_Context *mplayer, Mplayer_Music_Track *music_track)
{
	if (!music_track->file_loaded)
	{
		music_track->flac_file_buffer = mplayer->os.load_entire_file(music_track->path, &music_track->transient_arena);
		music_track->file_loaded = true;
	}
	
	if (!music_track->flac_stream)
	{
		music_track->flac_stream = m_arena_push_struct_z(&music_track->transient_arena, Flac_Stream);
		init_flac_stream(music_track->flac_stream, &music_track->transient_arena, music_track->flac_file_buffer);
		if (music_track->flac_stream->front_cover)
		{
			Flac_Picture *front_cover = music_track->flac_stream->front_cover;
			music_track->cover_texture = mplayer_load_texture(mplayer, front_cover->buffer);
		}
	}
}

internal void
mplayer_unload_music_track(Mplayer_Context *mplayer, Mplayer_Music_Track *music)
{
	uninit_flac_stream(music->flac_stream);
	music->file_loaded = false;
	music->flac_stream = 0;
	music->flac_file_buffer = ZERO_STRUCT;
	m_arena_free_all(&music->transient_arena);
	
	// TODO(fakhri): delete texture from gpu memory!
	music->cover_texture = ZERO_STRUCT;
}

internal void
mplayer_play_music_track(Mplayer_Context *mplayer, Mplayer_Music_Track *music_track)
{
	if (mplayer->current_music != music_track)
	{
		Mplayer_Music_Track *prev_music = mplayer->current_music;
		
		if (prev_music)
		{
			mplayer_unload_music_track(mplayer, prev_music);
		}
		
		mplayer_load_music_track(mplayer, music_track);
		mplayer_music_reset(music_track);
		mplayer->current_music = music_track;
	}
}

internal b32
is_music_track_still_playing(Mplayer_Music_Track *music_track)
{
	b32 result = (music_track && music_track->flac_stream && 
		!bitstream_is_empty(&music_track->flac_stream->bitstream));
	
	return result;
}

internal void
load_font(Mplayer_Context *mplayer, Mplayer_Font *font, String8 font_path, f32 font_size)
{
	if (font->loaded)
	{
		return;
	}
	
	V2_I32 atlas_dim = vec2i(1024, 1024);
  
	Memory_Checkpoint scratch = begin_scratch(0, 0);
  
	Buffer font_content = mplayer->os.load_entire_file(font_path, scratch.arena);
	assert(font_content.data);
  
	Buffer pixels_buf = arena_push_buffer(&mplayer->main_arena, atlas_dim.width * atlas_dim.height);
	assert(pixels_buf.data);
  
	f32 ascent   = 0;
	f32 descent  = 0;
	f32 line_gap = 0;
	stbtt_GetScaledFontVMetrics((const unsigned char *)font_content.data, 0, font_size, &ascent, &descent, &line_gap);
	
	f32 line_advance = ascent - descent + line_gap;
  
	u32 first_glyph_index = ' ';
	u32 opl_glyph_index = 128;
  
	stbtt_pack_context ctx;
  
	stbtt_PackBegin(&ctx, pixels_buf.data, atlas_dim.width, atlas_dim.height, 0, 1, 0);
	stbtt_PackSetOversampling(&ctx, 1, 1);
	stbtt_packedchar *chardata_for_range = m_arena_push_array(scratch.arena, stbtt_packedchar, opl_glyph_index-first_glyph_index);
	assert(chardata_for_range);
  
	stbtt_pack_range rng = ZERO_STRUCT;
	{
		rng.font_size = font_size;
		rng.first_unicode_codepoint_in_range = (i32)first_glyph_index;
		rng.array_of_unicode_codepoints = 0;
		rng.num_chars = (i32)(opl_glyph_index - first_glyph_index);
		rng.chardata_for_range = chardata_for_range;
	}
  
	stbtt_PackFontRanges(&ctx, (const unsigned char *)font_content.data, 0, &rng, 1);
	stbtt_PackEnd(&ctx);
  
	b32 monospaced = true;
	f32 glyph_advance = -1;
	
	// NOTE(fakhri): build direct map
	Mplayer_Glyph *glyphs = m_arena_push_array(&mplayer->main_arena, Mplayer_Glyph, opl_glyph_index - first_glyph_index);
	assert(glyphs);
  
	for (u32 ch = first_glyph_index; 
		ch < opl_glyph_index;
		ch += 1)
	{
		u32 index = ch - first_glyph_index;
		Mplayer_Glyph *glyph = glyphs + index;;
		
		f32 x_offset = 0;
		f32 y_offset = 0;
		
		stbtt_aligned_quad quad;
		stbtt_GetPackedQuad(rng.chardata_for_range, atlas_dim.x, atlas_dim.y, (i32)index, &x_offset, &y_offset, &quad, true);
		
		glyph->uv_offset = vec2(quad.s0, quad.t0);
		glyph->uv_scale  = vec2(ABS(quad.s1 - quad.s0), ABS(quad.t1 - quad.t0));
		glyph->dim      = glyph->uv_scale * vec2(atlas_dim);
		glyph->offset    = vec2(quad.x0 + 0.5f * glyph->dim.width, -(quad.y0 + 0.5f * glyph->dim.height));
		glyph->advance   = x_offset;
		
		glyph_advance = (glyph_advance == -1)? glyph->advance:glyph_advance;
		monospaced = monospaced && (glyph_advance == glyph->advance);
	}
  
	font->loaded = true;
	font->first_glyph_index = first_glyph_index;
	font->opl_glyph_index   = opl_glyph_index;
	font->glyphs = glyphs;
	font->line_advance = line_advance;
	font->ascent = ascent;
	font->descent = descent;
  
	font->pixels_buf = pixels_buf;
	font->atlas_tex = reserve_texture_handle(mplayer->render_ctx, (u16)atlas_dim.width, (u16)atlas_dim.height);
	set_flag(font->atlas_tex.flags, TEXTURE_FLAG_GRAY_BIT);
	push_texture_upload_request(&mplayer->render_ctx->upload_buffer, font->atlas_tex, pixels_buf);
}

internal Mplayer_Glyph
font_get_glyph_from_char(Mplayer_Font *font, u8 ch)
{
	Mplayer_Glyph glyph = ZERO_STRUCT;
	if (ch >= font->first_glyph_index && ch < font->opl_glyph_index) 
	{
		glyph = font->glyphs[ch - font->first_glyph_index];
	}
	else
	{
		// TODO(fakhri): add utf8 support
		glyph = font_get_glyph_from_char(font, '?');
	}
	return glyph;
}

internal V2_F32
font_compute_text_dim(Mplayer_Font *font, String8 text)
{
	V2_F32 result;
	result.height = font->ascent - font->descent;
	result.width = 0;
	
	if (font->monospaced)
	{
		Mplayer_Glyph glyph = font_get_glyph_from_char(font, ' ');
		result.width = text.len * glyph.advance;
	}
	else 
	{
		for (u32 i = 0; i < text.len; i += 1)
		{
			u8 ch = text.str[i];
			Mplayer_Glyph glyph = font_get_glyph_from_char(font, ch);
			result.width += glyph.advance;
		}
	}
	
	return result;
}

internal void
draw_text(Render_Group *group, Mplayer_Font *font, V2_F32 pos, V4_F32 color, String8 text)
{
	// NOTE(fakhri): render the text
	V3_F32 text_pt = vec3(pos, 0);
	for (u32 offset = 0;
		offset < text.len;
		offset += 1)
	{
		u8 ch = text.str[offset];
		// TODO(fakhri): utf8 support
		Mplayer_Glyph glyph = font_get_glyph_from_char(font, ch);
		V3_F32 glyph_p = vec3(text_pt.xy + (glyph.offset), 0);
		push_image(group, 
			glyph_p, 
			glyph.dim,
			font->atlas_tex,
			color,
			0.0f,
			glyph.uv_scale, 
			glyph.uv_offset);
		
		text_pt.x += glyph.advance;
	}
}

internal void
draw_text_f(Render_Group *group, Mplayer_Font *font, V2_F32 pos, V4_F32 color, const char *fmt, ...)
{
	Memory_Checkpoint scratch = get_scratch(0, 0);
	va_list args;
	va_start(args, fmt);
	String8 text = str8_fv(scratch.arena, fmt, args);
	va_end(args);
	
	draw_text(group, font, pos, color, text);
}

internal void
draw_text_centered(Render_Group *group, Mplayer_Font *font, V2_F32 pos, V4_F32 color, String8 text)
{
	V2_F32 text_dim = font_compute_text_dim(font, text);
	pos.x -= 0.5f * text_dim.width;
	pos.y -= 0.25f * text_dim.height;
	draw_text(group, font, pos, color, text);
}

internal void
draw_text_centered_f(Render_Group *group, Mplayer_Font *font, V2_F32 pos, V4_F32 color, const char *fmt, ...)
{
	Memory_Checkpoint scratch = get_scratch(0, 0);
	va_list args;
	va_start(args, fmt);
	String8 text = str8_fv(scratch.arena, fmt, args);
	va_end(args);
	
	draw_text_centered(group, font, pos, color, text);
}

internal void
draw_circle(Render_Group *group, V2_F32 pos, f32 radius, V4_F32 color)
{
	push_rect(group, pos, vec2(2 * radius), color, radius);
}

internal void
draw_outline(Render_Group *group, V3_F32 pos, V2_F32 dim, f32 thickness, V4_F32 color = vec4(1, 1, 1, 1))
{
	// left
	push_rect(group,
		vec3(pos.x - 0.5f * (dim.width - thickness), pos.y, pos.z),
		vec2(thickness, dim.height),
		color);
  
	// right
	push_rect(group,
		vec3(pos.x + 0.5f * (dim.width - thickness), pos.y, pos.z),
		vec2(thickness, dim.height),
		color);
  
	// top
	push_rect(group,
		vec3(pos.x, pos.y + 0.5f * (dim.height - thickness), pos.z),
		vec2(dim.width, thickness),
		color);
  
	// bottom
	push_rect(group,
		vec3(pos.x, pos.y - 0.5f * (dim.height - thickness), pos.z),
		vec2(dim.width, thickness),
		color);
}

internal void
draw_outline(Render_Group *group, V2_F32 pos, V2_F32 dim, f32 thickness, V4_F32 color = vec4(1, 1, 1, 1))
{
	draw_outline(group, vec3(pos, 0), dim, thickness, color);
}

internal void
ui_begin(Mplayer_UI *ui, Render_Group *group, Mplayer_Input *input, V2_F32 mouse_p)
{
	ui->group   = group;
	ui->input   = input;
	ui->mouse_p = mouse_p;
}

#define ui_widget_interaction(ui, widget_pos, widget_dim) _ui_widget_interaction(ui, __LINE__, widget_pos, widget_dim)

internal Mplayer_UI_Interaction
_ui_widget_interaction(Mplayer_UI *ui, u64 id, V2_F32 widget_pos, V2_F32 widget_dim)
{
	Mplayer_UI_Interaction interaction = ZERO_STRUCT;
	
	if (ui->hot_widget_id == id && mplayer_button_released(ui->input, Key_LeftMouse))
	{
		interaction.released = 1;
	}
	
	if (is_in_range(range_center_dim(widget_pos, widget_dim), ui->mouse_p))
	{
		interaction.hover = 1;
		if (ui->active_widget_id != id)
		{
			ui->active_widget_id = id;
			ui->active_t = 0;
			ui->active_dt = 8.0f;
		}
	}
	else
	{
		if (ui->active_widget_id == id)
		{
			ui->active_widget_id = 0;
		}
		
		if (interaction.released)
		{
			ui->hot_widget_id = 0;
		}
	}
	
	if (ui->active_widget_id == id)
	{
		ui->active_t += ui->input->frame_dt * ui->active_dt;
		ui->active_t = CLAMP(0, ui->active_t, 1.0f);
		
		if (mplayer_button_clicked(ui->input, Key_LeftMouse))
		{
			ui->hot_widget_id = id;
			ui->hot_t = 0;
			ui->hot_dt = 20.0f;
		}
		
		if (ui->hot_widget_id == id && mplayer_button_released(ui->input, Key_LeftMouse))
		{
			interaction.clicked = 1;
			ui->hot_widget_id = 0;
		}
	}
	
	
	if (ui->hot_widget_id == id)
	{
		interaction.pressed = true;
		ui->hot_t += ui->input->frame_dt * ui->hot_dt;
		ui->hot_t = CLAMP(0, ui->hot_t, 1.0f);
	}
	return interaction;
}

#define ui_slider_f32(ui, value, min, max, slider_pos, slider_dim, progress) _ui_slider_f32(ui, value, min, max, slider_pos, slider_dim, progress,  __LINE__)
internal Mplayer_UI_Interaction
_ui_slider_f32(Mplayer_UI *ui, f32 *value, f32 min, f32 max, V2_F32 slider_pos, V2_F32 slider_dim, b32 progress, u64 id)
{
	Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, id, slider_pos, slider_dim);
	V4_F32 color = vec4(0.3f, 0.3f, 0.3f, 1);
	
	V2_F32 track_dim = slider_dim;
	track_dim.width  *= 0.99f;
	track_dim.height *= 0.25f;
	
	V4_F32 track_color = vec4(0.6f, 0.6f, 0.6f, 1);
	
	V2_F32 handle_dim = vec2(0.75f * MIN(slider_dim.width, slider_dim.height));
	V2_F32 handle_pos = slider_pos;
	handle_pos.x -= 0.5f * (slider_dim.width - handle_dim.width);
	V4_F32 handle_color = vec4(0.5f, 0.5f, 0.5f, 1);
	
	V2_F32 track_final_dim = track_dim;
	V2_F32 handle_final_dim = handle_dim;
	
	if (interaction.hover)
	{
		track_final_dim.height = lerp(track_dim.height, 
			ui->active_t,
			1.25f * track_dim.height);
		
		handle_final_dim.height = lerp(handle_dim.height, 
			ui->active_t,
			1.15f * handle_dim.height);
	}
	
	if (interaction.pressed)
	{
		handle_final_dim.height = lerp(handle_final_dim.height, 
			ui->active_t,
			0.95f * handle_dim.height);
		
		handle_color = lerp(handle_color, ui->hot_t, vec4(0.7f, 0.7f, 0.7f, 1));
		
		if (slider_dim.width)
		{
			f32 click_percent = ((ui->mouse_p.x - slider_pos.x) + 0.5f * slider_dim.width) / slider_dim.width;
			click_percent = CLAMP(0.0f, click_percent, 1.0f);
			*value = lerp(min, click_percent, max);
		}
	}
	
	// NOTE(fakhri): draw slider background
	push_rect(ui->group, slider_pos, slider_dim, color, 5);
	
	// NOTE(fakhri): draw track
	push_rect(ui->group, slider_pos, track_final_dim, track_color, 5);
	handle_pos.x += map_into_range_zo(min, *value, max) * (slider_dim.width - handle_final_dim.width);
	
	if (progress)
	{
		V4_F32 progress_color = vec4(1, 1, 1, 1);
		// NOTE(fakhri): draw progress
		Range2_F32 track_rect = range_center_dim(slider_pos, track_final_dim);
		Range2_F32 progress_rect = range_cut_left(track_rect, 0.5f * track_final_dim.width + handle_pos.x - slider_pos.x).left;
		push_rect(ui->group, range_center(progress_rect), range_dim(progress_rect), progress_color, 5);
	}
	
	
	// NOTE(fakhri): draw handle
	push_rect(ui->group, handle_pos, handle_final_dim, handle_color, 0.5f * handle_final_dim.width);
	
	return interaction;
}


#define ui_button(ui, font, text, pos) _ui_button(ui, font, text, pos, __LINE__)
internal Mplayer_UI_Interaction
_ui_button(Mplayer_UI *ui, Mplayer_Font *font,  String8 text, V2_F32 pos, u64 id)
{
	V2_F32 padding    = vec2(10, 10);
	V2_F32 button_dim = font_compute_text_dim(font, text) + padding;
	
	Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, id, pos, button_dim);
	
	V2_F32 final_button_dim = button_dim;
	
	if (interaction.hover)
	{
		final_button_dim.height = lerp(final_button_dim.height, 
			ui->active_t,
			1.05f * button_dim.height);
	}
	
	if (interaction.pressed)
	{
		final_button_dim.height = lerp(final_button_dim.height, 
			ui->hot_t,
			0.95f * button_dim.height);
	}
	
	V4_F32 button_bg_color = vec4(0.3f, 0.3f, 0.3f, 1);
	V4_F32 text_color = vec4(1.0f, 1.0f, 1.0f, 1);
	push_rect(ui->group, pos, final_button_dim, button_bg_color);
	draw_text_centered(ui->group, font, pos, text_color, text);
	return interaction;
}


internal void
mplayer_get_audio_samples(Sound_Config device_config, Mplayer_Context *mplayer, void *output_buf, u32 frame_count)
{
	if (mplayer->play_track)
	{
		Mplayer_Music_Track *music = mplayer->current_music;
		if (music)
		{
			Flac_Stream *flac_stream = music->flac_stream;
			f32 volume = mplayer->volume;
			if (flac_stream)
			{
				u32 channels_count = flac_stream->streaminfo.nb_channels;
				
				// TODO(fakhri): convert to device channel layout
				assert(channels_count == device_config.channels_count);
				
				if (device_config.sample_rate != flac_stream->streaminfo.sample_rate)
				{
					
				}
				
				Memory_Checkpoint scratch = get_scratch(0, 0);
				Decoded_Samples streamed_samples = flac_read_samples(flac_stream, frame_count, device_config.sample_rate, scratch.arena);
				
				for (u32 i = 0; i < streamed_samples.frames_count; i += 1)
				{
					f32 *samples = streamed_samples.samples + i * channels_count;
					f32 *out_f32 = (f32*)output_buf + i * channels_count;
					for (u32 c = 0; c < channels_count; c += 1)
					{
						out_f32[c] = volume * samples[c];
					}
				}
			}
		}
	}
}

internal void
mplayer_load_library(Mplayer_Context *mplayer, String8 library_path)
{
	Memory_Checkpoint temp_mem = m_checkpoint_begin(&mplayer->frame_arena);
	File_Iterator_Handle *it = m_arena_push_struct(temp_mem.arena, File_Iterator_Handle);
	
	if (mplayer->os.file_iter_begin(it, library_path))
	{
		for (;;)
		{
			Memory_Checkpoint scratch = get_scratch(0, 0);
			File_Info info = mplayer->os.file_iter_next(it, scratch.arena);
			if (!has_flag(info.flags, FileFlag_Valid))
			{
				break;
			}
			
			if (has_flag(info.flags, FileFlag_Directory))
			{
				// NOTE(fakhri): directory
				Log("directory: %.*s", STR8_EXPAND(info.name));
				if (info.name.str[0] != '.')
				{
					String8 path = str8_f(scratch.arena, "%.*s/%.*s", STR8_EXPAND(library_path), STR8_EXPAND(info.name));
					mplayer_load_library(mplayer, path);
				}
			}
			else
			{
				if (str8_ends_with(info.name, str8_lit(".flac"), MatchFlag_CaseInsensitive))
				{
					Log("file: %.*s", STR8_EXPAND(info.name));
					Mplayer_Music_Track *music_track = mplayer_make_music_track(mplayer);
					mplayer_push_music_track(mplayer, music_track);
					music_track->path = str8_f(&music_track->arena, "%.*s/%.*s", STR8_EXPAND(library_path), STR8_EXPAND(info.name));
					music_track->name = push_str8_copy(&music_track->arena, info.name);
				}
			}
		}
		
		mplayer->os.file_iter_end(it);
	}
}

internal void
mplayer_initialize(Mplayer_Context *mplayer)
{
	load_font(mplayer, &mplayer->font, str8_lit("data/fonts/arial.ttf"), 30);
	load_font(mplayer, &mplayer->debug_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->timestamp_font, str8_lit("data/fonts/arial.ttf"), 20);
	mplayer->play_track = 0;
	mplayer->volume = 1.0f;
	
	mplayer_load_library(mplayer, mplayer->library_path);
}

struct Mplayer_Timestamp
{
	u8 hours;
	u8 minutes;
	u8 seconds;
};


internal Mplayer_Timestamp
flac_get_current_timestap(Flac_Stream *flac_stream)
{
	Mplayer_Timestamp result = ZERO_STRUCT;
	if (flac_stream)
	{
		u64 seconds_elapsed = flac_stream->next_sample_number / u64(flac_stream->streaminfo.sample_rate);
		u64 hours = seconds_elapsed / 3600;
		seconds_elapsed %= 3600;
		
		u64 minutes = seconds_elapsed / 60;
		seconds_elapsed %= 60;
		
		result.hours   = u8(hours);
		result.minutes = u8(minutes);
		result.seconds = u8(seconds_elapsed);
	}
	
	return result;
}

internal void
mplayer_update_and_render(Mplayer_Context *mplayer)
{
	Render_Context *render_ctx = mplayer->render_ctx;
	
	Range2_F32 screen_rect = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	Render_Group group = begin_render_group(render_ctx, vec2(0, 0), render_ctx->draw_dim, screen_rect);
	
	M4_Inv proj = group.config.proj;
	V2_F32 world_mouse_p = (proj.inv * vec4(mplayer->input.mouse_clip_pos)).xy;
	push_clear_color(render_ctx, vec4(0.1f, 0.1f, 0.1f, 1));
	
	// NOTE(fakhri): draw fps
	{
		V2_F32 fps_pos = 0.5 * render_ctx->draw_dim;
		fps_pos.x -= 20;
		fps_pos.y -= 15;
		draw_text_centered_f(&group, &mplayer->debug_font, fps_pos, vec4(0.5f, 0.6f, 0.6f, 1), "%d", u32(1.0f / mplayer->input.frame_dt));
	}
	
	if (mplayer->current_music && !is_music_track_still_playing(mplayer->current_music))
	{
		mplayer_play_music_track(mplayer, mplayer->current_music->next);
	}
	
	ui_begin(&mplayer->ui, &group, &mplayer->input, world_mouse_p);
	
	Range2_F32 available_space = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	// NOTE(fakhri): track control
	{
		Mplayer_Music_Track *current_music = mplayer->current_music;
		
		Range2_F32_Cut cut = range_cut_bottom(available_space, 125);
		available_space = cut.top;
		
		push_rect(&group, range_center(cut.bottom), range_dim(cut.bottom), vec4(0.05f, 0.05f, 0.05f, 1.0f));
		
		// NOTE(fakhri): padding
		cut = range_cut_top(cut.bottom, 10);
		
		// NOTE(fakhri): track
		{
			cut = range_cut_top(cut.bottom, 20);
			Range2_F32 track_rect = cut.top;
			
			// NOTE(fakhri): timestamp
			{
				Range2_F32_Cut cut2 = range_cut_left(track_rect, 100);
				track_rect = cut2.right;
				
				Range2_F32 timestamp_rect = range_cut_right(cut2.left, 100).right;
				Mplayer_Timestamp current_timestamp = ZERO_STRUCT;
				if (current_music) current_timestamp = flac_get_current_timestap(current_music->flac_stream);
				draw_text_centered_f(&group, &mplayer->timestamp_font, range_center(timestamp_rect), vec4(1, 1, 1, 1), "%.2d:%.2d:%.2d", current_timestamp.hours, current_timestamp.minutes, current_timestamp.seconds);
			}
			
			track_rect = range_cut_right(range_cut_left(track_rect, 20).right, 20).left;
			
			f32 samples_count = 0;
			f32 current_playing_sample = 0;
			if (current_music && current_music->flac_stream)
			{
				samples_count = (f32)current_music->flac_stream->streaminfo.samples_count;
				current_playing_sample = (f32)current_music->flac_stream->next_sample_number;
			}
			
			Mplayer_UI_Interaction track = ui_slider_f32(&mplayer->ui, &current_playing_sample, 0, samples_count, range_center(track_rect), range_dim(track_rect), 1);
			if (track.pressed)
			{
				if (current_music && current_music->flac_stream)
				{
					mplayer->play_track = false;
					flac_seek_stream(current_music->flac_stream, (u64)current_playing_sample);
				}
			}
			else if (track.released)
			{
				mplayer->play_track = true;
			}
		}
		
		if (!(current_music && current_music->flac_stream))
		{
			mplayer->play_track = false;
		}
		
		// NOTE(fakhri): horizontal padding
		cut = range_cut_left_percentage(cut.bottom, 1.0f/3.0f);
		
		// NOTE(fakhri): track name
		{
			if (mplayer->current_music)
			{
				// TODO(fakhri): stack of render configs?
				render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, cut.left);
				
				draw_text_centered(&group, &mplayer->font, range_center(cut.left), vec4(1, 1, 1, 1), mplayer->current_music->name);
				
				render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, screen_rect);
			}
		}
		
		// NOTE(fakhri): play button
		{
			cut = range_cut_left_percentage(cut.right, 0.5f);
			Range2_F32 button_rect = cut.left;
			
			if (!mplayer->play_track)
			{
				if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Play"), range_center(button_rect)).clicked)
				{
					mplayer->play_track = 1;
				}
			}
			else
			{
				if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Pause"), range_center(button_rect)).clicked)
				{
					mplayer->play_track = 0;
				}
			}
		}
		
		// NOTE(fakhri): volume
		{
			// NOTE(fakhri): vertical padding
			cut = range_cut_top(cut.right, 30);
			
			// NOTE(fakhri): horizontal padding
			cut = range_cut_right(cut.right, 20);
			
			// NOTE(fakhri): volume rect
			cut = range_cut_right(cut.left, 150);
			Range2_F32 volume_rect = range_cut_top(cut.right, 20).top;
			ui_slider_f32(&mplayer->ui, &mplayer->volume, 0, 1, range_center(volume_rect), range_dim(volume_rect), 1);
		}
	}
	
	// NOTE(fakhri): left side
	{
		Range2_F32_Cut cut = range_cut_left(available_space, 150);
		push_rect(&group, range_center(cut.left), range_dim(cut.left), vec4(0.06f, 0.06f, 0.06f, 1.0f));
		available_space = cut.right;
		
		// NOTE(fakhri): Library button  
		cut = range_cut_top(cut.left, 30);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Library"), range_center(cut.top)).clicked)
			{
				mplayer->mode = MODE_Library;
			}
		}
		
		// NOTE(fakhri):  button  
		cut = range_cut_top(cut.bottom, 30);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Lyrics"), range_center(cut.top)).clicked)
			{
				mplayer->mode = MODE_Lyrics;
			}
		}
	}
	
	switch (mplayer->mode)
	{
		case MODE_Library:
		{
			available_space = range_cut_top(available_space, 65).bottom;
			render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, available_space);
			
			V2_F32 available_space_dim = range_dim(available_space);
			V2_F32 music_option_dim = vec2(available_space_dim.width, 50);
			V2_F32 music_option_pos = range_center(available_space);
			music_option_pos.y += 0.5f * (available_space_dim.height - music_option_dim.height);
			
			for (Mplayer_Input_Event *event = mplayer->input.first_event; event; event = event->next)
			{
				if (event->consumed) continue;
				if (event->kind == Event_Kind_Mouse_Wheel)
				{
					event->consumed = true;
					
					mplayer->music_view_target_scroll -= 3.0f * event->scroll.y;
					mplayer->music_view_target_scroll = CLAMP(0, mplayer->music_view_target_scroll, mplayer->music_count - 1);
				}
			}
			
			Range2_F32 scrollbar_rect = range_cut_right(available_space, 50).right;
			
			// NOTE(fakhri): animate scroll
			{
				f32 dt = mplayer->input.frame_dt;
				f32 current_scroll = mplayer->music_view_scroll;
				f32 target_scroll  = mplayer->music_view_target_scroll;
				f32 scroll_speed  = mplayer->music_view_scroll_speed;
				
				f32 freq = 5.0f;
				f32 zeta = 1.f;
				
				f32 K1 = zeta / (PI32 * freq);
				f32 K2 = 1.0f / SQUARE(2 * PI32 * freq);
				
				f32 scroll_accel = (1.0f / K2) * (target_scroll - current_scroll - K1 * scroll_speed);
				mplayer->music_view_scroll_speed += dt * scroll_accel;
				mplayer->music_view_scroll += dt * mplayer->music_view_scroll_speed + 0.5f * SQUARE(dt) * scroll_accel;
			}
			
			music_option_pos.y += mplayer->music_view_scroll * music_option_dim.height;
			
			Mplayer_Music_Track *music = mplayer->first_music;
			// NOTE(fakhri): skip until the first visible option 
			for (music = mplayer->first_music; music; music = music->next)
			{
				Range2_F32 music_rect = range_center_dim(music_option_pos, music_option_dim);
				if (is_range_intersect(music_rect, available_space))
				{
					break;
				}
				music_option_pos.y -= music_option_dim.height;
			}
			
			for (; music; music = music->next)
			{
				Range2_F32 music_rect = range_center_dim(music_option_pos, music_option_dim);
				if (!is_range_intersect(music_rect, available_space))
				{
					break;
				}
				
				Range2_F32 visible_range = range_intersection(music_rect, available_space);
				Mplayer_UI_Interaction interaction = _ui_widget_interaction(&mplayer->ui, int_from_ptr(music), range_center(visible_range), range_dim(visible_range));
				
				V4_F32 bg_color = vec4(0.15f, 0.15f,0.15f, 1);
				if (music == mplayer->current_music)
				{
					bg_color = vec4(0.f, 0.f,0.f, 1);
				}
				
				if (interaction.hover)
				{
					bg_color = vec4(0.2f, 0.2f,0.2f, 1);
				}
				
				if (interaction.pressed)
				{
					bg_color = vec4(0.14f, 0.14f,0.14f, 1);
					
					mplayer_play_music_track(mplayer, music);
					mplayer->play_track = true;
				}
				
				// NOTE(fakhri): background
				push_rect(&group, music_option_pos, music_option_dim, bg_color);
				draw_outline(&group, music_option_pos, music_option_dim, 1, vec4(0, 0, 0, 1));
				
				Range2_F32_Cut cut = range_cut_left(music_rect, 20); // horizontal padding
				
				V2_F32 name_dim = font_compute_text_dim(&mplayer->font, music->name);
				cut = range_cut_left(cut.right, name_dim.width);
				
				Range2_F32 name_rect = cut.left;
				draw_text_centered(&group, &mplayer->font, range_center(name_rect), vec4(1, 1, 1, 1), music->name);
				
				music_option_pos.y -= music_option_dim.height;
			}
			
		} break;
		
		case MODE_Lyrics:
		{
			draw_text_centered(&group, &mplayer->font, range_center(available_space), vec4(1, 1, 1, 1), str8_lit("Lyrics not available"));
		} break;
	}
	
	if (mplayer->current_music && is_texture_valid(mplayer->current_music->cover_texture))
	{
		Texture cover_texture = mplayer->current_music->cover_texture;
		push_image(&group, vec3(0, 0, 0), vec2(f32(cover_texture.width), f32(cover_texture.height)), cover_texture);
	}
}
