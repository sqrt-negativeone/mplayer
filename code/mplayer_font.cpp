
#define STB_RECT_PACK_IMPLEMENTATION
#include "third_party/stb_rect_pack.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

struct Glyph_Metrics
{
	Texture handle;
	V2_F32 uv_scale;
	V2_F32 uv_offset;
	V2_F32 dim;
	V2_F32 offset;
	f32 advance;
};

struct Font_Metrics
{
	f32 ascent;
	f32 descent;
	f32 line_gap;
};

struct Font_Texture
{
	Texture handle;
	V2_I32 dim;
	Buffer buf;
};

struct Glyph_Entry
{
	Glyph_Entry *next;
	u32 codepoint;
	
	Glyph_Metrics glyph;
};

struct Font_Raster
{
	Font_Texture atlas;
	stbtt_pack_context ctx;
};

#define FACE_INDRECT_MAP_SIZE 128
struct Font_Face
{
	Font_Face *next;
	Font *font;
	
	f32 size;
	Font_Metrics metrics;
	Buffer content;
	
	Font_Raster raster;
	
	Glyph_Metrics utf8_class1_map[0x80];
	Glyph_Entry *utf8_indirect_map[FACE_INDRECT_MAP_SIZE];
};

struct Font
{
	// NOTE(fakhri): unscaled values
	i32 ascent;
	i32 descent;
	i32 line_gap;
	stbtt_fontinfo info;
	Buffer content;
	
	// TODO(fakhri): use hashtable?
	Font_Face *faces;
};


struct Fonts_Context
{
	Memory_Arena arena;
	Render_Context *render_ctx;
	Font_Face nil_face;
};

global Fonts_Context *g_fonts_ctx = 0;
#define NULL_FACE &g_fonts_ctx->nil_face


// NOTE(fakhri): hash yoinked from https://github.com/skeeto/hash-prospector
internal u32 
hash32(u32 x)
{
	x = (x ^ (x >> 16)) * 0x7feb352d;
	x = (x ^ (x >> 15)) * 0x846ca68b;
	x = (x ^ (x >> 16));
	return x;
}

internal b32
fnt_is_null(Font_Face *face)
{
	b32 result = face == NULL_FACE;
	return result;
}

internal void
fnt_set_fonts_context(Fonts_Context *fonts_ctx)
{
	g_fonts_ctx = fonts_ctx;
}

internal Fonts_Context *
fnt_init(Render_Context *render_ctx)
{
	Fonts_Context *fonts_ctx = m_arena_bootstrap_push(Fonts_Context, arena);
	fnt_set_fonts_context(fonts_ctx);
	fonts_ctx->render_ctx = render_ctx;
	return fonts_ctx;
}


internal Font *
fnt_open_font_from_memory(Buffer font_content)
{
	Font *font = 0;
	if (is_valid(font_content))
	{
		font = m_arena_push_struct(&g_fonts_ctx->arena, Font);
		font->content = font_content;
		stbtt_InitFont(&font->info, (const unsigned char *)font->content.data, 0);
		stbtt_GetFontVMetrics(&font->info, &font->ascent, &font->descent, &font->line_gap);
		
		font->faces = NULL_FACE;
	}
	else
	{
		invalid_code_path();
	}
	return font;
}

internal Font *
fnt_open_font(String8 path)
{
	Buffer font_content = platform->load_entire_file(path, &g_fonts_ctx->arena);
	Font *font = fnt_open_font_from_memory(font_content);
	return font;
}

internal Glyph_Entry *
fnt_indirect_map_find_glyph(Font_Face *face, u32 codepoint)
{
	Glyph_Entry *glyph_entry = 0;
	u32 hash = hash32(codepoint);
	u32 slot_index = hash % FACE_INDRECT_MAP_SIZE;
	
	for (Glyph_Entry *g = face->utf8_indirect_map[slot_index];
		g;
		g = g->next)
	{
		if (g->codepoint == codepoint)
		{
			glyph_entry = g;
			break;
		}
	}
	
	return glyph_entry;
}

internal Glyph_Metrics *
fnt_get_or_create_glyph(Font_Face *face, u32 codepoint)
{
	Glyph_Metrics *glyph = 0;
	
	if (codepoint <= 0x7F)
	{
		glyph = face->utf8_class1_map + codepoint;
	}
	else
	{
		Glyph_Entry *glyph_entry = fnt_indirect_map_find_glyph(face, codepoint);
		if (!glyph_entry)
		{
			u32 hash = hash32(codepoint);
			u32 slot_index = hash % FACE_INDRECT_MAP_SIZE;
			
			glyph_entry = m_arena_push_struct_z(&g_fonts_ctx->arena, Glyph_Entry);
			
			glyph_entry->next = face->utf8_indirect_map[slot_index];
			glyph_entry->codepoint = codepoint;
			
			face->utf8_indirect_map[slot_index] = glyph_entry;
		}
		
		assert(glyph_entry);
		glyph = &glyph_entry->glyph;
	}
	
	return glyph;
}

internal void
fnt_raster_init(Font_Raster *raster)
{
	V2_I32 atlas_dim = vec2i(1024, 1024);
	raster->atlas.dim = atlas_dim;
	if (!raster->atlas.buf.size)
	{
		raster->atlas.buf = arena_push_buffer(&g_fonts_ctx->arena, atlas_dim.width * atlas_dim.height);
	}
	assert((i64)raster->atlas.buf.size == atlas_dim.width * atlas_dim.height);
	raster->atlas.handle = reserve_texture_handle(g_fonts_ctx->render_ctx, (u16)atlas_dim.width, (u16)atlas_dim.height, TEXTURE_FLAG_GRAY_BIT);
	
	stbtt_PackBegin(&raster->ctx, raster->atlas.buf.data, raster->atlas.dim.width, raster->atlas.dim.height, 0, 1, 0);
	stbtt_PackSetOversampling(&raster->ctx, 1, 1);
}

internal void
fnt_raster_range(stbtt_fontinfo *info, Font_Face *face, Font_Raster *raster, stbtt_pack_range *rng)
{
	Memory_Checkpoint_Scoped scratch(begin_scratch(0, 0));
	
	stbrp_rect *rects = m_arena_push_array_z(scratch.arena, stbrp_rect, rng->num_chars);
	
	int n = stbtt_PackFontRangesGatherRects(&raster->ctx, info, rng, 1, rects);
	stbtt_PackFontRangesPackRects(&raster->ctx, rects, n);
	if (!stbtt_PackFontRangesRenderIntoRects(&raster->ctx, info, rng, 1, rects))
	{
		// TODO(fakhri): failed to pack all glyphs! probably the texture is full, if that's the case
		//               reserve a new texture and try again with the glyphs that failed
		#if 0
			stbtt_PackEnd(&raster->ctx);
		fnt_raster_init(raster);
		#endif
			
			not_impemeneted();
	}
	
	for (i32 ch_index = 0; 
		ch_index < rng->num_chars;
		ch_index += 1)
	{
		u32 codepoint = rng->first_unicode_codepoint_in_range + ch_index;
		Glyph_Metrics *glyph = fnt_get_or_create_glyph(face, codepoint);
		
		f32 x_offset = 0;
		f32 y_offset = 0;
		
		stbtt_aligned_quad quad;
		stbtt_GetPackedQuad(rng->chardata_for_range, raster->atlas.dim.x, raster->atlas.dim.y, ch_index, &x_offset, &y_offset, &quad, true);
		
		glyph->handle    = raster->atlas.handle;
		glyph->uv_offset = vec2(quad.s0, quad.t0);
		glyph->uv_scale  = vec2(ABS(quad.s1 - quad.s0), ABS(quad.t1 - quad.t0));
		glyph->dim       = glyph->uv_scale * vec2(face->raster.atlas.dim);
		glyph->offset    = vec2(quad.x0 + 0.5f * glyph->dim.width, -(quad.y0 + 0.5f * glyph->dim.height));
		glyph->advance   = x_offset;
	}
	
	push_texture_upload_request(&g_fonts_ctx->render_ctx->upload_buffer, &raster->atlas.handle, raster->atlas.buf, 0);
}


internal Font_Face *
fnt_get_face(Font *font, f32 size)
{
	Font_Face *face = NULL_FACE;
	for (Font_Face *f = font->faces; !fnt_is_null(f); f = f->next)
	{
		if (f->size == size)
		{
			face = f;
			break;
		}
	}
	
	if (fnt_is_null(face))
	{
		// NOTE(fakhri): create new face
		face = m_arena_push_struct_z(&g_fonts_ctx->arena, Font_Face);
		face->next = font->faces;
		font->faces = face;
		face->font = font;
		
		f32 scale = stbtt_ScaleForPixelHeight(&font->info, size);
		face->size = size;
		face->metrics.ascent   = scale * (f32)font->ascent;
		face->metrics.descent  = scale * (f32)font->descent;
		face->metrics.line_gap = scale * (f32)font->line_gap;
		
		//- NOTE(fakhri): build utf8 class 1 direct map
		{
			Memory_Checkpoint_Scoped scratch(begin_scratch(0, 0));
			fnt_raster_init(&face->raster);
			stbtt_packedchar *chardata_for_range = m_arena_push_array_z(scratch.arena, stbtt_packedchar, array_count(face->utf8_class1_map));
			stbtt_pack_range rng = {
				.font_size = size,
				.first_unicode_codepoint_in_range = 0,
				.array_of_unicode_codepoints = 0,
				.num_chars = 256,
				.chardata_for_range = chardata_for_range,
			};
			
			fnt_raster_range(&font->info, face, &face->raster, &rng);
		}
	}
	
	return face;
}

internal Glyph_Metrics *
fnt_get_glyph(Font *font, f32 size, u32 codepoint)
{
	Font_Face *face = fnt_get_face(font, size);
	Glyph_Metrics *glyph = 0;
	
	if (codepoint <= 0x7F)
	{
		// NOTE(fakhri): direct map class1 utf8
		glyph = &face->utf8_class1_map[codepoint];
	}
	else
	{
		u32 hash = hash32(codepoint);
		Glyph_Entry *glyph_entry = fnt_indirect_map_find_glyph(face, codepoint);
		if (!glyph_entry)
		{
			stbtt_packedchar packed_char;
			
			stbtt_pack_range rng = {
				.font_size = face->size,
				.first_unicode_codepoint_in_range = (int)codepoint,
				.array_of_unicode_codepoints = 0,
				.num_chars = 1,
				.chardata_for_range = &packed_char,
			};
			
			fnt_raster_range(&face->font->info, face, &face->raster, &rng);
			glyph_entry = fnt_indirect_map_find_glyph(face, codepoint);
		}
		
		assert(glyph_entry);
		glyph = &glyph_entry->glyph;
	}
	
	return glyph;
}

internal V2_F32
fnt_compute_text_dim(Font *font, f32 size, String8 text)
{
	Font_Face *face = fnt_get_face(font, size);
	
	V2_F32 result;
	result.height = face->metrics.ascent - face->metrics.descent;
	result.width = 0;
	for (u32 offset = 0, advance = 0;
		offset < text.len;
		offset += advance)
	{
		Decoded_Codepoint utf8 = decode_codepoint_from_utf8(text.str + offset, text.len - offset);
		advance = utf8.advance;
		u32 codepoint = utf8.codepoint;
		Glyph_Metrics *glyph = fnt_get_glyph(font, size, codepoint);
		
		result.width += glyph->advance;
		
	}
	return result;
}


enum Text_Render_Flags
{
	Text_Render_Flag_Centered    = (1 << 0),
	Text_Render_Flag_Limit_Width = (1 << 1),
	Text_Render_Flag_Underline   = (1 << 2),
};

internal V3_F32
fnt_draw_text(Render_Group *group, Font *font, f32 size, String8 text, V3_F32 pos, V4_F32 color, u32 flags)
{
	f32 current_width = 0;
	b32 width_overflow = 0;
	
	if (has_flag(flags, Text_Render_Flag_Centered))
	{
		V2_F32 text_dim = fnt_compute_text_dim(font, size, text);
		pos.x -= 0.5f * text_dim.width;
		pos.y -= 0.25f * text_dim.height;
	}
	
	// NOTE(fakhri): render the text
	V3_F32 text_pt = pos;
	for (String8_UTF8_Iterator it = str8_utf8_iterator(text);
		str8_utf8_it_valid(&it);
		str8_utf8_advance(&it))
	{
		Glyph_Metrics *glyph = fnt_get_glyph(font, size, it.utf8.codepoint);
		
		V3_F32 glyph_p = vec3(text_pt.xy + (glyph->offset), text_pt.z);
		push_image(group, glyph_p, 
			glyph->dim,
			glyph->handle,
			color,
			0.0f,
			glyph->uv_scale, 
			glyph->uv_offset);
		text_pt.x += glyph->advance;
		
		// TODO(fakhri): kerning
	}
	
	return text_pt;
}