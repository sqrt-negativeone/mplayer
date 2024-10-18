
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
	b32 pressed;
	b32 clicked; // pressed widget and released inside the widget
};

struct Mplayer_UI
{
	Render_Context *render_ctx;
	Mplayer_Input *input;
	V2_F32 mouse_p;
	
	u32 active_widget_id;
	u32 hot_widget_id;
	
	f32 active_t;
	f32 active_dt;
	
	f32 hot_t;
	f32 hot_dt;
};


struct Mplayer_Context
{
	Memory_Arena *main_arena;
	Memory_Arena *frame_arena;
	Render_Context *render_ctx;
	Mplayer_UI ui;
	Mplayer_Input input;
	
	b32 play_track;
	Flac_Stream flac_stream;
	Buffer flac_file_buffer;
	
	Load_Entire_File *load_entire_file;
	
	Mplayer_Font font;
	f32 volume;
	f32 seek_percentage;
	
	b32 seek_requested;
	u64 seek_target_sample;
};

internal void
load_font(Mplayer_Context *mplayer, Mplayer_Font *font, String8 font_path, f32 font_size)
{
	if (font->loaded)
	{
		return;
	}
	
	V2_I32 atlas_dim = vec2i(1024, 1024);
  
	Memory_Checkpoint scratch = begin_scratch(0, 0);
  
	Buffer font_content = mplayer->load_entire_file(font_path, scratch.arena);
	assert(font_content.data);
  
	Buffer pixels_buf = arena_push_buffer(mplayer->main_arena, atlas_dim.width * atlas_dim.height);
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
	Mplayer_Glyph *glyphs = m_arena_push_array(mplayer->main_arena, Mplayer_Glyph, opl_glyph_index - first_glyph_index);
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
		invalid_code_path();
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
draw_text(Render_Context *render_ctx, Mplayer_Font *font, String8 text, V2_F32 pos, V4_F32 color, M4_Inv clip, f32 scale = 1.0f)
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
		V3_F32 glyph_p = vec3(text_pt.xy + (glyph.offset * scale), 0);
		push_image(render_ctx, 
			glyph_p, 
			glyph.dim * scale,
			font->atlas_tex,
			clip,
			color,
			0.0f,
			glyph.uv_scale, 
			glyph.uv_offset);
		
		text_pt.x += scale * glyph.advance;
	}
}

internal void
draw_text_centered(Render_Context *render_ctx, Mplayer_Font *font, String8 text, V2_F32 pos, V4_F32 color, M4_Inv clip, f32 scale = 1.0f)
{
	V2_F32 text_dim = scale * font_compute_text_dim(font, text);
	pos.x -= 0.5f * text_dim.width;
	pos.y -= 0.25f * text_dim.height;
	draw_text(render_ctx, font, text, pos, color, clip, scale);
}

internal void
draw_circle(Render_Context *render_ctx, V2_F32 pos, f32 radius, V4_F32 color, M4_Inv clip)
{
	push_rect(render_ctx, pos, vec2(2 * radius), clip, color, radius);
}

internal void
ui_begin(Mplayer_UI *ui, Render_Context *render_ctx, Mplayer_Input *input, V2_F32 mouse_p)
{
	ui->render_ctx = render_ctx;
	ui->input   = input;
	ui->mouse_p = mouse_p;
}


#define ui_slider_f32(ui, value, min, max, slider_pos, slider_dim, clip) _ui_slider_f32(ui, value, min, max, slider_pos, slider_dim, clip, __LINE__)
internal Mplayer_UI_Interaction
_ui_slider_f32(Mplayer_UI *ui, f32 *value, f32 min, f32 max, V2_F32 slider_pos, V2_F32 slider_dim, M4_Inv clip, u32 id)
{
	Mplayer_UI_Interaction interaction = ZERO_STRUCT;
	V4_F32 color = vec4(0.3f, 0.3f, 0.3f, 1);
	
	V2_F32 track_dim = slider_dim;
	track_dim.width  *= 0.99f;
	track_dim.height *= 0.25f;
	
	V4_F32 track_color = vec4(0.6f, 0.6f, 0.6f, 1);
	
	V2_F32 handle_dim = vec2(0.75f * MIN(slider_dim.width, slider_dim.height));
	V2_F32 handle_pos = slider_pos;
	handle_pos.x -= 0.5f * (slider_dim.width - handle_dim.width);
	V4_F32 handle_color = vec4(0.5f, 0.5f, 0.5f, 1);
	
	if (is_in_range(range_center_dim(slider_pos, slider_dim), ui->mouse_p))
	{
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
		
		if (ui->hot_widget_id == id && 
			mplayer_button_released(ui->input, Key_LeftMouse))
		{
			ui->hot_widget_id = 0;
		}
	}
	
	V2_F32 track_final_dim = track_dim;
	V2_F32 handle_final_dim = handle_dim;
	
	if (ui->active_widget_id == id)
	{
		ui->active_t += ui->input->frame_dt * ui->active_dt;
		ui->active_t = CLAMP(0, ui->active_t, 1.0f);
		
		track_final_dim.height = lerp(track_dim.height, 
			ui->active_t,
			1.25f * track_dim.height);
		
		handle_final_dim.height = lerp(handle_dim.height, 
			ui->active_t,
			1.15f * handle_dim.height);
		
		
		if (mplayer_button_clicked(ui->input, Key_LeftMouse))
		{
			ui->hot_widget_id = id;
			ui->hot_t = 0;
			ui->hot_dt = 20.0f;
		}
		
		if (ui->hot_widget_id == id)
		{
			if (mplayer_button_released(ui->input, Key_LeftMouse))
			{
				interaction.clicked = 1;
				ui->hot_widget_id = 0;
			}
		}
	}
	
	if (ui->hot_widget_id == id)
	{
		interaction.pressed = true;
		ui->hot_t += ui->input->frame_dt * ui->hot_dt;
		ui->hot_t = CLAMP(0, ui->hot_t, 1.0f);
		
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
	push_rect(ui->render_ctx, slider_pos, slider_dim, clip, color, 5);
	
	// NOTE(fakhri): draw track
	push_rect(ui->render_ctx, slider_pos, track_final_dim, clip, track_color, 5);
	
	// NOTE(fakhri): draw handle
	handle_pos.x += map_into_range_zo(min, *value, max) * (slider_dim.width - handle_final_dim.width);
	push_rect(ui->render_ctx, handle_pos, handle_final_dim, clip, handle_color, 0.5f * handle_final_dim.width);
	
	return interaction;
}


#define ui_button(ui, font, clip, text, pos) _ui_button(ui, font, clip, text, pos, __LINE__)
internal Mplayer_UI_Interaction
_ui_button(Mplayer_UI *ui, Mplayer_Font *font, M4_Inv clip,  String8 text, V2_F32 pos, u32 id)
{
	Mplayer_UI_Interaction interaction = ZERO_STRUCT;
	
	V2_F32 padding    = vec2(10, 10);
	V2_F32 button_dim = font_compute_text_dim(font, text) + padding;
	
	if (is_in_range(range_center_dim(pos, button_dim), ui->mouse_p))
	{
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
		
		if (ui->hot_widget_id == id && 
			mplayer_button_released(ui->input, Key_LeftMouse))
		{
			ui->hot_widget_id = 0;
		}
	}
	
	V2_F32 final_button_dim = button_dim;
	
	if (ui->active_widget_id == id)
	{
		ui->active_t += ui->input->frame_dt * ui->active_dt;
		ui->active_t = CLAMP(0, ui->active_t, 1.0f);
		
		
		final_button_dim.height = lerp(final_button_dim.height, 
			ui->active_t,
			1.05f * button_dim.height);
		
		if (mplayer_button_clicked(ui->input, Key_LeftMouse))
		{
			ui->hot_widget_id = id;
			ui->hot_t = 0;
			ui->hot_dt = 20.0f;
		}
		
		if (ui->hot_widget_id == id)
		{
			if (mplayer_button_released(ui->input, Key_LeftMouse))
			{
				interaction.clicked = 1;
				ui->hot_widget_id = 0;
			}
		}
	}
	
	if (ui->hot_widget_id == id)
	{
		ui->hot_t += ui->input->frame_dt * ui->hot_dt;
		ui->hot_t = CLAMP(0, ui->hot_t, 1.0f);
		
		
		final_button_dim.height = lerp(final_button_dim.height, 
			ui->hot_t,
			0.95f * button_dim.height);
	}
	
	V4_F32 button_bg_color = vec4(0.3f, 0.3f, 0.3f, 1);
	V4_F32 text_color = vec4(1.0f, 1.0f, 1.0f, 1);
	push_rect(ui->render_ctx, pos, final_button_dim, clip, button_bg_color);
	draw_text_centered(ui->render_ctx, font, text, pos, text_color, clip);
	return interaction;
}


internal void
mplayer_get_audio_samples(Mplayer_Context *mplayer, void *output_buf, u32 frame_count)
{
	if (mplayer->seek_requested)
	{
		flac_seek_stream(&mplayer->flac_stream, mplayer->seek_target_sample);
		mplayer->seek_requested = false;
	}
	
	if (mplayer->play_track)
	{
		Flac_Stream *flac_stream = &mplayer->flac_stream;
		f32 volume = mplayer->volume;
		if (flac_stream)
		{
			u32 channels_count = flac_stream->streaminfo.nb_channels;
			
			Memory_Checkpoint scratch = get_scratch(0, 0);
			Decoded_Samples streamed_samples = flac_read_samples(flac_stream, frame_count, scratch.arena);
			
			for (u32 i = 0; i < streamed_samples.frames_count; i += 1)
			{
				f32 *samples = streamed_samples.samples + i * channels_count;
				f32 *out_f32 = (f32*)output_buf + i * channels_count;
				for (u32 c = 0; c < channels_count; c += 1)
				{
					out_f32[c] = volume * samples[c];
				}
			}
			// /memory_copy(output_buf, streamed_samples.samples, streamed_samples.frames_count * channels_count * sizeof(f32));
		}
	}
}

internal void
mplayer_initialize(Mplayer_Context *mplayer)
{
	mplayer->flac_file_buffer = mplayer->load_entire_file(str8_lit("data/tests/fear_inoculum.flac"), mplayer->main_arena);
	init_flac_stream(&mplayer->flac_stream, mplayer->flac_file_buffer);
	load_font(mplayer, &mplayer->font, str8_lit("data/fonts/arial.ttf"), 40);
	mplayer->play_track = 0;
	mplayer->volume = 1.0f;
}

internal void
mplayer_update_and_render(Mplayer_Context *mplayer)
{
	Render_Context *render_ctx = mplayer->render_ctx;
	
	M4_Inv clip = compute_clip_matrix(vec2(0, 0), render_ctx->draw_dim);
	
	V2_F32 world_mouse_p = (clip.inv * vec4(mplayer->input.mouse_clip_pos)).xy;
	push_clear_color(render_ctx, vec4(0.1f, 0.1f, 0.1f, 1));
	
	#if 0	
		Range2_F32 screen_rect = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	{
		f32 footer_percentage = 0.15f;
		f32 footer_height = MAX(footer_percentage * range_dim(screen_rect).height, 100);
		Range2_F32 footer = rect_cut_bottom(screen_rect, footer_height);
		V2_F32 footer_pos = range_center(footer);
		V2_F32 footer_dim = range_dim(footer);
		
		push_rect(render_ctx, footer_pos, footer_dim, clip, vec4(0.2f, 0.2f, 0.2f, 1));
		
		V2_F32 play_button_dim = 0.9f * vec2(footer_dim.height);
		push_rect(render_ctx, footer_pos, play_button_dim, clip, vec4(0.3f, 0.3f, 0.3f, 1));
	}
	#endif
		
		// NOTE(fakhri): UI
	{
		f32 y = -200.0;
		ui_begin(&mplayer->ui, render_ctx, &mplayer->input, world_mouse_p);
		
		f32 samples_count = (f32)mplayer->flac_stream.streaminfo.samples_count;
		f32 current_playing_sample = (f32)mplayer->flac_stream.next_sample_number;
		
		if (ui_slider_f32(&mplayer->ui, &current_playing_sample, 0, samples_count, vec2(0, y), vec2(1200, 20), clip).pressed)
		{
			mplayer->seek_target_sample = (u64)current_playing_sample;
			mplayer->seek_requested = true;
		}
		
		
		y -= 50.f;
		
		if (!mplayer->play_track)
		{
			if (ui_button(&mplayer->ui, &mplayer->font, clip, str8_lit("Play"), vec2(0, y)).clicked)
			{
				mplayer->play_track = 1;
			}
		}
		else
		{
			if (ui_button(&mplayer->ui, &mplayer->font, clip, str8_lit("Pause"), vec2(0, y)).clicked)
			{
				mplayer->play_track = 0;
			}
		}
		
		ui_slider_f32(&mplayer->ui, &mplayer->volume, 0, 1, vec2(400, y), vec2(200, 20), clip);
	}
}
