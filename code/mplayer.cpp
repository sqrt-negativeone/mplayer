
#include "mplayer.h"

#define STBI_ASSERT(x) assert(x)
#define STBI_SSE2
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"


#include "mplayer_renderer.cpp"
#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

#include "mplayer_byte_stream.h"


//~ NOTE(fakhri): string hashing stuff
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


//~ NOTE(fakhri): Tracks, Albums and Artists creation, and ID system stuff

internal b32
is_valid(Mplayer_Image_ID id)
{
	b32 result = id.index != 0;
	return result;
}

internal Mplayer_Image_ID
mplayer_reserve_image_id(Mplayer_Library *library)
{
	assert(library->images_count < array_count(library->images));
	Mplayer_Image_ID result = ZERO_STRUCT;
	result.index = library->images_count++;
	return result;
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
	
	memory_zero_struct(track);
	track->header.id = id;
	
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
	memory_zero_struct(album);
	album->header.id = id;
	
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
	memory_zero_struct(artist);
	artist->header.id = id;
	return artist;
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
		result->header.image_id = mplayer_reserve_image_id(library);
		
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


//~ NOTE(fakhri): Mplayer Image stuff

internal Decode_Texture_Data_Input *
mplayer_make_texure_upload_data(Mplayer_Context *mplayer, Mplayer_Item_Image *image)
{
	Decode_Texture_Data_Input *result = m_arena_bootstrap_push(Decode_Texture_Data_Input, work_arena);
	
	assert(result);
	result->mplayer = mplayer;
	result->image   = image;
	
	return result;
}

WORK_SIG(decode_texture_data_work)
{
	assert(input);
	Decode_Texture_Data_Input *upload_data = (Decode_Texture_Data_Input *)input;
	
	Mplayer_Context *mplayer = upload_data->mplayer;
	Mplayer_Item_Image *image = upload_data->image;
	Render_Context *render_ctx = mplayer->render_ctx;
	Textures_Upload_Buffer *upload_buffer = &render_ctx->upload_buffer;
	
	if (image->state == Image_State_Loading)
	{
		Buffer texture_data = image->texture_data;
		if (image->in_disk)
			texture_data = platform->load_entire_file(image->path, &upload_data->work_arena);
		
		if (is_valid(texture_data))
		{
			int req_channels = 4;
			int width, height, channels;
			u8 *pixels = stbi_load_from_memory(texture_data.data, int(texture_data.size), &width, &height, &channels, req_channels);
			assert(pixels);
			if (pixels)
			{
				image->texture.width  = u16(width);
				image->texture.height = u16(height);
				
				Buffer pixels_buf = make_buffer(pixels, width * height * req_channels); 
				push_texture_upload_request(upload_buffer, &image->texture, pixels_buf, 1);
			}
			
			CompletePreviousWritesBeforeFutureWrites();
			image->state = Image_State_Loaded;
		}
		else
		{
			image->state = Image_State_Invalid;
		}
	}
	
	Memory_Arena arena = upload_data->work_arena;
	m_arena_free_all(&arena);
}

internal b32
mplayer_is_valid_image(Mplayer_Item_Image image)
{
	b32 result = (image.in_disk && image.path.len) || is_valid(image.texture_data);
	return result;
}

internal b32
mplayer_item_image_ready(Mplayer_Item_Image image)
{
	b32 result = image.state == Image_State_Loaded;
	return result;
}

internal void
mplayer_load_item_image(Mplayer_Context *mplayer, Mplayer_Item_Image *image)
{
	if (!is_texture_valid(image->texture))
	{
		image->texture = reserve_texture_handle(mplayer->render_ctx);
	}
	
	if (image->state == Image_State_Unloaded)
	{
		image->state = Image_State_Loading;
		
		platform->push_work(decode_texture_data_work, 
			mplayer_make_texure_upload_data(mplayer, image));
	}
}

internal Mplayer_Item_Image *
mplayer_get_image_by_id(Mplayer_Context *mplayer, Mplayer_Image_ID image_id, b32 load)
{
	assert(image_id.index < array_count(mplayer->library.images));
	Mplayer_Item_Image *image = 0;
	
	image = mplayer->library.images + image_id.index;
	if (load)
	{
		mplayer_load_item_image(mplayer, image);
	}
	
	return image;
}

//~ NOTE(fakhri): track control stuff

internal void
mplayer_music_reset(Mplayer_Track *music_track)
{
	if (music_track && music_track->flac_stream)
	{
		flac_seek_stream(music_track->flac_stream, 0);
	}
}

internal void
mplayer_load_music_track(Mplayer_Context *mplayer, Mplayer_Track *music_track)
{
	if (!music_track) return;
	
	if (!music_track->file_loaded)
	{
		music_track->flac_file_buffer = mplayer->os.load_entire_file(music_track->path, &mplayer->library.track_transient_arena);
		music_track->file_loaded = true;
	}
	
	if (!music_track->flac_stream)
	{
		music_track->flac_stream = m_arena_push_struct_z(&mplayer->library.track_transient_arena, Flac_Stream);
		init_flac_stream(music_track->flac_stream, &mplayer->library.track_transient_arena, music_track->flac_file_buffer);
		if (!music_track->flac_stream->seek_table)
		{
			flac_build_seek_table(music_track->flac_stream, &mplayer->library.track_transient_arena, &music_track->build_seektable_work_data);
		}
		
		if (!is_valid(music_track->header.image_id))
		{
			Flac_Picture *front_cover = music_track->flac_stream->front_cover;
			if (front_cover)
			{
				music_track->header.image_id = mplayer_reserve_image_id(&mplayer->library);
				Mplayer_Item_Image *cover_image = mplayer_get_image_by_id(mplayer, music_track->header.image_id, 0);
				
				cover_image->in_disk = false;
				cover_image->texture_data = clone_buffer(&mplayer->library.arena, front_cover->buffer);
			}
			else
			{
				Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, music_track->album_id);
				if (album)
				{
					music_track->header.image_id = album->header.image_id;
				}
			}
		}
	}
}

internal void
mplayer_unload_music_track(Mplayer_Context *mplayer, Mplayer_Track *music)
{
	if (!music) return;
	
	music->build_seektable_work_data.cancel_req = 1;
	for (;music->build_seektable_work_data.running;)
	{
		// NOTE(fakhri): help worker threads instead of just waiting
		platform->do_next_work();
	}
	uninit_flac_stream(music->flac_stream);
	m_arena_free_all(&mplayer->library.track_transient_arena);
	music->file_loaded = false;
	music->flac_stream = 0;
	music->flac_file_buffer = ZERO_STRUCT;
}

internal b32
is_music_track_still_playing(Mplayer_Track *music_track)
{
	b32 result = (music_track->flac_stream && 
		!bitstream_is_empty(&music_track->flac_stream->bitstream));
	
	return result;
}

//~ NOTE(fakhri): Font stuff
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
	push_texture_upload_request(&mplayer->render_ctx->upload_buffer, &font->atlas_tex, pixels_buf, 0);
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

//~ NOTE(fakhri): Fancy string8 stuff
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


//~ NOTE(fakhri): Mplayer Draw stuff

enum Text_Render_Flags
{
	Text_Render_Flag_Centered    = (1 << 0),
	Text_Render_Flag_Limit_Width = (1 << 1),
	Text_Render_Flag_Underline   = (1 << 2),
};

internal V3_F32
draw_text(Render_Group *group, Mplayer_Font *font, V3_F32 pos, V4_F32 color, Fancy_String8 string, u32 flags = 0)
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
	V3_F32 text_pt = pos;
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
		V3_F32 glyph_p = vec3(text_pt.xy + (glyph.offset), text_pt.z);
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
		draw_text(group, font, text_pt, color, fancy_str8_lit("..."));
	}
	
	return text_pt;
}

internal V3_F32
draw_text(Render_Group *group, Mplayer_Font *font, V2_F32 pos, V4_F32 color, Fancy_String8 string, u32 flags = 0)
{
	V3_F32 result = draw_text(group, font, vec3(pos, 0), color, string, flags);
	return result;
}

internal V3_F32
draw_text(Render_Group *group, Mplayer_Font *font, V2_F32 pos, f32 z, V4_F32 color, Fancy_String8 string, u32 flags = 0)
{
	V3_F32 result = draw_text(group, font, vec3(pos, z), color, string, flags);
	return result;
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
draw_outline(Render_Group *group, V2_F32 pos, f32 z, V2_F32 dim, V4_F32 color, f32 thickness)
{
	draw_outline(group, vec3(pos, z), dim, color, thickness);
}

internal void
draw_outline(Render_Group *group, Range2_F32 rect, f32 z, V4_F32 color, f32 thickness)
{
	draw_outline(group, range_center(rect), z, range_dim(rect), color, thickness);
}

#include "mplayer_ui.cpp"

//~ NOTE(fakhri): Queue stuff
#define NULL_QUEUE_INDEX {}
internal b32
is_valid(Mplayer_Context *mplayer, Mplayer_Queue_Index index)
{
	b32 result = (index && (index < mplayer->queue.count) && 
		mplayer->queue.tracks[index] && (mplayer->queue.tracks[index] < mplayer->library.tracks_count));
	return result;
}

internal void
mplayer_queue_shuffle(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	Mplayer_Queue_Index current_new_index = queue->current_index;
	for (Mplayer_Queue_Index i = 1; i < queue->count; i += 1)
	{
		u16 swap_index = (u16)rng_next_minmax(&mplayer->entropy, i, queue->count);
		SWAP(Mplayer_Item_ID, queue->tracks[i], queue->tracks[swap_index]);
		
		// TODO(fakhri): can we afford to just do another loop to look for
		// the new index of the current track playing track?
		if (swap_index == current_new_index)
		{
			current_new_index = i;
		}
		else if (i == current_new_index)
		{
			current_new_index = swap_index;
		}
	}
	queue->current_index = current_new_index;
}

internal Mplayer_Track *
mplayer_queue_get_track_from_queue_index(Mplayer_Context *mplayer, Mplayer_Queue_Index index)
{
	assert(index < mplayer->queue.count);
	Mplayer_Queue *queue = &mplayer->queue;
	Mplayer_Track *result = mplayer_track_by_id(&mplayer->library, queue->tracks[index]);
	return result;
}

internal Mplayer_Track *
mplayer_queue_get_current_track(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	
	Mplayer_Track *result = 0;
	if (is_valid(mplayer, queue->current_index))
	{
		result = mplayer_queue_get_track_from_queue_index(mplayer, queue->current_index);
	}
	return result;
}

internal void
mplayer_set_current(Mplayer_Context *mplayer, Mplayer_Queue_Index index)
{
	Mplayer_Queue *queue = &mplayer->queue;
	
	if (is_valid(mplayer, queue->current_index))
	{
		mplayer_unload_music_track(mplayer, 
			mplayer_queue_get_current_track(mplayer));
	}
	
	queue->current_index = CLAMP(0, index, queue->count - 1);
	
	if (is_valid(mplayer, index))
	{
		mplayer_load_music_track(mplayer, 
			mplayer_track_by_id(&mplayer->library, queue->tracks[index]));
	}
}

internal void
mplayer_clear_queue(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	
	queue->tracks[0] = 0;
	queue->playing = 0;
	queue->count = 1;
	mplayer_set_current(mplayer, NULL_QUEUE_INDEX);
}

internal void
mplayer_init_queue(Mplayer_Context *mplayer)
{
	memory_zero_struct(&mplayer->queue);
	mplayer_clear_queue(mplayer);
}

internal void
mplayer_play_next_in_queue(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	
	mplayer_set_current(mplayer, queue->current_index + 1);
}

internal void
mplayer_play_prev_in_queue(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	mplayer_set_current(mplayer, queue->current_index - 1);
}

internal void
mplayer_update_queue(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	if (!is_valid(mplayer, queue->current_index))
	{
		mplayer_set_current(mplayer, 1);
	}
	
	if (queue->playing)
	{
		Mplayer_Track *current_track = mplayer_queue_get_current_track(mplayer);
		if (current_track && !is_music_track_still_playing(current_track))
		{
			mplayer_play_next_in_queue(mplayer);
		}
	}
}

internal void
mplayer_queue_pause(Mplayer_Context *mplayer)
{
	mplayer->queue.playing = 0;
}

internal void
mplayer_queue_resume(Mplayer_Context *mplayer)
{
	mplayer->queue.playing = 1;
}

internal void
mplayer_queue_toggle_play(Mplayer_Context *mplayer)
{
	if (mplayer->queue.playing)
	{
		mplayer_queue_pause(mplayer);
	}
	else 
	{
		mplayer_queue_resume(mplayer);
	}
}

internal b32
mplayer_is_queue_playing(Mplayer_Context *mplayer)
{
	b32 result = mplayer->queue.playing;
	return result;
}

internal void
mplayer_queue_insert_at(Mplayer_Context *mplayer, Mplayer_Queue_Index index, Mplayer_Item_ID track_id)
{
	Mplayer_Queue *queue = &mplayer->queue;
	
	if (queue->count < array_count(queue->tracks) && index <= queue->count)
	{
		if (index + 1 < queue->count)
		{
			memory_move(queue->tracks + index + 1, 
				queue->tracks + index,
				sizeof(queue->tracks[0]) * (queue->count - index));
		}
		
		queue->tracks[index] = track_id;
		queue->count += 1;
	}
}

internal void
mplayer_queue_next(Mplayer_Context *mplayer, Mplayer_Item_ID track_id)
{
	if (!track_id) return;
	mplayer_queue_insert_at(mplayer, mplayer->queue.current_index + 1, track_id);
}

internal void
mplayer_queue_last(Mplayer_Context *mplayer, Mplayer_Item_ID track_id)
{
	if (!track_id) return;
	mplayer_queue_insert_at(mplayer, mplayer->queue.count, track_id);
}

internal void
mplayer_empty_queue_after_current(Mplayer_Context *mplayer)
{
	Mplayer_Queue *queue = &mplayer->queue;
	
	if (queue->count > queue->current_index)
	{
		queue->count = queue->current_index + 1;
	}
}

internal void
mplayer_queue_play_track(Mplayer_Context *mplayer, Mplayer_Item_ID track_id)
{
	mplayer_queue_next(mplayer, track_id);
	mplayer_play_next_in_queue(mplayer);
}

internal Mplayer_Item_ID
mplayer_queue_current_track_id(Mplayer_Context *mplayer)
{
	Mplayer_Item_ID result = 0;
	if (is_valid(mplayer, mplayer->queue.current_index))
	{
		result = mplayer->queue.tracks[mplayer->queue.current_index];
	}
	return result;
}


//~ NOTE(fakhri): Library Indexing, Serializing and other stuff
internal void
mplayer_reset_library(Mplayer_Context *mplayer)
{
	Mplayer_Library *library = &mplayer->library;
	mplayer_clear_queue(mplayer);
	
	// NOTE(fakhri): wait for worker threads to end
	for (;platform->do_next_work();) {}
	m_arena_free_all(&library->arena);
	
	// NOTE(fakhri): reserve null ids
	library->artists_count = 1;
	library->albums_count = 1;
	library->tracks_count = 1;
	for (u32 i = 1; i < library->images_count; i += 1)
	{
		Mplayer_Item_Image *image = library->images + i;
		
		push_texture_release_request(&mplayer->render_ctx->release_buffer, image->texture);
		memory_zero_struct(image);
	}
	
	library->images_count = 1;
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
			
			Mplayer_Item_Image *image = mplayer_get_image_by_id(mplayer, album->header.image_id, 0);
			mplayer_serialize(index_file, *image);
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
	mplayer_reset_library(mplayer);
	
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
				u32 tracks_count;
				u32 albums_count;
				u32 artists_count;
				byte_stream_read(&bs, &tracks_count);
				byte_stream_read(&bs, &albums_count);
				byte_stream_read(&bs, &artists_count);
				
				
				for (u32 artist_id = 1; artist_id < artists_count; artist_id += 1)
				{
					Mplayer_Artist *artist = mplayer_make_artist(&mplayer->library);
					
					byte_stream_read(&bs, &artist->name);
					
					byte_stream_read(&bs, &artist->tracks);
					byte_stream_read(&bs, &artist->albums);
				}
				
				for (u32 album_id = 1; album_id < albums_count; album_id += 1)
				{
					Mplayer_Album *album = mplayer_make_album(&mplayer->library);
					album->header.image_id = mplayer_reserve_image_id(&mplayer->library);
					
					byte_stream_read(&bs, &album->name);
					byte_stream_read(&bs, &album->artist);
					byte_stream_read(&bs, &album->artist_id);
					byte_stream_read(&bs, &album->tracks);
					
					Mplayer_Item_Image *image = mplayer_get_image_by_id(mplayer, album->header.image_id, 0);
					byte_stream_read(&bs, image);
					if (image->in_disk)
					{
						image->path = str8_clone(&mplayer->library.arena, image->path);
					}
					else
					{
						image->texture_data = clone_buffer(&mplayer->library.arena, image->texture_data);
					}
				}
				
				for (u32 track_id = 1; track_id < tracks_count; track_id += 1)
				{
					Mplayer_Track *track = mplayer_make_track(&mplayer->library);
					
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
mplayer_load_tracks_in_directory(Mplayer_Context *mplayer, String8 library_path)
{
	Memory_Checkpoint temp_mem = m_checkpoint_begin(&mplayer->frame_arena);
	Directory dir = platform->read_directory(temp_mem.arena, library_path);
	for (u32 info_index = 0; info_index < dir.count; info_index += 1)
	{
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
			Memory_Checkpoint scratch = get_scratch(0, 0);
			if (str8_ends_with(info.name, str8_lit(".flac"), MatchFlag_CaseInsensitive))
			{
				Buffer tmp_file_block = arena_push_buffer(scratch.arena, megabytes(1));
				File_Handle *file = platform->open_file(info.path, File_Open_Read);
				Flac_Stream tmp_flac_stream = ZERO_STRUCT;
				assert(file);
				if (file)
				{
					// TODO(fakhri): make sure the track is not already indexed
					Mplayer_Track *music_track = mplayer_make_track(&mplayer->library);
					music_track->path = str8_clone(&mplayer->library.arena, info.path);
					
					Buffer buffer = platform->read_block(file, tmp_file_block.data, tmp_file_block.size);
					platform->close_file(file);
					// NOTE(fakhri): init flac stream
					{
						tmp_flac_stream.bitstream.buffer = buffer;
						tmp_flac_stream.bitstream.pos.byte_index = 0;
						tmp_flac_stream.bitstream.pos.bits_left  = 8;
					}
					flac_process_metadata(&tmp_flac_stream, scratch.arena);
					
					Flac_Picture *front_cover = tmp_flac_stream.front_cover;
					
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
					
					Mplayer_Item_Image *image = mplayer_get_image_by_id(mplayer, album->header.image_id, 0);
					
					if (!image->in_disk && !image->texture_data.size)
					{
						String8 cover_file_path = mplayer_attempt_find_cover_image_in_dir(mplayer, &mplayer->library.arena, dir);
						if (cover_file_path.len)
						{
							image->in_disk = true;
							image->path = cover_file_path;
						}
						else if (front_cover)
						{
							image->in_disk = false;
							image->texture_data = clone_buffer(&mplayer->library.arena, front_cover->buffer);
						}
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
	
	mplayer_reset_library(mplayer);
	
	for (u32 i = 0; i < mplayer->settings.locations_count; i += 1)
	{
		Mplayer_Library_Location *location = mplayer->settings.locations + i;
		mplayer_load_tracks_in_directory(mplayer, str8(location->path, location->path_len));
	}
	
	for (u32 artist_id = 1; artist_id < library->artists_count; artist_id += 1)
	{
		Mplayer_Artist *artist = mplayer_artist_by_id(library, artist_id);
		
		artist->tracks.items = m_arena_push_array(&library->arena, Mplayer_Item_ID, artist->tracks.count);
		artist->tracks.count = 0;
		
		artist->albums.items = m_arena_push_array(&library->arena, Mplayer_Item_ID, artist->albums.count);
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
		
		Mplayer_Album *album = mplayer_album_by_id(library, track->album_id);
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

//~ NOTE(fakhri): Mode stack stuff

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
			
			case MODE_Lyrics:  fallthrough;
			case MODE_Settings:fallthrough;
			case MODE_Queue:   fallthrough;
			default: break;
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
			
			case MODE_Lyrics:  fallthrough;
			case MODE_Settings:fallthrough;
			case MODE_Queue:   fallthrough;
			default: break;
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
}


//~ NOTE(fakhri): Settings stuff

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

//~ NOTE(fakhri): Timestamp stuff
struct Mplayer_Timestamp
{
	u8 hours;
	u8 minutes;
	u8 seconds;
};


internal Mplayer_Timestamp
flac_get_current_timestap(u64 current_sample, u64 sample_rate)
{
	Mplayer_Timestamp result = ZERO_STRUCT;
	u64 seconds_elapsed = current_sample / sample_rate;
	u64 hours = seconds_elapsed / 3600;
	seconds_elapsed %= 3600;
	
	u64 minutes = seconds_elapsed / 60;
	seconds_elapsed %= 60;
	
	result.hours   = u8(hours);
	result.minutes = u8(minutes);
	result.seconds = u8(seconds_elapsed);
	
	return result;
}


//~ NOTE(fakhri): Path lister stuff
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
	mplayer->show_path_modal = true;
}

internal void
mplayer_close_path_lister(Mplayer_Context *mplayer, Mplayer_Path_Lister *path_lister)
{
	m_arena_free_all(&path_lister->arena);
	path_lister->filtered_paths_count = 0;
	path_lister->base_dir = ZERO_STRUCT;
	mplayer->show_path_modal = false;
	
}

//~ NOTE(fakhri): Custom UI stuff
internal Mplayer_UI_Interaction
mplayer_ui_underlined_button(Mplayer_UI *ui, String8 string)
{
	ui_next_hover_cursor(ui, Cursor_Hand);
	UI_Element *button = ui_element(ui, string, UI_FLAG_Draw_Text | UI_FLAG_Clickable);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, button);
	if (interaction.hover) ui_parent(ui, button)
	{
		button->child_layout_axis = Axis2_Y;
		ui_spacer(ui, ui_size_percent(0.85f, 1));
		
		ui_next_width(ui, ui_size_percent(1, 1));
		ui_next_height(ui, ui_size_pixel(1, 1));
		ui_next_background_color(ui, button->text_color);
		ui_element(ui, {}, UI_FLAG_Draw_Background);
	}
	return interaction;
}

internal Mplayer_UI_Interaction
mplayer_ui_underlined_button_f(Mplayer_UI *ui, const char *fmt, ...)
{
	va_list args;
  va_start(args, fmt);
	String8 string = str8_fv(ui->frame_arena, fmt, args);
	va_end(args);
	
	return mplayer_ui_underlined_button(ui, string);
}

internal void
mplayer_ui_track_item(Mplayer_UI *ui, Mplayer_Context *mplayer, Mplayer_Item_ID track_id)
{
	Mplayer_Track *track = mplayer_track_by_id(&mplayer->library, track_id);
	UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
	
	V4_F32 bg_color = vec4(0.15f, 0.15f,0.15f, 1);
	if (track_id == mplayer_queue_current_track_id(mplayer))
	{
		bg_color = vec4(0, 0, 0, 1);
	}
	
	ui_next_background_color(ui, bg_color);
	ui_next_border_color(ui, vec4(0, 0, 0, 1));
	ui_next_border_thickness(ui, 1);
	ui_next_hover_cursor(ui, Cursor_Hand);
	UI_Element *track_el = ui_element_f(ui, flags, "library_track_%p", track);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, track_el);
	if (interaction.clicked)
	{
		mplayer_queue_play_track(mplayer, track_id);
		mplayer_queue_resume(mplayer);
	}
	
	ui_parent(ui, track_el) ui_vertical_layout(ui) ui_padding(ui, ui_size_parent_remaining())
		ui_horizontal_layout(ui)
	{
		ui_spacer_pixels(ui, 10, 1);
		
		ui_next_width(ui, ui_size_text_dim(1));
		ui_next_height(ui, ui_size_text_dim(1));
		ui_element_f(ui, UI_FLAG_Draw_Text, "%.*s##library_track_name_%p", 
			STR8_EXPAND(track->title), track);
	}
}


internal void
mplayer_ui_album_item(Mplayer_UI *ui, Mplayer_Context *mplayer, Mplayer_Item_ID album_id)
{
	Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, album_id);
	
	Mplayer_Item_Image *image = mplayer_get_image_by_id(mplayer, album->header.image_id, 1);
	
	UI_Element_Flags flags = UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim | UI_FLAG_Clickable | UI_FLAG_Clip;
	b32 image_ready = mplayer_item_image_ready(*image);
	if (image_ready)
	{
		flags |= UI_FLAG_Draw_Image;
		ui_next_texture_tint_color(ui, vec4(1, 1, 1, 0.35f));
		ui_next_texture(ui, image->texture);
	}
	else
	{
		flags |= UI_FLAG_Draw_Background;
		ui_next_background_color(ui, vec4(1, 1, 1, 0.35f));
	}
	
	ui_next_hover_cursor(ui, Cursor_Hand);
	ui_next_roundness(ui, 10);
	UI_Element *album_el = ui_element_f(ui, flags, "library_album_%p", album);
	album_el->child_layout_axis = Axis2_Y;
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, album_el);
	if (interaction.clicked)
	{
		mplayer->selected_album_id = album_id;
		mplayer_change_mode(mplayer, MODE_Album_Tracks, album_id);
	}
	
	if (interaction.hover && image_ready)
	{
		album_el->texture_tint_color = vec4(1, 1, 1, 0.9f);
	}
	
	ui_parent(ui, album_el)
	{
		ui_next_width(ui, ui_size_percent(1, 1));
		ui_next_height(ui, ui_size_pixel(50, 1));
		ui_next_background_color(ui, vec4(0.3f, 0.3f, 0.3f, 0.3f));
		ui_next_roundness(ui, 5);
		ui_element_f(ui, UI_FLAG_Draw_Background|UI_FLAG_Draw_Text | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim, "%.*s##library_album_name_%p", 
			STR8_EXPAND(album->name), album);
	}
}

internal void
mplayer_ui_aritst_item(Mplayer_UI *ui, Mplayer_Context *mplayer, Mplayer_Item_ID artist_id)
{
	Mplayer_Artist *artist = mplayer_artist_by_id(&mplayer->library, artist_id);
	Mplayer_Item_Image *image = mplayer_get_image_by_id(mplayer, artist->header.image_id, 1);
	
	UI_Element_Flags flags = 0;
	b32 image_ready = mplayer_item_image_ready(*image);
	if (image_ready)
	{
		flags |= UI_FLAG_Draw_Image;
		ui_next_texture_tint_color(ui, vec4(1, 1, 1, 0.35f));
		ui_next_texture(ui, image->texture);
	}
	else
	{
		flags |= UI_FLAG_Draw_Background;
		ui_next_background_color(ui, vec4(1, 1, 1, 0.35f));
	}
	
	ui_next_hover_cursor(ui, Cursor_Hand);
	ui_next_roundness(ui, 10);
	ui_next_width(ui, ui_size_percent(1, 1));
	ui_next_height(ui, ui_size_percent(1, 1));
	flags |= UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim | UI_FLAG_Clickable | UI_FLAG_Clip;
	UI_Element *artist_el = ui_element_f(ui, flags, "library_artist_%p", artist);
	artist_el->child_layout_axis = Axis2_Y;
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, artist_el);
	if (interaction.clicked)
	{
		mplayer->selected_artist_id = artist_id;
		mplayer_change_mode(mplayer, MODE_Artist_Albums, artist_id);
	}
	if (interaction.hover && image_ready)
	{
		artist_el->texture_tint_color = vec4(1, 1, 1, 1);
	}
	
	ui_parent(ui, artist_el)
	{
		ui_spacer_pixels(ui, 10, 1);
		
		ui_next_width(ui, ui_size_percent(1, 1));
		ui_next_height(ui, ui_size_text_dim(1));
		ui_element_f(ui, UI_FLAG_Draw_Text | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim, "%.*s##library_artist_name_%p", 
			STR8_EXPAND(artist->name), artist);
	}
}



//~ NOTE(fakhri): Mplayer Exported API
exported void
mplayer_get_audio_samples(Sound_Config device_config, Mplayer_Context *mplayer, void *output_buf, u32 frame_count)
{
	Mplayer_Track *music = mplayer_queue_get_current_track(mplayer);
	if (music && mplayer_is_queue_playing(mplayer))
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

exported void
mplayer_initialize(Mplayer_Context *mplayer)
{
	mplayer->entropy = rng_make_linear(1249817);
	
	load_font(mplayer, &mplayer->font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->debug_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->big_debug_font, str8_lit("data/fonts/arial.ttf"), 50);
	load_font(mplayer, &mplayer->timestamp_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->header_label_font, str8_lit("data/fonts/arial.ttf"), 50);
	load_font(mplayer, &mplayer->small_font, str8_lit("data/fonts/arial.ttf"), 15);
	
	mplayer->volume = 1.0f;
	
	mplayer_init_queue(mplayer);
	mplayer_load_settings(mplayer);
	mplayer_load_library(mplayer);
	mplayer_change_mode(mplayer, MODE_Music_Library, 0);
	
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
	
	mplayer_update_queue(mplayer);
	
	if (mplayer_mouse_button_clicked(&mplayer->input, Mouse_M4))
	{
		mplayer_change_previous_mode(mplayer);
	}
	
	if (mplayer_mouse_button_clicked(&mplayer->input, Mouse_M5))
	{
		mplayer_change_next_mode(mplayer);
	}
	
	Mplayer_UI *ui = &mplayer->ui;;
	ui_begin(ui, mplayer, &group, world_mouse_p);
	
	if (mplayer->show_path_modal)
	{
		// TODO(fakhri): draw locations path selector
		
		//ui_pref_width(ui, ui_size_by_childs(1))ui_pref_height(ui, ui_size_by_childs(1))
		_defer_loop(ui_modal_begin(ui, str8_lit("path-lister-modal")), ui_modal_end(ui))
			ui_vertical_layout(ui) ui_padding(ui, ui_size_parent_remaining())
			ui_pref_height(ui, ui_size_by_childs(1)) ui_horizontal_layout(ui) ui_padding(ui, ui_size_parent_remaining())
		{
			ui_next_border_color(ui, vec4(0,0,0,1));
			ui_next_border_thickness(ui, 2);
			ui_next_background_color(ui, vec4(0.15f, 0.15f, 0.15f, 1.0f));
			ui_next_width(ui, ui_size_percent(0.9f, 1));
			ui_next_height(ui, ui_size_percent(0.9f, 1));
			ui_next_flags(ui, UI_FLAG_Clip);
			ui_vertical_layout(ui) ui_padding(ui, ui_size_pixel(30, 1))
			{
				ui_next_height(ui, ui_size_pixel(30, 1));
				ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(20, 1))
					ui_pref_height(ui, ui_size_percent(1, 1))
				{
					ui_next_width(ui, ui_size_text_dim(1));
					ui_label(ui, str8_lit("Input path:"));
					
					ui_spacer_pixels(ui, 10, 1);
					
					ui_next_border_color(ui, vec4(0,0,0,1));
					ui_next_border_thickness(ui, 2);
					ui_next_background_color(ui, vec4(0.2f, 0.2f, 0.2f, 1.0f));
					if (ui_input_field(ui, str8_lit("path-input-field"), &mplayer->path_lister.user_path, sizeof(mplayer->path_lister.user_path_buffer)))
					{
						mplayer_refresh_path_lister_dir(&mplayer->path_lister);
					}
					
				}
				ui_spacer_pixels(ui, 10, 1);
				
				ui_next_border_thickness(ui, 2);
				ui_next_height(ui, ui_size_by_childs(1));
				ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 0))
				{
					u32 selected_option_index = mplayer->path_lister.filtered_paths_count;
					
					ui_next_height(ui, ui_size_percent(0.75f, 0));
					ui_for_each_list_item(ui, str8_lit("path-lister-options"), mplayer->path_lister.filtered_paths_count, 40.0f, 1.0f, option_index)
						ui_pref_height(ui, ui_size_percent(1,0)) ui_pref_width(ui, ui_size_percent(1, 0))
					{
						String8 option_path = mplayer->path_lister.filtered_paths[option_index];
						
						UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
						
						ui_next_roundness(ui, 5);
						ui_next_background_color(ui, vec4(0.25f, 0.25f,0.25f, 1));
						ui_next_hover_cursor(ui, Cursor_Hand);
						UI_Element *option_el = ui_element_f(ui, flags, "path-option-%.*s", STR8_EXPAND(option_path));
						Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, option_el);
						if (interaction.hover)
						{
							option_el->background_color = vec4(0.2f, 0.2f,0.2f, 1);
						}
						if (interaction.pressed)
						{
							option_el->background_color = vec4(0.12f, 0.12f,0.12f, 1);
							if (interaction.hover)
							{
								option_el->background_color = vec4(0.1f, 0.1f,0.1f, 1);
							}
						}
						if (interaction.clicked)
						{
							selected_option_index = option_index;
						}
						
						ui_parent(ui, option_el) ui_horizontal_layout(ui) ui_padding(ui, ui_size_parent_remaining())
						{
							ui_next_width(ui, ui_size_text_dim(1));
							ui_label(ui, option_path);
						}
					}
					
					if (selected_option_index < mplayer->path_lister.filtered_paths_count)
					{
						mplayer_set_path_lister_path(&mplayer->path_lister, mplayer->path_lister.filtered_paths[selected_option_index]);
					}
				}
				
				ui_spacer(ui, ui_size_parent_remaining());
				
				ui_next_height(ui, ui_size_by_childs(1));
				ui_horizontal_layout(ui) ui_padding(ui, ui_size_parent_remaining())
				{
					ui_next_height(ui, ui_size_text_dim(1));
					ui_next_width(ui, ui_size_text_dim(1));
					if (mplayer_ui_underlined_button(ui, str8_lit("Ok")).clicked)
					{
						if (mplayer->settings.locations_count < MAX_LOCATION_COUNT)
						{
							Mplayer_Library_Location *location = mplayer->settings.locations + mplayer->settings.locations_count++;
							memory_copy(location->path, mplayer->path_lister.user_path.str, mplayer->path_lister.user_path.len);
							location->path_len = u32(mplayer->path_lister.user_path.len);
						}
						
						mplayer_close_path_lister(mplayer, &mplayer->path_lister);
						mplayer_save_settings(mplayer);
					}
					
					ui_spacer(ui, ui_size_parent_remaining());
					ui_next_height(ui, ui_size_text_dim(1));
					ui_next_width(ui, ui_size_text_dim(1));
					if (mplayer_ui_underlined_button(ui, str8_lit("Cancel")).clicked)
					{
						mplayer_close_path_lister(mplayer, &mplayer->path_lister);
					}
					
				}
			}
		}
	}
	
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
					ui_pref_width(ui, ui_size_percent(1, 1)) ui_pref_height(ui, ui_size_pixel(30, 1)) ui_pref_roundness(ui, 15)
				{
					ui_spacer_pixels(ui, 20, 1);
					if (ui_button(ui, str8_lit("Songs")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Music_Library)
							mplayer_change_mode(mplayer, MODE_Music_Library, 0);
					}
					
					ui_spacer_pixels(ui, 2, 1);
					if (ui_button(ui, str8_lit("Artists")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Artist_Library)
							mplayer_change_mode(mplayer, MODE_Artist_Library, 0);
					}
					
					ui_spacer_pixels(ui, 2, 1);
					if (ui_button(ui, str8_lit("Albums")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Album_Library)
							mplayer_change_mode(mplayer, MODE_Album_Library, 0);
					}
					
					ui_spacer_pixels(ui, 2, 1);
					if (ui_button(ui, str8_lit("Lyrics")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Lyrics)
							mplayer_change_mode(mplayer, MODE_Lyrics, 0);
					}
					
					ui_spacer_pixels(ui, 2, 1);
					if (ui_button(ui, str8_lit("Queue")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Queue)
							mplayer_change_mode(mplayer, MODE_Queue, 0);
					}
					
					ui_spacer(ui, ui_size_parent_remaining());
					if (ui_button(ui, str8_lit("Settings")).clicked)
					{
						if (mplayer->mode_stack->mode != MODE_Settings)
							mplayer_change_mode(mplayer, MODE_Settings, 0);
					}
					ui_spacer_pixels(ui, 20, 1);
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
							ui_spacer_pixels(ui, 50, 0);
							
							ui_next_background_color(ui, vec4(1, 1, 1, 0));
							ui_next_text_color(ui, vec4(1, 1, 1, 1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, str8_lit("Artists"));
						}
						
						
						ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 1))
						{
							u32 selected_artist_id = 0;
							ui_for_each_grid_item(ui, str8_lit("library-artists-grid"), mplayer->library.artists_count - 1, vec2(250.0f, 220.0f), 10, artist_index)
							{
								Mplayer_Item_ID artist_id = 1 + artist_index;
								mplayer_ui_aritst_item(ui, mplayer, artist_id);
							}
							
						}
					} break;
					
					case MODE_Album_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(69, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 1);
							
							ui_next_background_color(ui, vec4(1, 1, 1, 0));
							ui_next_text_color(ui, vec4(1, 1, 1, 1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, str8_lit("Albums"));
						}
						
						ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 1))
						{
							ui_for_each_grid_item(ui, str8_lit("library-albums-grid"), mplayer->library.albums_count-1, vec2(300.0f, 300.0f), 10, album_index)
							{
								Mplayer_Item_ID album_id = 1 + album_index;
								mplayer_ui_album_item(ui, mplayer, album_id);
							}
						}
						
					} break;
					
					case MODE_Music_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(90, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 1);
							
							ui_vertical_layout(ui) ui_padding(ui, ui_size_pixel(10, 1))
							{
								ui_next_background_color(ui, vec4(1, 1, 1, 0));
								ui_next_text_color(ui, vec4(1, 1, 1, 1));
								ui_next_width(ui, ui_size_text_dim(1));
								ui_next_height(ui, ui_size_text_dim(1));
								ui_next_font(ui, &mplayer->header_label_font);
								ui_label(ui, str8_lit("Songs"));
								
								ui_spacer(ui, ui_size_parent_remaining());
								
								ui_next_height(ui, ui_size_by_childs(1));
								ui_horizontal_layout(ui)
									ui_pref_height(ui, ui_size_text_dim(1))
									ui_pref_width(ui, ui_size_text_dim(1))
								{
									if (mplayer_ui_underlined_button(ui, str8_lit("Queue All")).clicked)
									{
										for (u32 track_id = 1; track_id < mplayer->library.tracks_count; track_id += 1)
										{
											mplayer_queue_last(mplayer, track_id);
										}
										mplayer_queue_resume(mplayer);
									}
									
									ui_spacer_pixels(ui, 10, 1);
									
								}
							}
						}
						
						ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 0))
						{
							ui_for_each_list_item(ui, str8_lit("library-tracks-list"), mplayer->library.tracks_count - 1, 50.0f, 1.0f, track_index)
							{
								Mplayer_Item_ID track_id = 1 + track_index;
								mplayer_ui_track_item(ui, mplayer, track_id);
							}
						}
					} break;
					
					case MODE_Artist_Albums:
					{
						Mplayer_Artist *artist = mplayer_artist_by_id(&mplayer->library, mplayer->selected_artist_id);
						
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(69, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 1);
							
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, artist->name);
						}
						
						ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 1))
						{
							ui_for_each_grid_item(ui, str8_lit("artist-albums-grid"), artist->albums.count, vec2(300.0f, 300.0f), 10, album_index)
							{
								Mplayer_Item_ID album_id = artist->albums.items[album_index];
								mplayer_ui_album_item(ui, mplayer, album_id);
							}
						}
						
					} break;
					
					case MODE_Album_Tracks:
					{
						Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, mplayer->selected_album_id);
						Mplayer_Item_Image *image = mplayer_get_image_by_id(mplayer, album->header.image_id, 1);
						
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(200, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 10, 1);
							
							ui_vertical_layout(ui)
							{
								ui_spacer_pixels(ui, 10, 1);
								
								ui_horizontal_layout(ui)
								{
									// NOTE(fakhri): album cover image
									{
										UI_Element_Flags flags = 0;
										if (mplayer_item_image_ready(*image))
										{
											flags |= UI_FLAG_Draw_Image;
											ui_next_texture_tint_color(ui, vec4(1, 1, 1, 1));
											ui_next_texture(ui, image->texture);
										}
										else
										{
											flags |= UI_FLAG_Draw_Background;
											ui_next_background_color(ui, vec4(0.2f, 0.2f, 0.2f, 1.0f));
										}
										
										ui_next_roundness(ui, 10);
										ui_next_width(ui, ui_size_pixel(180, 1));
										ui_next_height(ui, ui_size_pixel(180, 1));
										ui_element(ui, {}, flags);
									}
									
									ui_spacer_pixels(ui, 10, 1);
									ui_vertical_layout(ui)
									{
										ui_spacer_pixels(ui, 30, 1);
										ui_next_width(ui, ui_size_text_dim(1));
										ui_next_height(ui, ui_size_text_dim(1));
										ui_label(ui, str8_lit("Album"));
										
										ui_spacer_pixels(ui, 20, 1);
										ui_next_width(ui, ui_size_text_dim(1));
										ui_next_height(ui, ui_size_text_dim(1));
										ui_next_font(ui, &mplayer->header_label_font);
										ui_label(ui, album->name);
										
										//ui_spacer_pixels(ui, 1, 1);
										
										// NOTE(fakhri): artist button
										{
											ui_next_width(ui, ui_size_text_dim(1));
											ui_next_height(ui, ui_size_text_dim(1));
											ui_next_font(ui, &mplayer->font);
											ui_next_text_color(ui, vec4(0.4f, 0.4f, 0.4f, 1));
											Mplayer_UI_Interaction interaction = mplayer_ui_underlined_button_f(ui, "%.*s###_album_artist_button", STR8_EXPAND(album->artist));
											if (interaction.hover)
											{
												interaction.element->text_color = vec4(0.6f, 0.6f, 0.6f, 1);
											}
											if (interaction.pressed)
											{
												interaction.element->text_color = vec4(1, 1, 1, 1);
											}
											
											if (interaction.clicked)
											{
												assert(album->artist_id);
												mplayer->selected_artist_id = album->artist_id;
												mplayer_change_mode(mplayer, MODE_Artist_Albums, album->artist_id);
											}
										}
										
										ui_spacer_pixels(ui, 10, 1);
										
										// NOTE(fakhri): queue album button
										{
											ui_next_width(ui, ui_size_text_dim(1));
											ui_next_height(ui, ui_size_text_dim(1));
											ui_next_font(ui, &mplayer->font);
											ui_next_text_color(ui, vec4(0.6f, 0.6f, 0.6f, 1));
											Mplayer_UI_Interaction interaction = mplayer_ui_underlined_button_f(ui, "Queue Album");
											if (interaction.pressed)
											{
												interaction.element->text_color = vec4(1, 1, 1, 1);
											}
											
											if (interaction.clicked)
											{
												for (u32 track_index = 0; track_index < album->tracks.count; track_index += 1)
												{
													mplayer_queue_last(mplayer, album->tracks.items[track_index]);
												}
												mplayer_queue_resume(mplayer);
											}
										}
										
									}
								}
								ui_spacer_pixels(ui, 10, 1);
							}
						}
						ui_spacer_pixels(ui, 20, 0);
						
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 0);
							ui_for_each_list_item(ui, str8_lit("album-tracks-list"), album->tracks.count, 50.0f, 1.0f, track_index)
							{
								Mplayer_Item_ID track_id = album->tracks.items[track_index];
								mplayer_ui_track_item(ui, mplayer, track_id);
							}
							
							ui_spacer_pixels(ui, 50, 0);
						}
						ui_spacer_pixels(ui, 10, 1);
					} break;
					
					case MODE_Lyrics:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(69, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 0);
							
							ui_next_background_color(ui, vec4(1, 1, 1, 0));
							ui_next_text_color(ui, vec4(1, 1, 1, 1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, str8_lit("Lyrics"));
						}
						
					} break;
					
					case MODE_Settings:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(69, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 0);
							
							ui_next_background_color(ui, vec4(1, 1, 1, 0));
							ui_next_text_color(ui, vec4(1, 1, 1, 1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->header_label_font);
							ui_label(ui, str8_lit("Settings"));
						}
						
						ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 1))
						{
							ui_next_childs_axis(ui, Axis2_Y);
							ui_parent(ui, ui_element(ui, str8_lit("setting_scrollable_view_test"), UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Clip | UI_FLAG_Animate_Scroll))
								ui_padding(ui, ui_size_pixel(20, 1))
							{
								ui_next_height(ui, ui_size_by_childs(1));
								ui_horizontal_layout(ui)
								{
									ui_next_width(ui, ui_size_text_dim(1));
									ui_next_height(ui, ui_size_text_dim(1));
									ui_label(ui, str8_lit("Library"));
								}
								
								ui_spacer_pixels(ui, 10, 1);
								
								ui_next_width(ui, ui_size_parent_remaining());
								ui_next_height(ui, ui_size_by_childs(1));
								ui_parent(ui, ui_element(ui, {}, 0))
								{
									ui_next_background_color(ui, vec4(0.2f,0.2f,0.2f,1));
									ui_next_height(ui, ui_size_pixel(60, 1));
									ui_next_roundness(ui, 5);
									UI_Element *add_location_el = ui_element(ui, str8_lit("setting-music-library"), UI_FLAG_Draw_Background|UI_FLAG_Clickable);
									ui_parent(ui, add_location_el) ui_padding(ui, ui_size_parent_remaining()) ui_pref_height(ui, ui_size_by_childs(1))
										ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(20, 1))
									{
										Mplayer_UI_Interaction add_loc_interaction = ui_interaction_from_element(ui, add_location_el);
										if (add_loc_interaction.hover)
										{
											add_location_el->background_color = vec4(0.3f,0.3f,0.3f,1);
										}
										if (add_loc_interaction.pressed)
										{
											add_location_el->background_color = vec4(0.17f,0.17f,0.17f,1);
										}
										if (add_loc_interaction.clicked)
										{
											mplayer->show_library_locations = !mplayer->show_library_locations;
										}
										
										ui_next_width(ui, ui_size_text_dim(1));
										ui_next_height(ui, ui_size_text_dim(1));
										ui_label(ui, str8_lit("Music Library Locations"));
										
										ui_spacer(ui, ui_size_parent_remaining());
										
										ui_next_roundness(ui, 5);
										ui_next_background_color(ui, vec4(0.25f,0.25f,0.25f,1));
										ui_next_width(ui, ui_size_by_childs(1));
										ui_next_height(ui, ui_size_by_childs(1));
										ui_next_childs_axis(ui, Axis2_Y);
										ui_next_hover_cursor(ui, Cursor_Hand);
										UI_Element *loc_el = ui_element(ui, str8_lit("add-location-button"), UI_FLAG_Draw_Background | UI_FLAG_Clickable);
										if (ui_interaction_from_element(ui, loc_el).clicked)
										{
											mplayer_open_path_lister(mplayer, &mplayer->path_lister);
										}
										
										ui_parent(ui, loc_el) 
											ui_padding(ui, ui_size_pixel(5, 1)) ui_pref_width(ui, ui_size_by_childs(1))
											ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(5, 1))
										{
											ui_next_width(ui, ui_size_text_dim(1));
											ui_next_height(ui, ui_size_text_dim(1));
											ui_label(ui, str8_lit("Add Location"));
										}
									}
									
									u32 location_to_delete = mplayer->settings.locations_count;
									
									if (mplayer->show_library_locations) 
										ui_pref_background_color(ui, vec4(0.15f,0.15f,0.15f,1))
										ui_pref_flags(ui, UI_FLAG_Animate_Dim)
										ui_pref_height(ui, ui_size_by_childs(1))
										ui_pref_roundness(ui, 10)
										ui_for_each_list_item(ui, str8_lit("settings-location-list"), mplayer->settings.locations_count, 40.0f, 1.0f, location_index)
										
										ui_pref_height(ui, ui_size_parent_remaining()) ui_pref_flags(ui, 0) ui_pref_roundness(ui, 100)
									{
										Mplayer_Library_Location *location = mplayer->settings.locations + location_index;
										ui_next_border_thickness(ui, 2);
										u32 flags = UI_FLAG_Animate_Dim | UI_FLAG_Clip | UI_FLAG_Draw_Background;
										UI_Element *location_el = ui_element(ui, {}, flags);
										location_el->child_layout_axis = Axis2_X;
										
										ui_parent(ui, location_el) ui_padding(ui, ui_size_pixel(10, 1))
										{
											ui_next_width(ui, ui_size_text_dim(1));
											ui_label(ui, str8(location->path, location->path_len));
											
											ui_spacer(ui, ui_size_parent_remaining());
											
											ui_next_width(ui, ui_size_pixel(20, 1));
											ui_vertical_layout(ui) ui_padding(ui, ui_size_parent_remaining())
											{
												ui_next_height(ui, ui_size_pixel(20, 1));
												ui_next_roundness(ui, 10);
												ui_next_background_color(ui, vec4(0.3f, 0.3f, 0.3f, 1));
												if (ui_button_f(ui, "X###delete-location-%p", location).clicked)
												{
													location_to_delete = location_index;
													mplayer_save_settings(mplayer);
												}
											}
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
								
								ui_spacer_pixels(ui, 20,1);
								ui_next_width(ui, ui_size_text_dim(1));
								ui_next_height(ui, ui_size_text_dim(1));
								if (mplayer_ui_underlined_button(ui, str8_lit("Reindex Library")).clicked)
								{
									mplayer_index_library(mplayer);
								}
							}
						}
					} break;
					
					case MODE_Queue:
					{
						// NOTE(fakhri): header
						ui_next_background_color(ui, vec4(0.05f, 0.05f, 0.05f, 1));
						ui_next_height(ui, ui_size_pixel(100, 1));
						ui_horizontal_layout(ui)
						{
							ui_spacer_pixels(ui, 50, 1);
							
							ui_vertical_layout(ui) ui_padding(ui, ui_size_pixel(10, 1))
							{
								ui_next_background_color(ui, vec4(1, 1, 1, 0));
								ui_next_text_color(ui, vec4(1, 1, 1, 1));
								ui_next_width(ui, ui_size_text_dim(1));
								ui_next_height(ui, ui_size_text_dim(1));
								ui_next_font(ui, &mplayer->header_label_font);
								
								String8 title = str8_f(&mplayer->frame_arena, "Queue(%d)", mplayer->queue.count - 1);
								ui_label(ui, title);
								
								ui_spacer(ui, ui_size_parent_remaining());
								
								ui_next_height(ui, ui_size_by_childs(1));
								ui_horizontal_layout(ui)
									ui_pref_height(ui, ui_size_text_dim(1))
									ui_pref_width(ui, ui_size_text_dim(1))
								{
									if (mplayer_ui_underlined_button(ui, str8_lit("Shuffle Queue")).clicked)
									{
										mplayer_queue_shuffle(mplayer);
										mplayer_set_current(mplayer, 1);
									}
									ui_spacer_pixels(ui, 30, 1);
									
									if (mplayer_ui_underlined_button(ui, str8_lit("Clear Queue")).clicked)
									{
										mplayer_clear_queue(mplayer);
									}
									ui_spacer_pixels(ui, 10, 1);
									
								}
							}
						}
						
						
						ui_next_width(ui, ui_size_parent_remaining());
						ui_next_height(ui, ui_size_parent_remaining());
						ui_horizontal_layout(ui) ui_padding(ui, ui_size_pixel(50, 0))
						{
							ui_for_each_list_item(ui, str8_lit("queue-tracks-list"), mplayer->queue.count-1, 50.0f, 1.0f, index)
							{
								Mplayer_Queue_Index queue_index = (Mplayer_Queue_Index)index + 1;
								Mplayer_Item_ID track_id = mplayer->queue.tracks[queue_index];
								Mplayer_Track *track = mplayer_track_by_id(&mplayer->library, track_id);
								
								V4_F32 bg_color = vec4(0.15f, 0.15f,0.15f, 1);
								if (track_id == mplayer_queue_current_track_id(mplayer))
								{
									bg_color = vec4(0, 0, 0, 1);
								}
								
								UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
								
								ui_next_background_color(ui, bg_color);
								ui_next_border_color(ui, vec4(0, 0, 0, 1));
								ui_next_border_thickness(ui, 1);
								ui_next_hover_cursor(ui, Cursor_Hand);
								UI_Element *track_el = ui_element_f(ui, flags, "queue_track_%p", track);
								Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, track_el);
								if (interaction.clicked)
								{
									mplayer_set_current(mplayer, Mplayer_Queue_Index(queue_index));
								}
								
								ui_parent(ui, track_el) ui_vertical_layout(ui) ui_padding(ui, ui_size_parent_remaining())
									ui_horizontal_layout(ui)
								{
									ui_spacer_pixels(ui, 10, 1);
									
									ui_next_width(ui, ui_size_text_dim(1));
									ui_next_height(ui, ui_size_text_dim(1));
									ui_element_f(ui, UI_FLAG_Draw_Text, "%.*s##library_track_name_%p", 
										STR8_EXPAND(track->title), track);
								}
							}
						}
						
					} break;
					
				}
			}
		}
		
		// NOTE(fakhri): track control
		ui_next_background_color(ui, vec4(0.f, 0.f, 0.f, 1));
		ui_pref_height(ui, ui_size_pixel(125, 1))
			ui_vertical_layout(ui)
		{
			ui_spacer_pixels(ui, 10, 1);
			
			//- NOTE(fakhri): track 
			//ui_next_background_color(ui, vec4(1, 0, 1, 1));
			ui_pref_height(ui, ui_size_pixel(20, 1))
				ui_horizontal_layout(ui)
			{
				ui_spacer_pixels(ui, 10, 1);
				
				Mplayer_Track *current_music = mplayer_queue_get_current_track(mplayer);
				// NOTE(fakhri): timestamp
				{
					ui_next_width(ui, ui_size_text_dim(1));
					ui_next_font(ui, &mplayer->timestamp_font);
					
					String8 timestamp_string = str8_lit("--:--:--");
					if (current_music && current_music->flac_stream)
					{
						Mplayer_Timestamp timestamp = flac_get_current_timestap(current_music->flac_stream->next_sample_number, u64(current_music->flac_stream->streaminfo.sample_rate));
						timestamp_string = str8_f(&mplayer->frame_arena, "%.2d:%.2d:%.2d", timestamp.hours, timestamp.minutes, timestamp.seconds);
					}
					
					ui_label(ui, timestamp_string);
				}
				
				ui_spacer_pixels(ui, 20, 1);
				
				// NOTE(fakhri): track progres slider
				ui_next_width(ui, ui_size_percent(1.0f, 0.25f));
				ui_next_background_color(ui, vec4(0.3f, 0.3f, 0.3f, 1.0f));
				ui_next_roundness(ui, 5);
				if (current_music && current_music->flac_stream)
				{
					u64 samples_count = 0;
					u64 current_playing_sample = 0;
					samples_count = current_music->flac_stream->streaminfo.samples_count;
					current_playing_sample = current_music->flac_stream->next_sample_number;
					
					Mplayer_UI_Interaction progress = ui_slider_u64(ui, str8_lit("track_progress_slider"), &current_playing_sample, 0, samples_count);
					if (progress.pressed)
					{
						mplayer_queue_pause(mplayer);
						flac_seek_stream(current_music->flac_stream, (u64)current_playing_sample);
					}
					else if (progress.released)
					{
						mplayer_queue_resume(mplayer);
					}
				}
				else
				{
					u64 current_playing_sample = 0;
					ui_slider_u64(ui, str8_lit("track_progress_slider"), &current_playing_sample, 0, 0);
				}
				
				ui_spacer_pixels(ui, 10, 1);
				
				{
					ui_next_width(ui, ui_size_text_dim(1));
					
					String8 timestamp_string = str8_lit("--:--:--");
					if (current_music && current_music->flac_stream)
					{
						Mplayer_Timestamp timestamp = flac_get_current_timestap(current_music->flac_stream->streaminfo.samples_count, u64(current_music->flac_stream->streaminfo.sample_rate));
						timestamp_string = str8_f(&mplayer->frame_arena, "%.2d:%.2d:%.2d", timestamp.hours, timestamp.minutes, timestamp.seconds);
					}
					ui_label(ui, timestamp_string);
				}
				
				ui_spacer_pixels(ui, 10, 1);
			}
			
			ui_pref_height(ui, ui_size_parent_remaining())
				ui_horizontal_layout(ui)
			{
				ui_next_width(ui, ui_size_percent(1.0f/3.0f, 1));
				ui_horizontal_layout(ui)
				{
					ui_spacer_pixels(ui, 10, 1);
					
					Mplayer_Track *current_music = mplayer_queue_get_current_track(mplayer);
					// NOTE(fakhri): cover image
					if (current_music)
					{
						Mplayer_Item_Image *cover_image = mplayer_get_image_by_id(mplayer, current_music->header.image_id, 1);
						
						// NOTE(fakhri): cover
						{
							u32 flags = 0;
							if (mplayer_item_image_ready(*cover_image))
							{
								flags |= UI_FLAG_Draw_Image;
								ui_next_texture_tint_color(ui, vec4(1, 1, 1, 1));
								ui_next_texture(ui, cover_image->texture);
							}
							
							ui_next_width(ui, ui_size_pixel(90, 1));
							ui_next_height(ui, ui_size_pixel(90, 1));
							ui_element(ui, {}, flags);
						}
						
						ui_spacer_pixels(ui, 10, 1);
						
						// NOTE(fakhri): track info
						ui_next_width(ui, ui_size_parent_remaining());
						ui_pref_flags(ui, UI_FLAG_Clip)
							ui_vertical_layout(ui)
						{
							ui_spacer_pixels(ui, 25, 0);
							
							ui_next_height(ui, ui_size_text_dim(1));
							ui_next_width(ui, ui_size_text_dim(1));
							if (mplayer_ui_underlined_button_f(ui, "%.*s##_song_title_button", STR8_EXPAND(current_music->title)).clicked)
							{
								assert(current_music->artist_id);
								mplayer->selected_album_id = current_music->album_id;
								mplayer_change_mode(mplayer, MODE_Album_Tracks, current_music->album_id);
							}
							
							ui_spacer_pixels(ui, 5, 0);
							
							ui_next_height(ui, ui_size_text_dim(1));
							ui_next_width(ui, ui_size_text_dim(1));
							ui_next_font(ui, &mplayer->small_font);
							if (mplayer_ui_underlined_button_f(ui, "%.*s##_song_artist_button", STR8_EXPAND(current_music->artist)).clicked)
							{
								assert(current_music->artist_id);
								mplayer->selected_artist_id = current_music->artist_id;
								mplayer_change_mode(mplayer, MODE_Artist_Albums, current_music->artist_id);
							}
						}
					}
				}
				
				// NOTE(fakhri): next/prevplay/pause buttons
				ui_next_width(ui, ui_size_percent(1.0f/3.0f, 1));
				ui_vertical_layout(ui)
				{
					ui_spacer_pixels(ui, 25, 1);
					
					ui_next_height(ui, ui_size_parent_remaining());
					ui_next_width(ui, ui_size_parent_remaining());
					ui_horizontal_layout(ui)
					{
						ui_spacer(ui, ui_size_parent_remaining());
						
						ui_pref_width(ui, ui_size_text_dim(1))
							ui_pref_height(ui, ui_size_text_dim(1))
							ui_pref_background_color(ui, vec4(0.3f, 0.3f, 0.3f, 1))
						{
							ui_spacer(ui, ui_size_parent_remaining());
							
							ui_next_roundness(ui, 20);
							ui_next_width(ui, ui_size_pixel(40, 1));
							ui_next_height(ui, ui_size_pixel(40, 1));
							if (ui_button(ui, str8_lit("<<")).clicked)
							{
								mplayer_play_prev_in_queue(mplayer);
							}
							
							ui_spacer_pixels(ui, 10, 1);
							
							ui_next_roundness(ui, 25);
							ui_next_width(ui, ui_size_pixel(50, 1));
							ui_next_height(ui, ui_size_pixel(50, 1));
							if (ui_button(ui, mplayer_is_queue_playing(mplayer)? str8_lit("II") : str8_lit("I>")).clicked)
							{
								mplayer_queue_toggle_play(mplayer);
							}
							ui_spacer_pixels(ui, 10, 1);
							
							ui_next_roundness(ui, 20);
							ui_next_width(ui, ui_size_pixel(40, 1));
							ui_next_height(ui, ui_size_pixel(40, 1));
							if (ui_button(ui, str8_lit(">>")).clicked)
							{
								mplayer_play_next_in_queue(mplayer);
							}
							
							ui_spacer(ui, ui_size_parent_remaining());
						}
						
						ui_spacer(ui, ui_size_parent_remaining());
					}
					
					ui_spacer(ui, ui_size_parent_remaining());
				}
				
				// NOTE(fakhri): volume
				ui_next_width(ui, ui_size_parent_remaining());
				ui_horizontal_layout(ui)
				{
					ui_spacer(ui, ui_size_parent_remaining());
					
					ui_next_width(ui, ui_size_pixel(150, 1));
					ui_vertical_layout(ui)
					{
						ui_spacer(ui, ui_size_parent_remaining());
						
						// NOTE(fakhri): volume slider
						{
							ui_next_height(ui, ui_size_pixel(12, 1));
							ui_next_background_color(ui, vec4(0.3f, 0.3f, 0.3f, 1.0f));
							ui_next_roundness(ui, 5);
							ui_slider_f32(ui, str8_lit("volume-control-slider"), &mplayer->volume, 0, 1);
						}
						
						ui_spacer(ui, ui_size_parent_remaining());
					}
					
					ui_spacer_pixels(ui, 10, 1);
				}
			}
		}
	}
	
	ui_end(ui);
	
	// NOTE(fakhri): draw fps
	#if 0
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
	#endif
		
}
