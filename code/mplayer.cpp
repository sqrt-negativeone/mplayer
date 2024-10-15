
#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#include "mplayer_input.cpp"

struct MPlayer_Glyph
{
	V2_F32 uv_scale;
	V2_F32 uv_offset;
	V2_F32 size;
	V2_F32 offset;
	f32 advance;
};

struct MPlayer_Font
{
	b32 loaded;
	Texture atlas_tex;
	Buffer pixels_buf;
	MPlayer_Glyph *glyphs;
	u32 first_glyph_index;
	u32 opl_glyph_index;
	f32 line_advance;
	f32 ascent;
	f32 descent;
};


struct MPlayer_Context
{
	Memory_Arena *main_arena;
	Memory_Arena *frame_arena;
	Render_Context *render_ctx;
	MPlayer_Input input;
	
	Flac_Stream flac_stream;
	Buffer flac_file_buffer;
	
	Load_Entire_File *load_entire_file;
	
	MPlayer_Font font;
};

internal void
load_font(MPlayer_Context *mplayer, MPlayer_Font *font, String8 font_path, f32 font_size)
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
  
	// NOTE(fakhri): build direct map
	MPlayer_Glyph *glyphs = m_arena_push_array(mplayer->main_arena, MPlayer_Glyph, opl_glyph_index - first_glyph_index);
	assert(glyphs);
  
	for (u32 ch = first_glyph_index; 
		ch < opl_glyph_index;
		ch += 1)
	{
		u32 index = ch - first_glyph_index;
		MPlayer_Glyph *glyph = glyphs + index;;
		
		f32 x_offset = 0;
		f32 y_offset = 0;
		
		stbtt_aligned_quad quad;
		stbtt_GetPackedQuad(rng.chardata_for_range, atlas_dim.x, atlas_dim.y, (i32)index, &x_offset, &y_offset, &quad, true);
		
		glyph->uv_offset = vec2(quad.s0, quad.t0);
		glyph->uv_scale  = vec2(ABS(quad.s1 - quad.s0), ABS(quad.t1 - quad.t0));
		glyph->size      = glyph->uv_scale * vec2(atlas_dim);
		glyph->offset    = vec2(quad.x0 + 0.5f * glyph->size.x, -(quad.y0 + 0.5f * glyph->size.y));
		glyph->advance   = x_offset;
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

internal MPlayer_Glyph
font_get_glyph_from_char(MPlayer_Font *font, u8 ch)
{
	MPlayer_Glyph glyph = ZERO_STRUCT;
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

internal void
draw_text(Render_Context *render_ctx, MPlayer_Font *font, String8 text, V2_F32 pos, V4_F32 color, M4_Inv clip, f32 scale = 1.0f)
{
	// NOTE(fakhri): render the text
	V3_F32 text_pt = vec3(pos, 0);
	for (u32 offset = 0;
		offset < text.len;
		offset += 1)
	{
		u8 ch = text.str[offset];
		// TODO(fakhri): utf8 support
		MPlayer_Glyph glyph = font_get_glyph_from_char(font, ch);
		V3_F32 glyph_p = vec3(text_pt.xy + (glyph.offset * scale), 0);
		push_image(render_ctx, 
			glyph_p, 
			glyph.size * scale,
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
	mplayer->flac_file_buffer = mplayer->load_entire_file(str8_lit("data/tests/fear_inoculum.flac"), mplayer->main_arena);
	init_flac_stream(&mplayer->flac_stream, mplayer->flac_file_buffer);
	load_font(mplayer, &mplayer->font, str8_lit("data/fonts/arial.ttf"), 20);
}

internal void
mplayer_update_and_render(MPlayer_Context *mplayer)
{
	Render_Context *render_ctx = mplayer->render_ctx;
	
	M4_Inv clip = compute_clip_matrix(vec2(0, 0), render_ctx->draw_dim);
	
	V2_F32 world_mouse_p = (clip.inv * vec4(mplayer->input.mouse_clip_pos)).xy;
	push_clear_color(render_ctx, vec4(0.1f, 0.1f, 0.1f, 1));
	
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
	
	V4_F32 cursor_color = vec4(1.0f, 1.0f, 0.0f, 1);
	if (mplayer->input.buttons[Key_LeftMouse].is_down)
	{
		cursor_color.g = 0;
	}
	push_rect(render_ctx, world_mouse_p, vec2(10, 10), clip, cursor_color);
}
