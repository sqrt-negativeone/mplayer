
#include "mplayer.h"

#define STBI_SSE2
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"


#include "mplayer_renderer.cpp"
#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

#include "mplayer_byte_stream.h"

internal void
mplayer_reset_item_header(Mplayer_Item_Header *header)
{
	m_arena_free_all(&header->transient_arena);
	
	// TODO(fakhri): this is just a hack, if the image texture in the header
	// is valid then we are leaking GPU memeory by not freeing or reusing that memory
	header->image = ZERO_STRUCT;
}

internal Mplayer_Track *
mplayer_track_by_id(Mplayer_Library *library, Mplayer_Item_ID id)
{
	assert(id < library->tracks_count);
	Mplayer_Track *track = library->tracks + id;
	return track;
}

internal Mplayer_Track *
mplayer_make_track(Mplayer_Library *library)
{
	assert(library->tracks_count < MAX_TRACKS_COUNT);
	Mplayer_Item_ID id = library->tracks_count++;
	
	Mplayer_Track *track = mplayer_track_by_id(library, id);
	track->header.id = id;
	mplayer_reset_item_header(&track->header);
	return track;
}

internal Mplayer_Album *
mplayer_album_by_id(Mplayer_Library *library, Mplayer_Item_ID id)
{
	assert(id < library->albums_count);
	Mplayer_Album *album = library->albums + id;
	return album;
}

internal Mplayer_Album *
mplayer_make_album(Mplayer_Library *library)
{
	assert(library->albums_count < MAX_ALBUMS_COUNT);
	Mplayer_Item_ID id = library->albums_count++;
	
	Mplayer_Album *album = mplayer_album_by_id(library, id);
	album->header.id = id;
	mplayer_reset_item_header(&album->header);
	return album;
}

internal Mplayer_Artist *
mplayer_artist_by_id(Mplayer_Library *library, Mplayer_Item_ID id)
{
	assert(id < library->artists_count);
	Mplayer_Artist *artist = library->artists + id;
	return artist;
}

internal Mplayer_Artist *
mplayer_make_artist(Mplayer_Library *library)
{
	assert(library->artists_count < MAX_ARTISTS_COUNT);
	Mplayer_Item_ID id = library->artists_count++;
	
	Mplayer_Artist *artist = mplayer_artist_by_id(library, id);
	artist->header.id = id;
	
	mplayer_reset_item_header(&artist->header);
	
	return artist;
}


internal u64
str8_hash(String8 string)
{
	u64 result = 0;
	for (u32 i = 0; i < string.len; i += 1)
	{
		result = ((result << 5) + result) + string.str[i];
	}
	return result;
}

internal u64
str8_hash_case_insensitive(String8 string)
{
	u64 result = 0;
	for (u32 i = 0; i < string.len; i += 1)
	{
		u8 c = string.str[i];
		if (c >= 'A' && c < 'Z') c = c - 'A' + 'a';
		result = ((result << 5) + result) + c;
	}
	return result;
}

internal Mplayer_Album *
mplayer_find_or_create_album(Mplayer_Context *mplayer, Mplayer_Artist *artist, String8 album_name)
{
	u64 hash = str8_hash_case_insensitive(album_name);
	Mplayer_Album *result = 0;
	
	Mplayer_Library *library = &mplayer->library;
	
	for (u32 album_index = 0; album_index < library->albums_count; album_index += 1)
	{
		Mplayer_Album *album = library->albums + album_index;
		if (album->header.hash == hash && 
			str8_match(album_name, album->name, MatchFlag_CaseInsensitive) &&
			album->artist_id == artist->header.id)
		{
			result = album;
			break;
		}
	}
	
	if (!result)
	{
		result = mplayer_make_album(library);
		
		result->name = str8_clone(&library->arena, album_name);
		result->artist = artist->name;
		result->header.hash = hash;
		result->artist_id = artist->header.id;
		
		artist->albums.count += 1;
	}
	
	return result;
}

internal Mplayer_Artist *
mplayer_find_or_create_artist(Mplayer_Context *mplayer, String8 artist_name)
{
	u64 hash = str8_hash_case_insensitive(artist_name);
	Mplayer_Artist *result = 0;
	Mplayer_Library *library = &mplayer->library;
	
	for (u32 artist_index = 0; artist_index < library->artists_count; artist_index += 1)
	{
		Mplayer_Artist *artist = library->artists + artist_index;
		if (artist->header.hash == hash && 
			str8_match(artist_name, artist->name, MatchFlag_CaseInsensitive))
		{
			result = artist;
			break;
		}
	}
	
	if (!result)
	{
		result = mplayer_make_artist(library);
		
		result->name = str8_clone(&library->arena, artist_name);
		result->header.hash = hash;
	}
	return result;
}

internal void
mplayer_music_reset(Mplayer_Track *music_track)
{
	if (music_track && music_track->flac_stream)
	{
		flac_seek_stream(music_track->flac_stream, 0);
	}
}

internal b32
mplayer_is_valid_image(Mplayer_Item_Image image)
{
	b32 result = (image.in_disk && image.path.len) || is_valid(image.texture_data);
	return result;
}

internal b32
mplayer_item_image_ready(Mplayer_Item_Image *image)
{
	b32 result = image->texture.state == Texture_State_Loaded;
	return result;
}

internal void
mplayer_load_item_image(Mplayer_Context *mplayer, Memory_Arena *arena, Mplayer_Item_Image *image)
{
	if (!is_texture_valid(image->texture))
	{
		image->texture = reserve_texture_handle(mplayer->render_ctx);
	}
	
	if (image->texture.state == Texture_State_Unloaded)
	{
		image->texture.state = Texture_State_Loading;
		if (!is_valid(image->texture_data) && image->in_disk)
		{
			image->texture_data = platform->load_entire_file(image->path, arena);
		}
		
		if (is_valid(image->texture_data))
		{
			platform->push_work(mplayer->render_ctx->upload_texture_to_gpu_work, 
				make_texure_upload_data(mplayer->render_ctx, &image->texture, image->texture_data));
		}
	}
	
}

internal void
mpalyer_draw_item_image(Render_Group *group, Mplayer_Context *mplayer, Mplayer_Item_Header *item, V2_F32 pos, V2_F32 dim, V4_F32 solid_color = vec4(1,1,1,1), V4_F32 image_color = vec4(1,1,1,1), f32 roundness = 0.0f)
{
	mplayer_load_item_image(mplayer, &item->transient_arena, &item->image);
	if (mplayer_item_image_ready(&item->image))
	{
		push_image(group, pos, dim, item->image.texture, image_color, roundness);
	}
	else
	{
		push_rect(group, pos, dim, solid_color, roundness);
	}
}

internal void
mplayer_load_music_track(Mplayer_Context *mplayer, Mplayer_Track *music_track)
{
	if (!music_track->file_loaded)
	{
		music_track->flac_file_buffer = mplayer->os.load_entire_file(music_track->path, &music_track->header.transient_arena);
		music_track->file_loaded = true;
	}
	
	music_track->header.image.in_disk = false;
	if (!music_track->flac_stream)
	{
		music_track->flac_stream = m_arena_push_struct_z(&music_track->header.transient_arena, Flac_Stream);
		init_flac_stream(music_track->flac_stream, &music_track->header.transient_arena, music_track->flac_file_buffer);
		if (!music_track->flac_stream->seek_table)
		{
			flac_build_seek_table(music_track->flac_stream, &music_track->header.transient_arena, &music_track->build_seektable_work_data);
		}
		
		
		if (music_track->flac_stream->front_cover)
		{
			music_track->header.image.texture_data = music_track->flac_stream->front_cover->buffer;
			if (music_track->header.image.texture.state == Texture_State_Loading)
			{
				music_track->header.image.texture.state = Texture_State_Unloaded;
			}
			else
			{
				music_track->header.image.texture.state = Texture_State_Invalid;
			}
		}
	}
}

internal void
mplayer_unload_music_track(Mplayer_Context *mplayer, Mplayer_Track *music)
{
	music->build_seektable_work_data.cancel_req = 1;
	for (;music->build_seektable_work_data.running;)
	{
		// NOTE(fakhri): help worker threads instead of just waiting
		platform->do_next_work();
	}
	uninit_flac_stream(music->flac_stream);
	m_arena_free_all(&music->header.transient_arena);
	music->file_loaded = false;
	music->flac_stream = 0;
	music->flac_file_buffer = ZERO_STRUCT;
	
	// TODO(fakhri): delete texture from gpu memory!
	music->header.image.texture.state = Texture_State_Unloaded;
}

internal void
mplayer_play_music_track(Mplayer_Context *mplayer, Mplayer_Item_ID music_track_id)
{
	if (music_track_id)
	{
		Mplayer_Track *music_track = mplayer_track_by_id(&mplayer->library, music_track_id);
		
		if (mplayer->current_music_id != music_track_id)
		{
			Mplayer_Item_ID prev_music_id = mplayer->current_music_id;
			
			if (prev_music_id)
			{
				mplayer_unload_music_track(mplayer, 
					mplayer_track_by_id(&mplayer->library, prev_music_id));
			}
			
			mplayer_load_music_track(mplayer, music_track);
		}
		
		mplayer_music_reset(music_track);
		mplayer->current_music_id = music_track->header.id;
		mplayer->play_track = true;
	}
}

internal b32
is_music_track_still_playing(Mplayer_Track *music_track)
{
	b32 result = (music_track->flac_stream && 
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
	push_texture_upload_request(&mplayer->render_ctx->upload_buffer, font->atlas_tex, pixels_buf, 0);
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

enum Text_Render_Flags
{
	Text_Render_Flag_Centered    = (1 << 0),
	Text_Render_Flag_Limit_Width = (1 << 1),
	Text_Render_Flag_Underline   = (1 << 2),
};

struct Fancy_String8
{
	String8 text;
	f32 max_width;
	f32 underline_thickness;
};

#define fancy_str8_lit(str) fancy_str8(str8_lit(str))

internal Fancy_String8
fancy_str8(String8 text, f32 max_width = 0, f32 underline_thickness = 0)
{
	Fancy_String8 result = ZERO_STRUCT;
	result.text = text;
	result.max_width = max_width;
	result.underline_thickness = underline_thickness;
	return result;
}

internal Fancy_String8
fancy_str8(u8 *str, u64 len, f32 max_width = 0, f32 underline_thickness = 0)
{
	Fancy_String8 result = fancy_str8(str8(str, len), max_width, underline_thickness);
	return result;
}

internal V3_F32
draw_text(Render_Group *group, Mplayer_Font *font, V2_F32 pos, V4_F32 color, Fancy_String8 string, u32 flags = 0)
{
	f32 current_width = 0;
	b32 width_overflow = 0;
	
	f32 max_width = string.max_width;
	V2_F32 text_dim = font_compute_text_dim(font, string.text);
	
	if (has_flag(flags, Text_Render_Flag_Limit_Width))
	{
		if (text_dim.width > max_width)
		{
			text_dim.width = max_width;
			width_overflow = 1;
			Mplayer_Glyph glyph = font_get_glyph_from_char(font, '.');
			max_width -= 3 * glyph.dim.width;
			max_width = MAX(max_width, 0.0f);
		}
	}
	
	if (has_flag(flags, Text_Render_Flag_Centered))
	{
		pos.x -= 0.5f * text_dim.width;
		pos.y -= 0.25f * text_dim.height;
	}
	
	
	// NOTE(fakhri): render the text
	V3_F32 text_pt = vec3(pos, 0);
	for (u32 offset = 0;
		offset < string.text.len;
		offset += 1)
	{
		u8 ch = string.text.str[offset];
		// TODO(fakhri): utf8 support
		Mplayer_Glyph glyph = font_get_glyph_from_char(font, ch);
		if (has_flag(flags, Text_Render_Flag_Limit_Width) && current_width + glyph.dim.width > max_width)
		{
			break;
		}
		V3_F32 glyph_p = vec3(text_pt.xy + (glyph.offset), 0);
		push_image(group, 
			glyph_p, 
			glyph.dim,
			font->atlas_tex,
			color,
			0.0f,
			glyph.uv_scale, 
			glyph.uv_offset);
		
		if (has_flag(flags, Text_Render_Flag_Underline))
		{
			V2_F32 underline_dim = vec2(glyph.advance + 0.25f * glyph.dim.width, string.underline_thickness);
			V3_F32 underline_pos = vec3(glyph_p.x, text_pt.y - 5.0f, text_pt.z);
			push_rect(group, underline_pos, underline_dim, color);
		}
		text_pt.x += glyph.advance;
	}
	
	if (width_overflow)
	{
		draw_text(group, font, text_pt.xy, color, fancy_str8_lit("..."));
	}
	
	return text_pt;
}

internal void
draw_text_left_side_fixed_width(Render_Group *group, Mplayer_Font *font, Range2_F32 range, V4_F32 color, String8 text, b32 should_underline = 0, f32 underline_thickness = 0)
{
	V2_F32 text_dim = font_compute_text_dim(font, text);
	
	u32 flags = Text_Render_Flag_Limit_Width;
	if (should_underline)
	{
		flags |= Text_Render_Flag_Underline;
	}
	
	Fancy_String8 string = fancy_str8(text, range_dim(range).width, underline_thickness);
	draw_text(group, font, 
		vec2(range.min_x, range_center(range).y - 0.25f * text_dim.height), 
		color, 
		string, flags);
}

internal void
draw_circle(Render_Group *group, V2_F32 pos, f32 radius, V4_F32 color)
{
	push_rect(group, pos, vec2(2 * radius), color, radius);
}

internal void
draw_outline(Render_Group *group, V3_F32 pos, V2_F32 dim, V4_F32 color, f32 thickness)
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
draw_outline(Render_Group *group, V2_F32 pos, V2_F32 dim, V4_F32 color, f32 thickness)
{
	draw_outline(group, vec3(pos, 0), dim, color, thickness);
}

internal void
draw_outline(Render_Group *group, Range2_F32 rect, V4_F32 color, f32 thickness)
{
	draw_outline(group, range_center(rect), range_dim(rect), color, thickness);
}

internal void
mplayer_animate_scroll(Mplayer_Scroll_Data *scroll, f32 frame_dt, f32 freq = 5.0f, f32 zeta = 1.f)
{
	f32 current_scroll = scroll->t;
	f32 target_scroll  = scroll->target;
	f32 scroll_speed   = scroll->dt;
	
	f32 K1 = zeta / (PI32 * freq);
	f32 K2 = SQUARE(2 * PI32 * freq);
	
	f32 scroll_accel = K2 * (target_scroll - current_scroll - K1 * scroll_speed);
	scroll->dt += frame_dt * scroll_accel;
	scroll->t  += frame_dt * scroll->dt + 0.5f * SQUARE(frame_dt) * scroll_accel;
}

#include "mplayer_ui.cpp"

internal String8
mplayer_attempt_find_cover_image_in_dir(Mplayer_Context *mplayer, Memory_Arena *arena, Directory dir)
{
	String8 result = ZERO_STRUCT;
	for (u32 i = 0; i < dir.count; i += 1)
	{
		File_Info info = dir.files[i];
		
		if (!has_flag(info.flags, FileFlag_Directory))
		{
			if (str8_ends_with(info.name, str8_lit(".jpg"), MatchFlag_CaseInsensitive)  ||
				str8_ends_with(info.name, str8_lit(".jpeg"), MatchFlag_CaseInsensitive) ||
				str8_ends_with(info.name, str8_lit(".png"), MatchFlag_CaseInsensitive)
			)
			{
				result = str8_clone(arena, info.path);
				break;
			}
		}
	}
	return result;
}

internal void
mplayer_reset_library(Mplayer_Library *library)
{
	// NOTE(fakhri): wait for worker threads to end
	for (;platform->do_next_work();) {}
	m_arena_free_all(&library->arena);
	
	// NOTE(fakhri): reserve null ids
	library->artists_count = 1;
	library->albums_count = 1;
	library->tracks_count = 1;
	
}

#define MPLAYER_INDEX_FILENAME str8_lit("index.mplayer")

enum Mplayer_Library_Index_Version : u32
{
	INDEX_VERSION_NULL,
	INDEX_VERSION_1,
};

internal void
mplayer_serialize(File_Handle *file, u32 num)
{
	platform->write_block(file, &num, sizeof(num));
}

internal void
mplayer_serialize(File_Handle *file, b32 num)
{
	platform->write_block(file, &num, sizeof(num));
}

internal void
mplayer_serialize(File_Handle *file, u64 num)
{
	platform->write_block(file, &num, sizeof(num));
}

internal void
mplayer_serialize(File_Handle *file, String8 string)
{
	mplayer_serialize(file, string.len);
	platform->write_block(file, string.str, string.len);
}

internal void
mplayer_serialize(File_Handle *file, Buffer buffer)
{
	mplayer_serialize(file, buffer.size);
	platform->write_block(file, buffer.data, buffer.size);
}

internal void
mplayer_serialize(File_Handle *file, Mplayer_Items_Array items_array)
{
	mplayer_serialize(file, items_array.count);
	platform->write_block(file, items_array.items, items_array.count * sizeof(Mplayer_Item_ID));
}

internal void
mplayer_serialize(File_Handle *file, Mplayer_Item_Image image)
{
	mplayer_serialize(file, image.in_disk);
	if (image.in_disk)
	{
		mplayer_serialize(file, image.path);
	}
	else
	{
		mplayer_serialize(file, image.texture_data);
	}
}

internal void
mplayer_save_indexed_library(Mplayer_Context *mplayer)
{
	String8 index_path = MPLAYER_INDEX_FILENAME;
	File_Handle *index_file = platform->open_file(index_path, File_Open_Write | File_Create_Always);
	
	if (index_file)
	{
		mplayer_serialize(index_file, INDEX_VERSION_1);
		
		mplayer_serialize(index_file, mplayer->library.tracks_count);
		mplayer_serialize(index_file, mplayer->library.albums_count);
		mplayer_serialize(index_file, mplayer->library.artists_count);
		
		for (u32 artist_id = 1; artist_id < mplayer->library.artists_count; artist_id += 1)
		{
			Mplayer_Artist *artist = mplayer->library.artists + artist_id;
			
			mplayer_serialize(index_file, artist->name);
			
			mplayer_serialize(index_file, artist->tracks);
			mplayer_serialize(index_file, artist->albums);
		}
		
		for (u32 album_id = 1; album_id < mplayer->library.albums_count; album_id += 1)
		{
			Mplayer_Album *album = mplayer->library.albums + album_id;
			
			mplayer_serialize(index_file, album->name);
			mplayer_serialize(index_file, album->artist);
			mplayer_serialize(index_file, album->artist_id);
			mplayer_serialize(index_file, album->tracks);
			
			mplayer_serialize(index_file, album->header.image);
		}
		
		for (u32 track_id = 1; track_id < mplayer->library.tracks_count; track_id += 1)
		{
			Mplayer_Track *track = mplayer->library.tracks + track_id;
			
			mplayer_serialize(index_file, track->path);
			mplayer_serialize(index_file, track->title);
			mplayer_serialize(index_file, track->album);
			mplayer_serialize(index_file, track->artist);
			mplayer_serialize(index_file, track->genre);
			mplayer_serialize(index_file, track->date);
			mplayer_serialize(index_file, track->track_number);
			
			mplayer_serialize(index_file, track->artist_id);
			mplayer_serialize(index_file, track->album_id);
		}
		
		
		platform->close_file(index_file);
	}
}

internal b32
mplayer_load_indexed_library(Mplayer_Context *mplayer)
{
	b32 success = 0;
	mplayer_reset_library(&mplayer->library);
	
	String8 index_path = MPLAYER_INDEX_FILENAME;
	
	Buffer lib_index = platform->load_entire_file(index_path, &mplayer->library.arena);
	if (is_valid(lib_index))
	{
		success = 1;
		Byte_Stream bs = make_byte_stream(lib_index);
		u32 index_version;
		byte_stream_read(&bs, &index_version);
		assert(INDEX_VERSION_1 == index_version);
		switch (index_version)
		{
			case INDEX_VERSION_1:
			{
				byte_stream_read(&bs, &mplayer->library.tracks_count);
				byte_stream_read(&bs, &mplayer->library.albums_count);
				byte_stream_read(&bs, &mplayer->library.artists_count);
				
				
				for (u32 artist_id = 1; artist_id < mplayer->library.artists_count; artist_id += 1)
				{
					Mplayer_Artist *artist = mplayer->library.artists + artist_id;
					artist->header.id = artist_id;
					mplayer_reset_item_header(&artist->header);
					
					byte_stream_read(&bs, &artist->name);
					
					byte_stream_read(&bs, &artist->tracks);
					byte_stream_read(&bs, &artist->albums);
				}
				
				for (u32 album_id = 1; album_id < mplayer->library.albums_count; album_id += 1)
				{
					Mplayer_Album *album = mplayer->library.albums + album_id;
					album->header.id = album_id;
					mplayer_reset_item_header(&album->header);
					
					byte_stream_read(&bs, &album->name);
					byte_stream_read(&bs, &album->artist);
					byte_stream_read(&bs, &album->artist_id);
					byte_stream_read(&bs, &album->tracks);
					
					byte_stream_read(&bs, &album->header.image);
					if (album->header.image.in_disk)
					{
						album->header.image.path = str8_clone(&mplayer->library.arena, album->header.image.path);
					}
					else
					{
						album->header.image.texture_data = clone_buffer(&mplayer->library.arena, album->header.image.texture_data);
					}
				}
				
				for (u32 track_id = 1; track_id < mplayer->library.tracks_count; track_id += 1)
				{
					Mplayer_Track *track = mplayer->library.tracks + track_id;
					track->header.id = track_id;
					mplayer_reset_item_header(&track->header);
					
					byte_stream_read(&bs, &track->path);
					byte_stream_read(&bs, &track->title);
					byte_stream_read(&bs, &track->album);
					byte_stream_read(&bs, &track->artist);
					byte_stream_read(&bs, &track->genre);
					byte_stream_read(&bs, &track->date);
					byte_stream_read(&bs, &track->track_number);
					
					byte_stream_read(&bs, &track->artist_id);
					byte_stream_read(&bs, &track->album_id);
				}
				
			} break;
		}
	}
	
	return success;
}

internal void
mplayer_load_tracks_in_directory(Mplayer_Context *mplayer, String8 library_path)
{
	Memory_Checkpoint temp_mem = m_checkpoint_begin(&mplayer->frame_arena);
	Directory dir = platform->read_directory(temp_mem.arena, library_path);
	for (u32 info_index = 0; info_index < dir.count; info_index += 1)
	{
		Memory_Checkpoint scratch = get_scratch(0, 0);
		File_Info info = dir.files[info_index];
		
		if (has_flag(info.flags, FileFlag_Directory))
		{
			// NOTE(fakhri): directory
			if (info.name.str[0] != '.')
			{
				mplayer_load_tracks_in_directory(mplayer, info.path);
			}
		}
		else
		{
			if (str8_ends_with(info.name, str8_lit(".flac"), MatchFlag_CaseInsensitive))
			{
				// TODO(fakhri): make sure the track is not already indexed
				Mplayer_Track *music_track = mplayer_make_track(&mplayer->library);
				
				music_track->path = str8_clone(&mplayer->library.arena, info.path);
				
				Buffer tmp_file_block = arena_push_buffer(scratch.arena, megabytes(1));
				File_Handle *file = platform->open_file(music_track->path, File_Open_Read);
				Flac_Stream tmp_flac_stream = ZERO_STRUCT;
				assert(file);
				if (file)
				{
					Buffer buffer = platform->read_block(file, tmp_file_block.data, tmp_file_block.size);
					platform->close_file(file);
					
					// NOTE(fakhri): init flac stream
					{
						tmp_flac_stream.bitstream.buffer = buffer;
						tmp_flac_stream.bitstream.pos.byte_index = 0;
						tmp_flac_stream.bitstream.pos.bits_left  = 8;
					}
					flac_process_metadata(&tmp_flac_stream, scratch.arena);
					
					music_track->album        = str8_lit("***Unkown Album***");
					music_track->artist       = str8_lit("***Unkown Artist***");
					music_track->date         = str8_lit("***Unkown Date***");
					music_track->genre        = str8_lit("***Unkown Gener***");
					music_track->track_number = str8_lit("-");
					
					for (String8_Node *node = tmp_flac_stream.vorbis_comments.first;
						node;
						node = node->next)
					{
						String8 value = ZERO_STRUCT;
						String8 key = ZERO_STRUCT;
						for (u32 i = 0; i < node->str.len; i += 1)
						{
							if (node->str.str[i] == '=')
							{
								key   = prefix8(node->str, i);
								value = suffix8(node->str, node->str.len - i - 1);
								break;
							}
						}
						
						if (false) {}
						else if (str8_match(key, str8_lit("title"), MatchFlag_CaseInsensitive))
						{
							music_track->title = str8_clone(&mplayer->library.arena, value);
						}
						else if (str8_match(key, str8_lit("album"), MatchFlag_CaseInsensitive))
						{
							music_track->album = str8_clone(&mplayer->library.arena, value);
						}
						else if (str8_match(key, str8_lit("artist"), MatchFlag_CaseInsensitive))
						{
							music_track->artist = str8_clone(&mplayer->library.arena, value);
						}
						else if (str8_match(key, str8_lit("genre"), MatchFlag_CaseInsensitive))
						{
							music_track->genre = str8_clone(&mplayer->library.arena, value);
						}
						else if (str8_match(key, str8_lit("data"), MatchFlag_CaseInsensitive))
						{
							music_track->date = str8_clone(&mplayer->library.arena, value);
						}
						else if (str8_match(key, str8_lit("tracknumber"), MatchFlag_CaseInsensitive))
						{
							music_track->track_number = str8_clone(&mplayer->library.arena, value);
						}
					}
				}
				
				if (!music_track->title.len)
				{
					music_track->title = str8_clone(&mplayer->library.arena, info.name);
				}
				
				// TODO(fakhri): use parent directory name as album name if the track doesn't contain an album name
				
				Mplayer_Artist *artist = mplayer_find_or_create_artist(mplayer, music_track->artist);
				artist->tracks.count += 1;
				music_track->artist_id = artist->header.id;
				Mplayer_Album *album = mplayer_find_or_create_album(mplayer, artist, music_track->album);
				album->tracks.count += 1;
				music_track->album_id = album->header.id;
				
				if (!album->header.image.texture_data.size)
				{
					album->header.image.texture = ZERO_STRUCT;
					
					String8 cover_file_path = mplayer_attempt_find_cover_image_in_dir(mplayer, &mplayer->library.arena, dir);
					if (cover_file_path.len)
					{
						album->header.image.in_disk = true;
						album->header.image.path = cover_file_path;
					}
					else if (tmp_flac_stream.front_cover)
					{
						Flac_Picture *front_cover = tmp_flac_stream.front_cover;
						
						album->header.image.in_disk = false;
						album->header.image.texture_data = clone_buffer(&mplayer->library.arena, front_cover->buffer);
					}
				}
				
			}
		}
	}
}

internal void
mplayer_index_library(Mplayer_Context *mplayer)
{
	Mplayer_Library *library = &mplayer->library;
	mplayer_reset_library(library);
	
	for (u32 i = 0; i < mplayer->settings.locations_count; i += 1)
	{
		Mplayer_Library_Location *location = mplayer->settings.locations + i;
		mplayer_load_tracks_in_directory(mplayer, str8(location->path, location->path_len));
	}
	
	for (u32 artist_id = 1; artist_id < library->artists_count; artist_id += 1)
	{
		Mplayer_Artist *artist = mplayer_artist_by_id(library, artist_id);
		artist->tracks.items = m_arena_push_array(&library->arena, Mplayer_Item_ID, artist->tracks.count);
		artist->albums.items = m_arena_push_array(&library->arena, Mplayer_Item_ID, artist->albums.count);
		
		artist->tracks.count = 0;
		artist->albums.count = 0;
	}
	
	for (u32 album_id = 1; album_id < library->albums_count; album_id += 1)
	{
		Mplayer_Album *album = mplayer_album_by_id(library, album_id);
		album->tracks.items = m_arena_push_array(&library->arena, Mplayer_Item_ID, album->tracks.count);
		album->tracks.count = 0;
		
		Mplayer_Artist *artist = mplayer_artist_by_id(library, album->artist_id);
		artist->albums.items[artist->albums.count++] = album_id;
	}
	
	for (u32 track_id = 1; track_id < library->tracks_count; track_id += 1)
	{
		Mplayer_Track *track = mplayer_track_by_id(library, track_id);
		
		Mplayer_Artist *artist = mplayer_artist_by_id(library, track->artist_id);
		artist->tracks.items[artist->tracks.count++] = track_id;
		
		Mplayer_Album *album= mplayer_album_by_id(library, track->album_id);
		album->tracks.items[album->tracks.count++] = track_id;
	}
	mplayer_save_indexed_library(mplayer);
}

internal void
mplayer_load_library(Mplayer_Context *mplayer)
{
	if (!mplayer_load_indexed_library(mplayer))
	{
		mplayer_index_library(mplayer);
	}
}


internal void
mplayer_change_previous_mode(Mplayer_Context *mplayer)
{
	if (mplayer->mode_stack && mplayer->mode_stack->prev)
	{
		mplayer->mode_stack = mplayer->mode_stack->prev;
		
		switch(mplayer->mode_stack->mode)
		{
			case MODE_Music_Library: break;
			case MODE_Artist_Library: break;
			case MODE_Album_Library: break;
			
			case MODE_Artist_Albums:
			{
				assert(mplayer->mode_stack->item_id);
				
				mplayer->selected_artist_id = mplayer->mode_stack->item_id;
			} break;
			case MODE_Album_Tracks:
			{
				assert(mplayer->mode_stack->item_id);
				
				mplayer->selected_album_id = mplayer->mode_stack->item_id;
			} break;
			
			case MODE_Lyrics: break;
			case MODE_Settings: break;
		}
	}
}

internal void
mplayer_change_next_mode(Mplayer_Context *mplayer)
{
	if (mplayer->mode_stack && mplayer->mode_stack->next)
	{
		mplayer->mode_stack = mplayer->mode_stack->next;
		
		switch(mplayer->mode_stack->mode)
		{
			case MODE_Music_Library: break;
			case MODE_Artist_Library: break;
			case MODE_Album_Library: break;
			
			case MODE_Artist_Albums:
			{
				assert(mplayer->mode_stack->item_id);
				
				mplayer->selected_artist_id = mplayer->mode_stack->item_id;
			} break;
			case MODE_Album_Tracks:
			{
				assert(mplayer->mode_stack->item_id);
				
				mplayer->selected_album_id = mplayer->mode_stack->item_id;
			} break;
			
			case MODE_Lyrics: break;
			case MODE_Settings: break;
		}
	}
}

internal void
mplayer_change_mode(Mplayer_Context *mplayer, Mplayer_Mode new_mode, Mplayer_Item_ID item_id)
{
	if (mplayer->mode_stack && mplayer->mode_stack->next)
	{
		QueuePush(mplayer->mode_stack_free_list_first, mplayer->mode_stack_free_list_last, mplayer->mode_stack->next);
		mplayer->mode_stack->next = 0;
	}
	
	Mplayer_Mode_Stack *new_mode_node = 0;
	if (mplayer->mode_stack_free_list_first)
	{
		new_mode_node = mplayer->mode_stack_free_list_first;
		QueuePop(mplayer->mode_stack_free_list_first, mplayer->mode_stack_free_list_last);
	}
	
	if (!new_mode_node)
	{
		new_mode_node = m_arena_push_struct_z(&mplayer->main_arena, Mplayer_Mode_Stack);
	}
	
	new_mode_node->mode = new_mode;
	new_mode_node->item_id = item_id;
	
	new_mode_node->prev = mplayer->mode_stack;
	if (mplayer->mode_stack)
	{
		mplayer->mode_stack->next = new_mode_node;
	}
	mplayer->mode_stack = new_mode_node;
	
	mplayer->mode_stack->scroll.target = 0;
	mplayer->mode_stack->scroll.t = 0;
	
	mplayer->ui.active_widget_id = 0;
	mplayer->ui.hot_widget_id = 0;
}

internal void
mplayer_add_location(Mplayer_Context *mplayer, String8 location_path)
{
	assert(mplayer->settings.locations_count < array_count(mplayer->settings.locations));
	u32 i = mplayer->settings.locations_count++;
	Mplayer_Library_Location *location = mplayer->settings.locations + i;
	assert(location_path.len < array_count(location->path));
	
	location->path_len = u32(location_path.len);
	memory_copy(location->path, location_path.str, location_path.len);
}

#define SETTINGS_FILE_NAME str8_lit("settings.mplayer")
internal void
mplayer_load_settings(Mplayer_Context *mplayer)
{
	// TODO(fakhri): save the config in user directory
	String8 config_path = SETTINGS_FILE_NAME;
	Buffer config_content = platform->load_entire_file(config_path, &mplayer->frame_arena);
	
	if (config_content.size)
	{
		assert(config_content.size == sizeof(MPlayer_Settings));
		memory_copy(&mplayer->settings, config_content.data, config_content.size);
	}
	else
	{
		mplayer->settings = DEFAUTL_SETTINGS;
	}
}

internal void
mplayer_save_settings(Mplayer_Context *mplayer)
{
	// TODO(fakhri): save the config in user directory
	String8 config_path = SETTINGS_FILE_NAME;
	File_Handle *config_file = platform->open_file(config_path, File_Open_Write | File_Create_Always);
	
	if (config_file)
	{
		platform->write_block(config_file, &mplayer->settings, sizeof(mplayer->settings));
		platform->close_file(config_file);
	}
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
mplayer_filter_path_lister(Mplayer_Path_Lister *path_lister)
{
	path_lister->filtered_paths_count = 0;
	
	for (u32 i = 0; i < path_lister->dir.count; i += 1)
	{
		File_Info info = path_lister->dir.files[i];
		if (has_flag(info.flags, FileFlag_Directory) && 
			info.name.str[0] != '.' &&
			str8_is_subsequence(info.path, path_lister->user_path, MatchFlag_CaseInsensitive | MatchFlag_SlashInsensitive))
		{
			path_lister->filtered_paths[path_lister->filtered_paths_count++] = info.path;
		}
	}
}

internal void
mplayer_refresh_path_lister_dir(Mplayer_Path_Lister *path_lister)
{
	String8 base_dir = str8_chop_last_slash(path_lister->user_path);
	if (!str8_match(base_dir, path_lister->base_dir, MatchFlag_CaseInsensitive))
	{
		m_arena_free_all(&path_lister->arena);
		path_lister->base_dir = base_dir;
		path_lister->dir = platform->read_directory(&path_lister->arena, base_dir);
		path_lister->filtered_paths = m_arena_push_array(&path_lister->arena, String8, path_lister->dir.count);
	}
	mplayer_filter_path_lister(path_lister);
}

internal void
mplayer_set_path_lister_path(Mplayer_Path_Lister *path_lister, String8 path)
{
	assert(path.len <= array_count(path_lister->user_path_buffer));
	if (path.len <= array_count(path_lister->user_path_buffer))
	{
		memory_copy(path_lister->user_path_buffer, path.str, path.len);
		path_lister->user_path.len = path.len;
		
		if (path.str[path.len - 1] != '/' && path.str[path.len - 1] != '\\')
		{
			assert(path.len + 1 <= array_count(path_lister->user_path_buffer));
			path_lister->user_path_buffer[path.len] = '/';
			path_lister->user_path.len += 1;
		}
		mplayer_refresh_path_lister_dir(path_lister);
	}
}

internal void
mplayer_open_path_lister(Mplayer_Context *mplayer, Mplayer_Path_Lister *path_lister)
{
	Memory_Checkpoint scratch = get_scratch(0, 0);
	
	path_lister->user_path = str8(path_lister->user_path_buffer, 0);
	String8 cwd = platform->get_current_directory(scratch.arena);
	mplayer_set_path_lister_path(path_lister, cwd);
	mplayer->show_path_popup = true;
}

internal void
mplayer_close_path_lister(Mplayer_Context *mplayer, Mplayer_Path_Lister *path_lister)
{
	m_arena_free_all(&path_lister->arena);
	path_lister->filtered_paths_count = 0;
	path_lister->base_dir = ZERO_STRUCT;
	mplayer->show_path_popup = false;
	
}

exported void
mplayer_get_audio_samples(Sound_Config device_config, Mplayer_Context *mplayer, void *output_buf, u32 frame_count)
{
	if (mplayer->play_track)
	{
		Mplayer_Track *music = mplayer_track_by_id(&mplayer->library, mplayer->current_music_id);
		if (music)
		{
			Flac_Stream *flac_stream = music->flac_stream;
			f32 volume = mplayer->volume;
			if (flac_stream)
			{
				u32 channels_count = flac_stream->streaminfo.nb_channels;
				
				// TODO(fakhri): convert to device channel layout
				assert(channels_count == device_config.channels_count);
				
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

exported void
mplayer_initialize(Mplayer_Context *mplayer)
{
	load_font(mplayer, &mplayer->font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->debug_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->big_debug_font, str8_lit("data/fonts/arial.ttf"), 50);
	load_font(mplayer, &mplayer->timestamp_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->header_label_font, str8_lit("data/fonts/arial.ttf"), 40);
	mplayer->play_track = 0;
	mplayer->volume = 1.0f;
	
	mplayer_load_settings(mplayer);
	
	mplayer_load_library(mplayer);
	
	mplayer_change_mode(mplayer, MODE_Album_Library, 0);
}

exported  void
mplayer_hotload(Mplayer_Context *mplayer)
{
	platform = &mplayer->os;
}

exported void
mplayer_update_and_render(Mplayer_Context *mplayer)
{
	Render_Context *render_ctx = mplayer->render_ctx;
	
	Range2_F32 screen_rect = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	Render_Group group = begin_render_group(render_ctx, vec2(0, 0), render_ctx->draw_dim, screen_rect);
	
	M4_Inv proj = group.config.proj;
	V2_F32 world_mouse_p = (proj.inv * vec4(mplayer->input.mouse_clip_pos)).xy;
	push_clear_color(render_ctx, vec4(0.1f, 0.1f, 0.1f, 1));
	
	Mplayer_Track *current_music = mplayer_track_by_id(&mplayer->library, mplayer->current_music_id);
	if (mplayer->current_music_id && !is_music_track_still_playing(current_music))
	{
		// TODO(fakhri): play next song in queue
		mplayer_play_music_track(mplayer, current_music->header.id + 1);
		current_music = mplayer_track_by_id(&mplayer->library, mplayer->current_music_id);
	}
	
	if (mplayer_button_clicked(&mplayer->input, Key_Mouse_M4))
	{
		mplayer_change_previous_mode(mplayer);
	}
	
	if (mplayer_button_clicked(&mplayer->input, Key_Mouse_M5))
	{
		mplayer_change_next_mode(mplayer);
	}
	
	Mplayer_UI *ui = &mplayer->ui;;
	ui_begin(ui, mplayer, &group, world_mouse_p);
	
	if (mplayer->show_path_popup)
	{
		ui->disable_input = true;
	}
	
	#if 1
		
		ui_vertical_layout(ui)
	{
		ui_pref_height(ui, ui_size_parent_remaining())
			ui_horizontal_layout(ui)
		{
			ui_next_background_color(ui, vec4(0.06f, 0.06f, 0.06f, 1.0f));
			ui_next_width(ui, ui_size_pixel(150, 1));
			ui_next_border_color(ui, vec4(0.f, 0.f, 0.f, 1.0f));
			ui_next_border_thickness(ui, 2);
			ui_vertical_layout(ui)
			{
				ui_pref_background_color(ui, vec4(0.1f, 0.1f, 0.1f, 1)) ui_pref_text_color(ui, vec4(1, 1, 1, 1))
					ui_pref_width(ui, ui_size_percent(1, 1)) ui_pref_height(ui, ui_size_text_dim(1))
				{
					ui_spacer(ui, ui_size_pixel(20, 1));
					if (ui_button(ui, str8_lit("Songs")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Music_Library)
							mplayer_change_mode(mplayer, MODE_Music_Library, 0);
					}
					
					ui_spacer(ui, ui_size_pixel(2, 1));
					if (ui_button(ui, str8_lit("Artists")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Artist_Library)
							mplayer_change_mode(mplayer, MODE_Artist_Library, 0);
					}
					
					ui_spacer(ui, ui_size_pixel(2, 1));
					if (ui_button(ui, str8_lit("Albums")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Album_Library)
							mplayer_change_mode(mplayer, MODE_Album_Library, 0);
					}
					
					ui_spacer(ui, ui_size_pixel(2, 1));
					if (ui_button(ui, str8_lit("Lyrics")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Lyrics)
							mplayer_change_mode(mplayer, MODE_Lyrics, 0);
					}
					
					ui_spacer(ui, ui_size_pixel(2, 1));
					if (ui_button(ui, str8_lit("Settings")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Settings)
							mplayer_change_mode(mplayer, MODE_Settings, 0);
					}
				}
			}
			
			ui_next_width(ui, ui_size_parent_remaining());
			ui_vertical_layout(ui)
			{
				switch(mplayer->mode_stack->mode)
				{
					case MODE_Artist_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(69, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer(ui, ui_size_pixel(50, 0));
							
							ui_next_background_color(ui, vec4(1, 1, 1, 0));
							ui_next_text_color(ui, vec4(1, 1, 1, 1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, str8_lit("Artists"));
						}
						
						// NOTE(fakhri): library artists list
						{
							
						}
					} break;
					
					case MODE_Album_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(69, 1));
						ui_next_flags(ui, UI_FLAG_Allow_OverflowX);
						ui_horizontal_layout(ui)
						{
							ui_spacer(ui, ui_size_pixel(50, 1));
							
							ui_next_background_color(ui, vec4(1, 1, 1, 0));
							ui_next_text_color(ui, vec4(1, 1, 1, 1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, str8_lit("Albums"));
						}
						
						UI_Element *grid = ui_element(ui, str8_lit("library-albums-grid"), UI_FLAG_Allow_OverflowY);
						grid->child_layout_axis = Axis2_Y;
						
						f32 grid_item_min_width = 250.0f;
						f32 grid_item_min_height = 220.0f;
						ui_parent(ui, grid)
						{
							f32 padding = 50;
							u32 childs_count_per_row = (u32)((grid->computed_dim.width - 2 * padding) / grid_item_min_width);
							childs_count_per_row = MAX(childs_count_per_row, 1);
							f32 childs_width_sum = childs_count_per_row * grid_item_min_width;
							
							u32 rows_count = u32(grid->computed_dim.height / grid_item_min_height) + 1;
							
							Mplayer_Item_ID album_id = 1;
							for (u32 row = 0; row < rows_count; row += 1)
							{
								b32 done = 0;
								
								ui_next_height(ui, ui_size_pixel(grid_item_min_height, 1));
								ui_horizontal_layout(ui)
								{
									ui_spacer(ui, ui_size_pixel(padding, 1));
									ui_spacer(ui, ui_size_parent_remaining());
									for (u32 i = 0; 
										i < childs_count_per_row && album_id < mplayer->library.albums_count; 
										i += 1, album_id += 1)
									{
										
										Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, album_id);
										mplayer_load_item_image(mplayer, &album->header.transient_arena, &album->header.image);
										
										UI_Element_Flags flags = 0;
										if (mplayer_item_image_ready(&album->header.image))
										{
											flags |= UI_FLAG_Draw_Image;
											ui_next_texture_tint_color(ui, vec4(1, 1, 1, 0.35f));
											ui_next_texture(ui, album->header.image.texture);
										}
										else
										{
											flags |= UI_FLAG_Draw_Background;
											ui_next_background_color(ui, vec4(1, 1, 1, 0.35f));
										}
										
										ui_next_roundness(ui, 10);
										ui_next_width(ui, ui_size_pixel(grid_item_min_width, 1));
										ui_next_height(ui, ui_size_pixel(grid_item_min_height, 1));
										ui_next_flags(ui, UI_FLAG_Allow_OverflowY | UI_FLAG_Allow_OverflowX | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim);
										UI_Element *album_el = ui_element_f(ui, flags, "library_album_%p", album);
										album_el->child_layout_axis = Axis2_Y;
										
										ui_parent(ui, album_el)
										{
											ui_spacer(ui, ui_size_pixel(10, 1));
											
											ui_next_width(ui, ui_size_percent(1, 1));
											ui_next_height(ui, ui_size_text_dim(1));
											ui_element_f(ui, UI_FLAG_Draw_Text | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim, "%.*s##library_album_name_%p", 
												STR8_EXPAND(album->name), album);
										}
										ui_spacer(ui, ui_size_parent_remaining());
									}
									ui_spacer(ui, ui_size_parent_remaining());
									ui_spacer(ui, ui_size_pixel(padding, 1));
								}
							}
							
							#if 0
								// NOTE(fakhri): library albums list
								for (Mplayer_Item_ID album_id = 1; album_id < mplayer->library.albums_count; album_id += 1)
							{
								Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, album_id);
								mplayer_load_item_image(mplayer, &album->header.transient_arena, &album->header.image);
								
								UI_Element_Flags flags = 0;
								if (mplayer_item_image_ready(&album->header.image))
								{
									flags |= UI_FLAG_Draw_Image;
									ui_next_texture_tint_color(ui, vec4(1, 1, 1, 0.35f));
									ui_next_texture(ui, album->header.image.texture);
								}
								else
								{
									flags |= UI_FLAG_Draw_Background;
									ui_next_background_color(ui, vec4(1, 1, 1, 0.35f));
								}
								
								ui_next_width(ui, ui_size_pixel(350, 1));
								ui_next_height(ui, ui_size_pixel(300, 1));
								ui_next_flags(ui, UI_FLAG_Allow_OverflowY | UI_FLAG_Allow_OverflowX);
								UI_Element *album_el = ui_element(ui, str8_lit(""), flags);
								album_el->child_layout_axis = Axis2_Y;
								ui_parent(ui, album_el)
								{
									ui_spacer(ui, ui_size_pixel(10, 1));
									
									ui_next_background_color(ui, vec4(1, 1, 1, 0));
									ui_next_width(ui, ui_size_percent(1, 1));
									ui_next_height(ui, ui_size_text_dim(1));
									ui_label(ui, album->name);
								}
							}
							#endif
						}
					} break;
					
					case MODE_Artist_Albums:
					{
						
					} break;
					
					case MODE_Album_Tracks:
					{
						
					} break;
					
					case MODE_Music_Library:
					{
						
					} break;
					
					case MODE_Lyrics:
					{
						
					} break;
					
					case MODE_Settings:
					{
						
					} break;
				}
			}
		}
		
		// NOTE(fakhri): track control
		ui_next_background_color(ui, vec4(0.f, 0.f, 0.f, 1));
		ui_pref_height(ui, ui_size_pixel(125, 1))
			ui_vertical_layout(ui)
		{
			
		}
	}
	
	ui_end(ui);
	
	#else
		Range2_F32 available_space = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	Range2_F32 track_control_rect;
	{
		Range2_F32_Cut cut = range_cut_bottom(available_space, 125);
		available_space    = cut.top;
		track_control_rect = cut.bottom;
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
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Songs"), range_center(cut.top)).clicked)
			{
				if (mplayer->mode_stack->mode != MODE_Music_Library)
					mplayer_change_mode(mplayer, MODE_Music_Library, 0);
			}
		}
		
		// NOTE(fakhri): Library button  
		cut = range_cut_top(cut.bottom, 20);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Artists"), range_center(cut.top)).clicked)
			{
				if (mplayer->mode_stack->mode != MODE_Artist_Library)
					mplayer_change_mode(mplayer, MODE_Artist_Library, 0);
			}
		}
		
		// NOTE(fakhri): Library button  
		cut = range_cut_top(cut.bottom, 20);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Album"), range_center(cut.top)).clicked)
			{
				if (mplayer->mode_stack->mode != MODE_Album_Library)
					mplayer_change_mode(mplayer, MODE_Album_Library, 0);
			}
		}
		
		// NOTE(fakhri):  Lyrics button  
		cut = range_cut_top(cut.bottom, 20);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Lyrics"), range_center(cut.top)).clicked)
			{
				mplayer_change_mode(mplayer, MODE_Lyrics, 0);
			}
		}
		
		// NOTE(fakhri):  Settings button  
		cut = range_cut_top(cut.bottom, 20);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Settings"), range_center(cut.top)).clicked)
			{
				mplayer_change_mode(mplayer, MODE_Settings, 0);
			}
		}
	}
	
	
	// TODO(fakhri): buttons to play all artist songs
	// TODO(fakhri): search bar
	
	V4_F32 header_bg_color = vec4(0.05f, 0.05f, 0.05f, 1.0f);
	switch (mplayer->mode_stack->mode)
	{
		case MODE_Artist_Library:
		{
			Range2_F32_Cut cut = range_cut_top(available_space, 65);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				
				String8 artist_label = str8_lit("Artists");
				f32 artist_label_width = font_compute_text_dim(&mplayer->header_label_font, artist_label).width;
				Range2_F32_Cut cut2 = range_cut_left(header, 50); // padding
				cut2 = range_cut_left(cut2.right, artist_label_width + 10);
				
				Range2_F32 label_rect = cut2.left;
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(artist_label), Text_Render_Flag_Centered);
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_set_cull_range(&group, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			// NOTE(fakhri): library artists list
			{
				u32 first_id = ui_begin_list(&mplayer->ui, &mplayer->mode_stack->scroll, vec2(350, 300), available_space, mplayer->library.artists_count - 1, vec2(10, 10)) + 1;
				
				u32 selected_artist_id = 0;
				for (u32 artist_id = first_id; artist_id < mplayer->library.artists_count; artist_id += 1)
				{
					Mplayer_List_Option_Result list_opt = mplayer_artist_list_option(&group, mplayer, artist_id);
					if (!list_opt.handled) break;
					if (list_opt.selected)
					{
						selected_artist_id = artist_id;
					}
				}
				
				if (selected_artist_id)
				{
					mplayer->selected_artist_id = selected_artist_id;
					mplayer_change_mode(mplayer, MODE_Artist_Albums, selected_artist_id);
				}
			}
			
		} break;
		case MODE_Artist_Albums:
		{
			assert(mplayer->selected_artist_id);
			Mplayer_Artist *artist = mplayer_artist_by_id(&mplayer->library, mplayer->selected_artist_id);
			
			f32 header_height = 65;
			Range2_F32_Cut cut = range_cut_top(available_space, header_height);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				Range2_F32_Cut cut2 = range_cut_left(header, 50); // padding
				
				cut2 = range_cut_left(cut2.right, 500);
				draw_text_left_side_fixed_width(&group, &mplayer->header_label_font, cut2.left, vec4(1, 1, 1, 1), artist->name);
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_set_cull_range(&group, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			// NOTE(fakhri): artist albums list
			{
				u32 index = ui_begin_list(&mplayer->ui, &mplayer->mode_stack->scroll, vec2(350, 300), available_space, artist->albums.count, vec2(10, 10));
				
				u32 selected_album_id = 0;
				for (; index < artist->albums.count; index += 1)
				{
					Mplayer_Item_ID album_id = artist->albums.items[index];
					Mplayer_List_Option_Result list_opt = mplayer_album_list_option(&group, mplayer, album_id);
					if (!list_opt.handled) break;
					if (list_opt.selected)
					{
						selected_album_id = album_id;
					}
				}
				
				if (selected_album_id)
				{
					mplayer->selected_album_id = selected_album_id;
					mplayer_change_mode(mplayer, MODE_Album_Tracks, selected_album_id);
				}
			}
		} break;
		
		case MODE_Album_Tracks:
		{
			Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, mplayer->selected_album_id);
			
			f32 header_height = 200;
			Range2_F32_Cut cut = range_cut_top(available_space, header_height);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				Range2_F32_Cut cut2 = range_cut_left(header, 10); // padding
				
				// NOTE(fakhri): album cover
				{
					cut2 = range_cut_left(cut2.right, header_height);
					Range2_F32 cover_rect = cut2.left;
					
					mpalyer_draw_item_image(&group, mplayer, &album->header, 
						range_center(cover_rect), 0.9f * range_dim(cover_rect), vec4(0.2f, 0.2f, 0.2f, 1.0f), vec4(1,1,1,1), 10);
				}
				
				// NOTE(fakhri): Album information region
				{
					cut2 = range_cut_left(cut2.right, 500); // info region
					Range2_F32 info_region = cut2.left;
					
					Range2_F32_Cut cut3 = range_cut_top(info_region, 30); // padding
					
					// NOTE(fakhri): "Album" label
					{
						cut3 = range_cut_top(cut3.bottom, 20);
						draw_text_left_side_fixed_width(&group, &mplayer->font, cut3.top, vec4(1, 1, 1, 1), str8_lit("Album"));
					}
					
					cut3 = range_cut_top(cut3.bottom, 30); // padding
					// NOTE(fakhri): album name
					{
						cut3 = range_cut_top(cut3.bottom, 20); // Album name
						draw_text_left_side_fixed_width(&group, &mplayer->header_label_font, cut3.top, vec4(1, 1, 1, 1), album->name);
					}
					
					cut3 = range_cut_top(cut3.bottom, 20); // padding
					// NOTE(fakhri): artist name
					{
						Mplayer_Font *artist_name_font = &mplayer->font;
						
						cut3 = range_cut_top(cut3.bottom, 30); // Album name
						
						V4_F32 text_color = vec4(0.4f, 0.4f, 0.4f, 1);
						b32 should_underline = 0;
						f32 underline_thickness = 0;
						
						String8 artist_name = album->artist;
						
						// NOTE(fakhri): artist button
						{
							V2_F32 artist_name_dim = font_compute_text_dim(artist_name_font, artist_name);
							
							V2_F32 button_dim = range_dim(cut3.top);
							button_dim.width = MIN(button_dim.width, artist_name_dim.width);
							
							V2_F32 button_pos = vec2(cut3.top.min_x + 0.5f * button_dim.width, range_center(cut3.top).y - 0.25f * artist_name_dim.height);
							
							Mplayer_UI_Interaction interaction = ui_widget_interaction(&mplayer->ui, range_center_dim(button_pos, button_dim), 0);
							
							if (interaction.hover)
							{
								should_underline = 1;
								text_color = vec4(0.6f, 0.6f, 0.6f, 1);
								
								underline_thickness = 2.0f;
							}
							
							if (interaction.pressed)
							{
								text_color = vec4(1, 1, 1, 1);
							}
							
							if (interaction.clicked)
							{
								assert(album->artist_id);
								mplayer->selected_artist_id = album->artist_id;
								mplayer_change_mode(mplayer, MODE_Artist_Albums, album->artist_id);
							}
						}
						
						draw_text_left_side_fixed_width(&group, artist_name_font, cut3.top, text_color, artist_name, should_underline, underline_thickness);
						
					}
					
				}
			}
			
			available_space = cut.bottom;
			render_group_set_cull_range(&group, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			// NOTE(fakhri): album tracks list
			{
				u32 index = ui_begin_list(&mplayer->ui, &mplayer->mode_stack->scroll, 
					vec2(available_space_dim.width, 50), available_space, album->tracks.count);
				
				
				u32 selected_track_id = 0;
				for (; index < album->tracks.count; index += 1)
				{
					Mplayer_Item_ID track_id = album->tracks.items[index];
					Mplayer_List_Option_Result list_opt = mplayer_track_list_option(&group, mplayer, track_id);
					if (!list_opt.handled) break;
					if (list_opt.selected)
					{
						selected_track_id = track_id;
					}
				}
				
				if (selected_track_id)
				{
					mplayer_play_music_track(mplayer, selected_track_id);
					current_music = mplayer_track_by_id(&mplayer->library, mplayer->current_music_id);
				}
			}
		} break;
		
		
		case MODE_Album_Library:
		{
			Range2_F32_Cut cut = range_cut_top(available_space, 65);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				
				String8 album_label = str8_lit("Albums");
				f32 album_label_width = font_compute_text_dim(&mplayer->header_label_font, album_label).width;
				Range2_F32_Cut cut2 = range_cut_left(header, 50); // padding
				cut2 = range_cut_left(cut2.right, album_label_width + 10);
				
				Range2_F32 label_rect = cut2.left;
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(album_label), Text_Render_Flag_Centered);
				
				// TODO(fakhri): display the number of albums and tracks the selected artist have
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_set_cull_range(&group, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			// NOTE(fakhri): library albums list
			{
				Mplayer_Item_ID first_album_id = ui_begin_list(&mplayer->ui, &mplayer->mode_stack->scroll, vec2(350, 300), available_space, mplayer->library.albums_count - 1, vec2(10, 10)) + 1;
				
				u32 selected_album_id = 0;
				for (u32 album_id = first_album_id; album_id < mplayer->library.albums_count; album_id += 1)
				{
					Mplayer_List_Option_Result list_opt = mplayer_album_list_option(&group, mplayer, album_id);
					if (!list_opt.handled) break;
					if (list_opt.selected)
					{
						selected_album_id = album_id;
					}
				}
				
				if (selected_album_id)
				{
					mplayer->selected_album_id = selected_album_id;
					mplayer_change_mode(mplayer, MODE_Album_Tracks, selected_album_id);
				}
			}
		} break;
		case MODE_Music_Library:
		{
			Range2_F32_Cut cut = range_cut_top(available_space, 65);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				
				String8 songs_label = str8_lit("Songs");
				f32 songs_label_width = font_compute_text_dim(&mplayer->header_label_font, songs_label).width;
				Range2_F32_Cut cut2 = range_cut_left(header, 50); // padding
				cut2 = range_cut_left(cut2.right, songs_label_width + 10);
				
				Range2_F32 label_rect = cut2.left;
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(songs_label), Text_Render_Flag_Centered);
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_set_cull_range(&group, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			// NOTE(fakhri): library tracks list
			{
				u32 first_track_id = ui_begin_list(&mplayer->ui, &mplayer->mode_stack->scroll, 
					vec2(available_space_dim.width, 50), available_space, mplayer->library.tracks_count - 1) + 1;
				
				u32 selected_track_id = 0;
				for (u32 track_id = first_track_id; track_id < mplayer->library.tracks_count; track_id += 1)
				{
					Mplayer_List_Option_Result list_opt = mplayer_track_list_option(&group, mplayer, track_id);
					if (!list_opt.handled) break;
					if (list_opt.selected)
					{
						selected_track_id = track_id;
					}
				}
				
				if (selected_track_id)
				{
					mplayer_play_music_track(mplayer, selected_track_id);
					current_music = mplayer_track_by_id(&mplayer->library, mplayer->current_music_id);
				}
			}
		} break;
		
		case MODE_Lyrics:
		{
			draw_text(&group, &mplayer->font, range_center(available_space), vec4(1, 1, 1, 1), fancy_str8_lit("Lyrics not available"), Text_Render_Flag_Centered);
		} break;
		
		case MODE_Settings:
		{
			Range2_F32_Cut cut = range_cut_top(available_space, 65);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				
				String8 settings_label = str8_lit("Settings");
				
				f32 settings_label_width = font_compute_text_dim(&mplayer->header_label_font, settings_label).width;
				Range2_F32_Cut cut2 = range_cut_left(header, 50); // padding
				cut2 = range_cut_left(cut2.right, settings_label_width + 10);
				
				Range2_F32 label_rect = cut2.left;
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(settings_label), Text_Render_Flag_Centered);
			}
			
			available_space = cut.bottom;
			
			// NOTE(fakhri): draw locations
			{
				Range2_F32_Cut cut2 = range_cut_top(available_space, 50);
				
				String8 library_location_str = str8_lit("Library Locations:");
				V2_F32 label_pos = vec2(cut2.top.min_x + 10, (cut2.top.max_y + cut2.top.min_y) / 2);
				draw_text(&group, &mplayer->font, label_pos, vec4(1, 1, 1, 1), fancy_str8(library_location_str));
				
				Range2_F32 add_location_button_region = range_cut_left(cut2.top, 20 + font_compute_text_dim(&mplayer->font, library_location_str).width).right;
				if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("+"), 
						vec2(10 + add_location_button_region.min_x, (5 + add_location_button_region.max_y + add_location_button_region.min_y) / 2)).clicked)
				{
					mplayer_open_path_lister(mplayer, &mplayer->path_lister);
				}
				
				// NOTE(fakhri): save button
				{
					Range2_F32 save_button_rect = range_cut_right(cut2.top, 100).right;
					
					if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Save"), 
							range_center(save_button_rect)).clicked)
					{
						mplayer_save_settings(mplayer);
					}
				}
				
				cut2 = range_cut_top(cut2.bottom, 200);
				available_space = cut2.bottom;
				
				cut2 = range_cut_left(cut2.top, 50);
				cut2 = range_cut_right(cut2.right, 50);
				
				Range2_F32 library_paths_region = cut2.left;
				V4_F32 bg_color = vec4(0.15f, 0.15f, 0.15f, 1.0f);
				push_rect(&group, range_center(library_paths_region), range_dim(library_paths_region), bg_color);
				
				Range2_F32 prev_cull_range = group.config.cull_range;
				render_group_add_cull_range(&group, library_paths_region);
				u32 location_to_delete = mplayer->settings.locations_count;
				
				for (u32 i = 0; i < mplayer->settings.locations_count; i += 1)
				{
					Mplayer_Library_Location *location = mplayer->settings.locations + i;
					
					Range2_F32_Cut location_cut = range_cut_top(library_paths_region, 40);
					Range2_F32 location_region = location_cut.top;
					library_paths_region = location_cut.bottom;
					
					V2_F32 location_region_center = range_center(location_region);
					V2_F32 location_region_dim = range_dim(location_region);
					V2_F32 location_pos = vec2(10 + location_region.min_x, location_region_center.y - 0.25f * (mplayer->font.ascent - mplayer->font.descent));
					
					draw_outline(&group, vec3(location_region_center, 0), location_region_dim, vec4(0, 0, 0, 1), 1.5f);
					draw_text(&group, &mplayer->font, location_pos, vec4(1, 1, 1, 1), fancy_str8(location->path, location->path_len));
					
					
					location_cut = range_cut_right(location_region, 40);
					Range2_F32 button_region = location_cut.right;
					if (_ui_button(&mplayer->ui, &mplayer->font, str8_lit("X"), range_center(button_region), __LINE__ * (i + 1) * 3912047291307).clicked)
					{
						location_to_delete = i;
					}
				}
				
				if (location_to_delete < mplayer->settings.locations_count)
				{
					for (u32 i = location_to_delete; i < mplayer->settings.locations_count - 1; i += 1)
					{
						memory_copy_struct(&mplayer->settings.locations[i], &mplayer->settings.locations[i + 1]);
					}
					
					mplayer->settings.locations_count -= 1;
				}
			}
			
			render_group_set_cull_range(&group, available_space);
			
			{
				Range2_F32_Cut cut2 = range_cut_top(available_space, 20);
				cut2 = range_cut_top(cut2.bottom, 30);
				
				available_space = cut2.bottom;
				
				Range2_F32 index_button_rect = cut2.top;
				
				if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("re-index library"), range_center(index_button_rect)).clicked)
				{
					mplayer->current_music_id = 0;
					mplayer_index_library(mplayer);
				}
			}
			
		} break;
	}
	
	
	// NOTE(fakhri): track control
	{
		render_group_set_cull_range(&group, track_control_rect);
		
		V4_F32 track_control_bg_color = vec4(0.05f, 0.05f, 0.05f, 1.0f);
		push_rect(&group, range_center(track_control_rect), range_dim(track_control_rect), track_control_bg_color);
		
		// NOTE(fakhri): padding
		Range2_F32_Cut cut = range_cut_top(track_control_rect, 10);
		
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
				
				Memory_Checkpoint scratch = get_scratch(0, 0);
				String8 timestamp_string = str8_f(scratch.arena, "%.2d:%.2d:%.2d", current_timestamp.hours, current_timestamp.minutes, current_timestamp.seconds);
				draw_text(&group, &mplayer->timestamp_font, range_center(timestamp_rect), vec4(1, 1, 1, 1), 
					fancy_str8(timestamp_string),
					Text_Render_Flag_Centered);
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
		
		// NOTE(fakhri): cover picture and track name
		{
			if (current_music)
			{
				Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, current_music->album_id);
				
				Range2_F32_Cut cut2 = range_cut_left(cut.left, 100);
				Range2_F32 cover_rect      = cut2.left;
				Range2_F32 track_name_rect = cut2.right;
				
				if (mplayer_is_valid_image(current_music->header.image))
				{
					mpalyer_draw_item_image(&group, mplayer, &current_music->header, range_center(cover_rect), range_dim(cover_rect), vec4(1,1,1,1));
				}
				else if (current_music->album_id && mplayer_is_valid_image(album->header.image))
				{
					mpalyer_draw_item_image(&group, mplayer, &album->header, range_center(cover_rect), range_dim(cover_rect), vec4(1,1,1,1));
				}
				
				// TODO(fakhri): stack of render configs?
				
				Range2_F32 prev_cull_range = group.config.cull_range;
				render_group_add_cull_range(&group, track_name_rect);
				
				Mplayer_UI_Interaction interaction = _ui_widget_interaction(&mplayer->ui, __LINE__, track_name_rect, 0);
				V4_F32 title_rect_color = track_control_bg_color;
				if (interaction.hover)
				{
					title_rect_color = vec4(0.1f, 0.1f, 0.1f, 1.0f);
				}
				
				if (interaction.pressed)
				{
					title_rect_color = vec4(0.0f, 0.f, 0.f, 1.0f);
				}
				
				if (interaction.clicked && current_music->album_id)
				{
					mplayer->selected_album_id = current_music->album_id;
					mplayer_change_mode(mplayer, MODE_Album_Tracks, mplayer->selected_album_id);
				}
				
				V2_F32 track_name_center = range_center(track_name_rect); 
				V2_F32 track_name_dim = range_dim(track_name_rect); 
				push_rect(&group, track_name_center, track_name_dim, title_rect_color);
				
				V2_F32 text_pos = vec2(5 + track_name_rect.min_x, track_name_center.y);
				f32 text_width = font_compute_text_dim(&mplayer->font, current_music->title).width;
				
				if (interaction.hover && text_width > track_name_dim.width)
				{
					mplayer->track_name_hover_t += mplayer->input.frame_dt;
					
					text_pos.x -= 10 * PI32 * mplayer->track_name_hover_t;
					draw_text(&group, &mplayer->font, text_pos, vec4(1, 1, 1, 1), fancy_str8(current_music->title));
					text_pos.x += text_width + 10;
				}
				else
				{
					mplayer->track_name_hover_t = 0;
				}
				
				draw_text(&group, &mplayer->font, text_pos, vec4(1, 1, 1, 1), fancy_str8(current_music->title));
				render_group_set_cull_range(&group, prev_cull_range);
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
	
	if (mplayer->show_path_popup)
	{
		mplayer->ui.disable_input = false;
		
		V2_F32 popup_center = vec2(0, 0);
		V2_F32 popup_dim    = 0.9f * render_ctx->draw_dim;
		
		Range2_F32 popup_rect = range_center_dim(popup_center, popup_dim);
		render_group_set_cull_range(&group, popup_rect);
		push_rect(&group, popup_rect, vec4(0.15f, 0.15f, 0.15f, 1.0f));
		draw_outline(&group, popup_rect, vec4(0, 0, 0, 1), 5);
		
		Range2_F32_Cut cut = range_cut_top(popup_rect, 50);
		
		Range2_F32 close_button_rect = range_cut_right(cut.top, 50).right;
		if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("X"), range_center(close_button_rect)).clicked)
		{
			mplayer_close_path_lister(mplayer, &mplayer->path_lister);
		}
		
		cut = range_cut_top(cut.bottom, 30);
		Range2_F32 input_field_rect = range_cut_right(range_cut_left(cut.top, 50).right, 50).left;
		push_rect(&group, input_field_rect, vec4(0.2f, 0.2f, 0.2f, 1.0f));
		draw_outline(&group, input_field_rect, vec4(0, 0, 0, 1), 1);
		
		if (ui_input_field(&mplayer->ui, &mplayer->font, range_center(input_field_rect), range_dim(input_field_rect), 
				&mplayer->path_lister.user_path, sizeof(mplayer->path_lister.user_path_buffer)).edited)
		{
			mplayer_refresh_path_lister_dir(&mplayer->path_lister);
		}
		
		cut = range_cut_top(cut.bottom, 20);
		cut = range_cut_bottom(cut.bottom, 100);
		Range2_F32 footer_rect = cut.bottom;
		cut = range_cut_left(cut.top, 50);
		cut = range_cut_right(cut.right, 50);
		
		Range2_F32 options_rect = cut.left;
		V2_F32 options_rect_center = range_center(options_rect);
		V2_F32 options_rect_dim = range_dim(options_rect);
		
		push_rect(&group, options_rect, vec4(0.2f, 0.2f, 0.2f, 1.0f));
		render_group_add_cull_range(&group, options_rect);
		
		// TODO(fakhri): scroll
		
		u32 selected_option_index = mplayer->path_lister.filtered_paths_count;
		V2_F32 option_dim = vec2(options_rect_dim.width, 40);
		V2_F32 option_pos = options_rect_center;
		option_pos.y += 0.5f * (options_rect_dim.height - option_dim.height);
		
		for (u32 i = 0; i < mplayer->path_lister.filtered_paths_count; i += 1)
		{
			Range2_F32 option_rect = range_center_dim(option_pos, option_dim);
			Range2_F32 visible_rect = range_intersection(option_rect, options_rect);
			
			if (!is_range_intersect(option_rect, options_rect))
			{
				break;
			}
			
			String8 option_path = mplayer->path_lister.filtered_paths[i];
			
			V4_F32 bg_color = vec4(0.25f, 0.25f,0.25f, 1);
			Mplayer_UI_Interaction interaction = _ui_widget_interaction(&mplayer->ui, 
				int_from_ptr(mplayer->path_lister.filtered_paths + i), 
				visible_rect, 0);
			
			if (interaction.hover)
			{
				bg_color = vec4(0.2f, 0.2f,0.2f, 1);
			}
			
			if (interaction.pressed)
			{
				bg_color = vec4(0.12f, 0.12f,0.12f, 1);
				if (interaction.hover)
				{
					bg_color = vec4(0.1f, 0.1f,0.1f, 1);
				}
				selected_option_index = i;
			}
			
			
			push_rect(&group, option_pos, option_dim, bg_color);
			draw_outline(&group, option_pos, option_dim, vec4(0, 0, 0, 1), 1);
			draw_text(&group, &mplayer->font, vec2(option_pos.x - 0.5f * option_dim.width + 20, option_pos.y), vec4(1, 1, 1, 1), fancy_str8(option_path));
			
			option_pos.y -= option_dim.height;
		}
		
		if (selected_option_index < mplayer->path_lister.filtered_paths_count)
		{
			mplayer_set_path_lister_path(&mplayer->path_lister, mplayer->path_lister.filtered_paths[selected_option_index]);
		}
		
		render_group_set_cull_range(&group, popup_rect);
		if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Ok"), range_center(footer_rect)).clicked)
		{
			if (mplayer->settings.locations_count < MAX_LOCATION_COUNT)
			{
				Mplayer_Library_Location *location = mplayer->settings.locations + mplayer->settings.locations_count++;
				memory_copy(location->path, mplayer->path_lister.user_path.str, mplayer->path_lister.user_path.len);
				location->path_len = u32(mplayer->path_lister.user_path.len);
			}
			
			mplayer_close_path_lister(mplayer, &mplayer->path_lister);
		}
	}
	#endif
		
		// NOTE(fakhri): draw fps
	{
		render_group_set_cull_range(&group, screen_rect);
		V2_F32 fps_pos = 0.5 * render_ctx->draw_dim;
		fps_pos.x -= 20;
		fps_pos.y -= 15;
		Memory_Checkpoint scratch = get_scratch(0, 0);
		draw_text(&group, &mplayer->debug_font, fps_pos, vec4(0.5f, 0.6f, 0.6f, 1), 
			fancy_str8(str8_f(scratch.arena, "%d", u32(1.0f / mplayer->input.frame_dt))),
			Text_Render_Flag_Centered);
	}
	
}
