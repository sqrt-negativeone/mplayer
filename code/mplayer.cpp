#include "mplayer_memory.h"
#include "mplayer_string.cpp"
#include "mplayer_buffer.h"
#include "mplayer_thread_context.cpp"

#define STBI_SSE2
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"


#include "third_party/samplerate.h"


typedef void Work_Proc(void *data);
struct Work_Entry
{
	Work_Proc *work;
	void *input;
};

#define WORK_SIG(name) void name(void *input)

typedef b32 Push_Work_Proc(Work_Proc *work, void *input);
typedef b32 Do_Next_Work_Proc();

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


typedef u32 File_Open_Flags;
enum
{
	File_Open_Read,
	File_Open_Write,
};

typedef void File_Handle;

typedef i64 Atomic_Increment64_Proc(volatile i64 *addend);
typedef i64 Atomic_Decrement64_Proc(volatile i64 *addend);
typedef b32 Atomic_Compare_And_Exchange64_Proc(i64 volatile *dst, i64 expect, i64 exchange);
typedef b32 Atomic_Compare_And_Exchange_Pointer_Proc(Address volatile *dst, Address expect, Address exchange);

typedef void     *Allocate_Memory_Proc(u64 size);
typedef void      Deallocate_Memory_Proc(void *memory);
typedef b32       File_Iter_Begin_Proc(File_Iterator_Handle *it, String8 path);
typedef File_Info File_Iter_Next_Proc (File_Iterator_Handle *it, Memory_Arena *arena);
typedef void      File_Iter_End_Proc  (File_Iterator_Handle *it);

typedef File_Handle *Open_File_Proc(String8 path, u32 flags);
typedef void   Close_File_Proc(File_Handle *file);
typedef Buffer File_Read_block_Proc(File_Handle *file, void *data, u64 size);

typedef Buffer Load_Entire_File(String8 file_path, Memory_Arena *arena);

struct Mplayer_OS_Vtable
{
	File_Iter_Begin_Proc *file_iter_begin;
	File_Iter_Next_Proc  *file_iter_next;
	File_Iter_End_Proc   *file_iter_end;
	Load_Entire_File     *load_entire_file;
	Open_File_Proc       *open_file;
	Close_File_Proc      *close_file;
	File_Read_block_Proc *read_block;
	
	Push_Work_Proc    *push_work;
	Do_Next_Work_Proc *do_next_work;
	
	Allocate_Memory_Proc   *alloc;
	Deallocate_Memory_Proc *dealloc;
	
	Atomic_Increment64_Proc *atomic_increment64;
	Atomic_Decrement64_Proc *atomic_decrement64;
	Atomic_Compare_And_Exchange64_Proc *atomic_compare_and_exchange64;
	Atomic_Compare_And_Exchange_Pointer_Proc *atomic_compare_and_exchange_pointer;
};

global Mplayer_OS_Vtable *platform = 0;


#include "mplayer_memory.cpp"

#include "mplayer_base.cpp"
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

enum Mplayer_Item_Kind
{
	Item_Kind_Track,
	Item_Kind_Album,
	Item_Kind_Artist,
};

struct Mplayer_Items_List
{
	struct Mplayer_Item *first;
	struct Mplayer_Item *last;
	u32 count;
};

enum Mplayer_Items_Links
{
	Links_Library,
	Links_Artist,
	Links_Album,
	Links_Count,
};


struct Mplayer_Item_Image
{
	b32 in_disk;
	String8 path;
	// NOTE(fakhri): for cover and artist picture if we have it
	Buffer texture_data;
	Texture texture;
};

struct Mplayer_Item
{
	Mplayer_Item *next_links[Links_Count];
	
	Mplayer_Item_Kind kind;
	
	Memory_Arena arena;
	String8 path;
	
	Mplayer_Item_Image image;
	
	Memory_Arena transient_arena;
	Flac_Stream *flac_stream;
	b32 file_loaded;
	Buffer flac_file_buffer;
	
	String8 title;
	String8 album;
	String8 artist;
	String8 genre;
	String8 date;
	String8 track_number;
	
	u64 hash;
	
	Mplayer_Item *album_ref;
	Mplayer_Item *artist_ref;
	Mplayer_Items_List tracks;
	Mplayer_Items_List albums;
};

enum Mplayer_Mode
{
	MODE_Music_Library,
	MODE_Artist_Library,
	MODE_Album_Library,
	
	MODE_Artist_Albums,
	MODE_Album_Tracks,
	
	MODE_Lyrics,
};

struct Mplayer_Mode_Stack
{
	Mplayer_Mode_Stack *next;
	Mplayer_Mode_Stack *prev;
	Mplayer_Mode mode;
	Mplayer_Item *item;
	
	f32 view_target_scroll;
	f32 view_scroll_speed;
	f32 view_scroll;
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
	Mplayer_Font header_label_font;
	Mplayer_Font font;
	Mplayer_Font debug_font;
	Mplayer_Font big_debug_font;
	f32 volume;
	f32 seek_percentage;
	
	String8 library_path;
	
	Mplayer_OS_Vtable os;
	
	Mplayer_Mode_Stack *mode_stack;
	Mplayer_Mode_Stack *mode_stack_free_list_first;
	Mplayer_Mode_Stack *mode_stack_free_list_last;
	// Mplayer_Mode mode;
	
	Mplayer_Items_List artists;
	Mplayer_Items_List albums;
	Mplayer_Items_List tracks;
	
	Mplayer_Item *current_music;
	Mplayer_Item *selected_artist;
	Mplayer_Item *selected_album;
	
	f32 track_name_hover_t;
	
	// TODO(fakhri): pick a good number for this
	Mplayer_Item_Image images[1024];
	u32 images_count;
};

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

internal Mplayer_Item *
mplayer_find_or_create_album(Mplayer_Context *mplayer, Mplayer_Item *artist, String8 album_name)
{
	assert(artist->kind == Item_Kind_Artist);
	u64 hash = str8_hash_case_insensitive(album_name);
	Mplayer_Item *result = 0;
	
	for (Mplayer_Item *album = artist->albums.first;
		album;
		album = album->next_links[Links_Artist])
	{
		if (album->hash == hash && str8_match(album_name, album->album, MatchFlag_CaseInsensitive))
		{
			result = album;
			break;
		}
	}
	
	if (!result)
	{
		result = m_arena_push_struct_z(&mplayer->main_arena, Mplayer_Item);
		result->album = str8_copy(&mplayer->main_arena, album_name);
		result->artist = str8_copy(&mplayer->main_arena, artist->artist);
		result->hash = hash;
		
		QueuePush_Next(artist->albums.first, artist->albums.last, result, next_links[Links_Artist]);
		QueuePush_Next(mplayer->albums.first, mplayer->albums.last, result, next_links[Links_Library]);
		
		result->artist_ref = artist;
		artist->albums.count += 1;
		mplayer->albums.count += 1;
	}
	
	result->kind = Item_Kind_Album;
	return result;
}

internal Mplayer_Item *
mplayer_find_or_create_artist(Mplayer_Context *mplayer, String8 artist_name)
{
	u64 hash = str8_hash_case_insensitive(artist_name);
	Mplayer_Item *result = 0;
	for (Mplayer_Item *artist = mplayer->artists.first;
		artist;
		artist = artist->next_links[Links_Library])
	{
		if (artist->hash == hash && str8_match(artist_name, artist->artist, MatchFlag_CaseInsensitive))
		{
			result = artist;
			break;
		}
	}
	if (!result)
	{
		result = m_arena_push_struct_z(&mplayer->main_arena, Mplayer_Item);
		result->artist = str8_copy(&mplayer->main_arena, artist_name);
		result->hash = hash;
		
		QueuePush_Next(mplayer->artists.first, mplayer->artists.last, result, next_links[Links_Library]);
		mplayer->artists.count += 1;
	}
	result->kind = Item_Kind_Artist;
	return result;
}

internal Mplayer_Item *
mplayer_make_track_item(Mplayer_Context *mplayer)
{
	Mplayer_Item *result = 0;
	result = m_arena_push_struct_z(&mplayer->main_arena, Mplayer_Item);
	assert(result);
	result->kind = Item_Kind_Track;
	return result;
}

internal void
mplayer_push_music_track(Mplayer_Context *mplayer, Mplayer_Item *music_track)
{
	assert(music_track->kind == Item_Kind_Track);
	mplayer->tracks.count += 1;
	QueuePush_Next(mplayer->tracks.first, mplayer->tracks.last, music_track, next_links[Links_Library]);
}

internal void
mplayer_music_reset(Mplayer_Item *music_track)
{
	if (music_track && music_track->flac_stream)
	{
		music_track->flac_stream->bitstream.pos = music_track->flac_stream->first_block_pos;
	}
}

internal b32
mplayer_is_valid_image(Mplayer_Item_Image image)
{
	b32 result = (image.in_disk && image.path.len) || is_valid(image.texture_data);
	return result;
}

internal b32
mplayer_item_image_ready(Mplayer_Item *item)
{
	b32 result = item->image.texture.state == Texture_State_Loaded;
	return result;
}

internal void
mplayer_load_item_image(Mplayer_Context *mplayer, Mplayer_Item *item)
{
	Mplayer_Item_Image *image = &item->image;
	if (!is_texture_valid(image->texture))
	{
		image->texture = reserve_texture_handle(mplayer->render_ctx);
	}
	
	if (image->texture.state == Texture_State_Unloaded)
	{
		image->texture.state = Texture_State_Loading;
		if (!is_valid(image->texture_data) && image->in_disk)
		{
			image->texture_data = platform->load_entire_file(image->path, &item->transient_arena);
		}
		
		if (is_valid(image->texture_data))
		{
			platform->push_work(mplayer->render_ctx->upload_texture_to_gpu_work, 
				make_texure_upload_data(mplayer->render_ctx, &image->texture, image->texture_data));
		}
	}
	
}

internal void
mpalyer_draw_item_image(Render_Group *group, Mplayer_Context *mplayer, Mplayer_Item *item, V2_F32 pos, V2_F32 dim, V4_F32 solid_color = vec4(1,1,1,1), V4_F32 image_color = vec4(1,1,1,1), f32 roundness = 0.0f)
{
	mplayer_load_item_image(mplayer, item);
	if (mplayer_item_image_ready(item))
	{
		push_image(group, pos, dim, item->image.texture, image_color, roundness);
	}
	else
	{
		push_rect(group, pos, dim, solid_color, roundness);
	}
}

internal void
mplayer_load_music_track(Mplayer_Context *mplayer, Mplayer_Item *music_track)
{
	assert(music_track->kind == Item_Kind_Track);
	if (!music_track->file_loaded)
	{
		music_track->flac_file_buffer = mplayer->os.load_entire_file(music_track->path, &music_track->transient_arena);
		music_track->file_loaded = true;
	}
	
	music_track->image.in_disk = false;
	if (!music_track->flac_stream)
	{
		music_track->flac_stream = m_arena_push_struct_z(&music_track->transient_arena, Flac_Stream);
		init_flac_stream(music_track->flac_stream, &music_track->transient_arena, music_track->flac_file_buffer);
		
		if (music_track->flac_stream->front_cover)
		{
			music_track->image.texture_data = music_track->flac_stream->front_cover->buffer;
			music_track->image.texture.state = Texture_State_Invalid;
		}
	}
}

internal void
mplayer_unload_music_track(Mplayer_Context *mplayer, Mplayer_Item *music)
{
	assert(music->kind == Item_Kind_Track);
	uninit_flac_stream(music->flac_stream);
	m_arena_free_all(&music->transient_arena);
	music->file_loaded = false;
	music->flac_stream = 0;
	music->flac_file_buffer = ZERO_STRUCT;
	
	// TODO(fakhri): delete texture from gpu memory!
	music->image = ZERO_STRUCT;
}

internal void
mplayer_play_music_track(Mplayer_Context *mplayer, Mplayer_Item *music_track)
{
	if (!music_track) return;
	assert(music_track->kind == Item_Kind_Track);
	if (mplayer->current_music != music_track)
	{
		Mplayer_Item *prev_music = mplayer->current_music;
		
		if (prev_music)
		{
			mplayer_unload_music_track(mplayer, prev_music);
		}
		
		mplayer_load_music_track(mplayer, music_track);
		mplayer_music_reset(music_track);
		mplayer->current_music = music_track;
		mplayer->play_track = true;
	}
}

internal b32
is_music_track_still_playing(Mplayer_Item *music_track)
{
	assert(music_track->kind == Item_Kind_Track);
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
	Text_Render_Flag_Centered,
	Text_Render_Flag_Limit_Width,
	Text_Render_Flag_Underline,
};

#define Text_Render_Flag_Centered_Bit    make_flag(Text_Render_Flag_Centered)
#define Text_Render_Flag_Limit_Width_Bit make_flag(Text_Render_Flag_Limit_Width)
#define Text_Render_Flag_Underline_Bit   make_flag(Text_Render_Flag_Underline)

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
	
	u32 flags = Text_Render_Flag_Limit_Width_Bit;
	if (should_underline)
	{
		flags |= Text_Render_Flag_Underline_Bit;
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
	draw_text(ui->group, font, pos, text_color, fancy_str8(text), Text_Render_Flag_Centered_Bit);
	return interaction;
}

internal Mplayer_Item *
ui_items_list(Mplayer_Context *mplayer, Mplayer_UI *ui, Mplayer_Items_List items, V2_F32 option_dim, Range2_F32 available_space, Mplayer_Items_Links next_link, V2_F32 padding = vec2(0, 0))
{
	for (Mplayer_Input_Event *event = mplayer->input.first_event; event; event = event->next)
	{
		if (event->consumed) continue;
		if (event->kind == Event_Kind_Mouse_Wheel)
		{
			event->consumed = true;
			
			mplayer->mode_stack->view_target_scroll -= event->scroll.y;
		}
	}
	
	Mplayer_Item *selected_item = 0;
	
	V2_F32 available_space_dim = range_dim(available_space);
	V2_F32 option_start_pos = range_center(available_space);
	option_start_pos.x -= 0.5f * (available_space_dim.width - option_dim.width) - padding.width;
	option_start_pos.y += 0.5f * (available_space_dim.height - option_dim.height) - padding.height;
	
	u32 options_per_row = u32(available_space_dim.width / (option_dim.width + padding.width));
	options_per_row = MAX(options_per_row, 1);
	
	u32 rows_count = items.count / options_per_row + !!(items.count % options_per_row);
	
	mplayer->mode_stack->view_target_scroll = CLAMP(0, mplayer->mode_stack->view_target_scroll, rows_count - 1);
	
	V2_F32 option_pos = option_start_pos;
	option_pos.y += mplayer->mode_stack->view_scroll * (option_dim.height + padding.height);
	
	Mplayer_Item *item = items.first;
	
	// NOTE(fakhri): skip until the first visible option 
	for (; item; 
		item = item->next_links[next_link])
	{
		Range2_F32 item_rect = range_center_dim(option_pos, option_dim);
		if (is_range_intersect(item_rect, available_space))
		{
			break;
		}
		
		option_pos.x += option_dim.width + padding.width;
		
		if (option_pos.x + 0.5f * option_dim.width > available_space.max_x)
		{
			option_pos.x = option_start_pos.x;
			option_pos.y -= option_dim.height + padding.height;
		}
	}
	
	
	for (; 
		item; 
		item = item->next_links[next_link])
	{
		V4_F32 bg_color = vec4(0.15f, 0.15f,0.15f, 1);
		
		Range2_F32 item_rect = range_center_dim(option_pos, option_dim);
		Range2_F32 visible_range = range_intersection(item_rect, available_space);
		if (!is_range_intersect(item_rect, available_space))
		{
			break;
		}
		
		switch (item->kind)
		{
			case Item_Kind_Artist:
			{
				// TODO(fakhri): better id
				Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, int_from_ptr(item), range_center(visible_range), range_dim(visible_range));
				
				if (interaction.hover)
				{
					bg_color = vec4(0.2f, 0.2f,0.2f, 1);
				}
				
				if (interaction.pressed)
				{
					bg_color = vec4(0.14f, 0.14f,0.14f, 1);
					selected_item = item;
				}
				
				push_rect(ui->group, option_pos, option_dim, bg_color);
				V2_F32 name_dim = font_compute_text_dim(&mplayer->font, item->artist);
				
				Range2_F32_Cut cut = range_cut_top(item_rect, 10); // padding
				cut = range_cut_top(cut.bottom, 30);
				
				Range2_F32 artist_name_rect = cut.top;
				Range2_F32 artist_image_rect = cut.bottom;
				draw_text(ui->group, &mplayer->font, range_center(artist_name_rect), vec4(1, 1, 1, 1), fancy_str8(item->artist), Text_Render_Flag_Centered_Bit);
				
			} break;
			
			case Item_Kind_Album:
			{
				bg_color = vec4(1, 1, 1, 0.35f);
				
				// TODO(fakhri): better id
				Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, int_from_ptr(item), range_center(visible_range), range_dim(visible_range));
				
				if (interaction.hover)
				{
					bg_color.a = 0.80f;
				}
				
				if (interaction.pressed)
				{
					bg_color.a = 0;
					selected_item = item;
				}
				
				mpalyer_draw_item_image(ui->group, mplayer, item, option_pos, option_dim, bg_color, bg_color);
				
				V2_F32 name_dim = font_compute_text_dim(&mplayer->font, item->album);
				
				Range2_F32_Cut cut = range_cut_top(item_rect, 10); // padding
				cut = range_cut_top(cut.bottom, 30);
				
				Range2_F32 album_name_rect = cut.top;
				Range2_F32 album_image_rect = cut.bottom;
				draw_text(ui->group, &mplayer->font, range_center(album_name_rect), vec4(1, 1, 1, 1), fancy_str8(item->album), Text_Render_Flag_Centered_Bit);
				
			} break;
			
			case Item_Kind_Track:
			{
				// TODO(fakhri): better id
				Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, int_from_ptr(item), range_center(visible_range), range_dim(visible_range));
				if (item == mplayer->current_music)
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
					selected_item = item;
				}
				
				// NOTE(fakhri): background
				push_rect(ui->group, option_pos, option_dim, bg_color);
				
				Range2_F32_Cut cut = range_cut_left(item_rect, 20); // horizontal padding
				
				cut = range_cut_left(cut.right, 500);
				Range2_F32 name_rect = cut.left;
				draw_text(ui->group, &mplayer->font, vec2(name_rect.min_x, range_center(name_rect).y), vec4(1, 1, 1, 1), fancy_str8(item->title, range_dim(name_rect).width), Text_Render_Flag_Limit_Width_Bit);
				
				draw_outline(ui->group, option_pos, option_dim, 1, vec4(0, 0, 0, 1));
			} break;
		}
		
		
		option_pos.x += option_dim.width + padding.width;
		
		if (option_pos.x + 0.5f * option_dim.width > available_space.max_x)
		{
			option_pos.x = option_start_pos.x;
			option_pos.y -= option_dim.height + padding.height;
		}
	}
	
	return selected_item;
}

internal void
mplayer_get_audio_samples(Sound_Config device_config, Mplayer_Context *mplayer, void *output_buf, u32 frame_count)
{
	if (mplayer->play_track)
	{
		Mplayer_Item *music = mplayer->current_music;
		assert(music->kind == Item_Kind_Track);
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

internal String8
mplayer_attempt_find_cover_image_in_dir(Mplayer_Context *mplayer, Memory_Arena *arena, String8 dir)
{
	String8 result = ZERO_STRUCT;
	Memory_Checkpoint scratch = begin_scratch(&arena, 1);
	File_Iterator_Handle *it = m_arena_push_struct(scratch.arena, File_Iterator_Handle);
	if (mplayer->os.file_iter_begin(it, dir))
	{
		for (;;)
		{
			Memory_Checkpoint tmp_mem = m_checkpoint_begin(scratch.arena);
			File_Info info = mplayer->os.file_iter_next(it, tmp_mem.arena);
			if (!has_flag(info.flags, FileFlag_Valid))
			{
				break;
			}
			
			if (!has_flag(info.flags, FileFlag_Directory))
			{
				if (str8_ends_with(info.name, str8_lit(".jpg"), MatchFlag_CaseInsensitive)  ||
					str8_ends_with(info.name, str8_lit(".jpeg"), MatchFlag_CaseInsensitive) ||
					str8_ends_with(info.name, str8_lit(".png"), MatchFlag_CaseInsensitive)
				)
				{
					result = str8_f(arena, "%.*s/%.*s", STR8_EXPAND(dir), STR8_EXPAND(info.name));
					break;
				}
			}
		}
		
		mplayer->os.file_iter_end(it);
	}
	return result;
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
					Mplayer_Item *music_track = mplayer_make_track_item(mplayer);
					mplayer_push_music_track(mplayer, music_track);
					music_track->path = str8_f(&music_track->arena, "%.*s/%.*s", STR8_EXPAND(library_path), STR8_EXPAND(info.name));
					
					Buffer tmp_file_block = arena_push_buffer(scratch.arena, megabytes(1));
					File_Handle *file = platform->open_file(music_track->path, make_flag(File_Open_Read));
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
								music_track->title = str8_copy(&music_track->arena, value);
							}
							else if (str8_match(key, str8_lit("album"), MatchFlag_CaseInsensitive))
							{
								music_track->album = str8_copy(&music_track->arena, value);
							}
							else if (str8_match(key, str8_lit("artist"), MatchFlag_CaseInsensitive))
							{
								music_track->artist = str8_copy(&music_track->arena, value);
							}
							else if (str8_match(key, str8_lit("genre"), MatchFlag_CaseInsensitive))
							{
								music_track->genre = str8_copy(&music_track->arena, value);
							}
							else if (str8_match(key, str8_lit("data"), MatchFlag_CaseInsensitive))
							{
								music_track->date = str8_copy(&music_track->arena, value);
							}
							else if (str8_match(key, str8_lit("tracknumber"), MatchFlag_CaseInsensitive))
							{
								music_track->track_number = str8_copy(&music_track->arena, value);
							}
						}
					}
					
					if (!music_track->title.len)
					{
						music_track->title = str8_copy(&music_track->arena, info.name);
					}
					
					// TODO(fakhri): use parent directory name as album name if the track doesn't contain an album name
					
					Mplayer_Item *artist = mplayer_find_or_create_artist(mplayer, music_track->artist);
					QueuePush_Next(artist->tracks.first, artist->tracks.last, music_track, next_links[Links_Artist]);
					artist->tracks.count += 1;
					music_track->artist_ref = artist;
					
					Mplayer_Item *album = mplayer_find_or_create_album(mplayer, artist, music_track->album);
					QueuePush_Next(album->tracks.first, album->tracks.last, music_track, next_links[Links_Album]);
					album->tracks.count += 1;
					
					music_track->album_ref = album;
					
					if (!album->image.texture_data.size)
					{
						album->image.texture = ZERO_STRUCT;
						if (tmp_flac_stream.front_cover)
						{
							Flac_Picture *front_cover = tmp_flac_stream.front_cover;
							
							album->image.in_disk = false;
							album->image.texture_data = clone_buffer(&album->arena, front_cover->buffer);
						}
						else
						{
							String8 cover_file_path = mplayer_attempt_find_cover_image_in_dir(mplayer, &album->arena, library_path);
							if (cover_file_path.len)
							{
								album->image.in_disk = true;
								album->image.path = cover_file_path;
							}
						}
					}
				}
			}
		}
		
		mplayer->os.file_iter_end(it);
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
				assert(mplayer->mode_stack->item);
				assert(mplayer->mode_stack->item->kind == Item_Kind_Artist);
				
				mplayer->selected_artist = mplayer->mode_stack->item;
			} break;
			case MODE_Album_Tracks:
			{
				assert(mplayer->mode_stack->item);
				assert(mplayer->mode_stack->item->kind == Item_Kind_Album);
				
				mplayer->selected_album = mplayer->mode_stack->item;
			} break;
			
			case MODE_Lyrics: break;
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
				assert(mplayer->mode_stack->item);
				assert(mplayer->mode_stack->item->kind == Item_Kind_Artist);
				
				mplayer->selected_artist = mplayer->mode_stack->item;
			} break;
			case MODE_Album_Tracks:
			{
				assert(mplayer->mode_stack->item);
				assert(mplayer->mode_stack->item->kind == Item_Kind_Album);
				
				mplayer->selected_album = mplayer->mode_stack->item;
			} break;
			
			case MODE_Lyrics: break;
		}
	}
}

internal void
mplayer_change_mode(Mplayer_Context *mplayer, Mplayer_Mode new_mode, Mplayer_Item *item)
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
	new_mode_node->item = item;
	
	new_mode_node->prev = mplayer->mode_stack;
	if (mplayer->mode_stack)
	{
		mplayer->mode_stack->next = new_mode_node;
	}
	mplayer->mode_stack = new_mode_node;
	
	mplayer->mode_stack->view_target_scroll = 0;
	mplayer->mode_stack->view_scroll = 0;
	
	mplayer->ui.active_widget_id = 0;
	mplayer->ui.hot_widget_id = 0;
}

internal void
mplayer_initialize(Mplayer_Context *mplayer)
{
	load_font(mplayer, &mplayer->font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->debug_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->big_debug_font, str8_lit("data/fonts/arial.ttf"), 50);
	load_font(mplayer, &mplayer->timestamp_font, str8_lit("data/fonts/arial.ttf"), 20);
	load_font(mplayer, &mplayer->header_label_font, str8_lit("data/fonts/arial.ttf"), 40);
	mplayer->play_track = 0;
	mplayer->volume = 1.0f;
	
	mplayer_load_library(mplayer, mplayer->library_path);
	mplayer_change_mode(mplayer, MODE_Artist_Library, 0);
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
		Memory_Checkpoint scratch = get_scratch(0, 0);
		draw_text(&group, &mplayer->debug_font, fps_pos, vec4(0.5f, 0.6f, 0.6f, 1), 
			fancy_str8(str8_f(scratch.arena, "%d", u32(1.0f / mplayer->input.frame_dt))),
			Text_Render_Flag_Centered_Bit);
	}
	
	if (mplayer->current_music && !is_music_track_still_playing(mplayer->current_music) && mplayer->current_music->next_links[Links_Library])
	{
		mplayer_play_music_track(mplayer, mplayer->current_music->next_links[Links_Library]);
	}
	
	if (mplayer_button_clicked(&mplayer->input, Key_Mouse_M4))
	{
		mplayer_change_previous_mode(mplayer);
	}
	
	if (mplayer_button_clicked(&mplayer->input, Key_Mouse_M5))
	{
		mplayer_change_next_mode(mplayer);
	}
	
	ui_begin(&mplayer->ui, &group, &mplayer->input, world_mouse_p);
	
	Range2_F32 available_space = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	// NOTE(fakhri): track control
	{
		Mplayer_Item *current_music = mplayer->current_music;
		assert(!current_music || current_music->kind == Item_Kind_Track);
		
		Range2_F32_Cut cut = range_cut_bottom(available_space, 125);
		available_space = cut.top;
		
		V4_F32 track_control_bg_color = vec4(0.05f, 0.05f, 0.05f, 1.0f);
		push_rect(&group, range_center(cut.bottom), range_dim(cut.bottom), track_control_bg_color);
		
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
				
				Memory_Checkpoint scratch = get_scratch(0, 0);
				String8 timestamp_string = str8_f(scratch.arena, "%.2d:%.2d:%.2d", current_timestamp.hours, current_timestamp.minutes, current_timestamp.seconds);
				draw_text(&group, &mplayer->timestamp_font, range_center(timestamp_rect), vec4(1, 1, 1, 1), 
					fancy_str8(timestamp_string),
					Text_Render_Flag_Centered_Bit);
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
				Range2_F32_Cut cut2 = range_cut_left(cut.left, 100);
				Range2_F32 cover_rect      = cut2.left;
				Range2_F32 track_name_rect = cut2.right;
				
				if (mplayer_is_valid_image(current_music->image))
				{
					mpalyer_draw_item_image(&group, mplayer, current_music, range_center(cover_rect), range_dim(cover_rect), vec4(1,1,1,1));
				}
				else if (current_music->album_ref && mplayer_is_valid_image(current_music->album_ref->image))
				{
					mpalyer_draw_item_image(&group, mplayer, current_music->album_ref, range_center(cover_rect), range_dim(cover_rect), vec4(1,1,1,1));
				}
				
				// TODO(fakhri): stack of render configs?
				render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, track_name_rect);
				
				Mplayer_UI_Interaction interaction = _ui_widget_interaction(&mplayer->ui, __LINE__, range_center(track_name_rect), range_dim(track_name_rect));
				V4_F32 title_rect_color = track_control_bg_color;
				if (interaction.hover)
				{
					title_rect_color = vec4(0.1f, 0.1f, 0.1f, 1.0f);
				}
				
				if (interaction.pressed)
				{
					title_rect_color = vec4(0.0f, 0.f, 0.f, 1.0f);
				}
				
				if (interaction.clicked)
				{
					if (mplayer->current_music->album_ref)
					{
						mplayer->selected_album = mplayer->current_music->album_ref;
						mplayer_change_mode(mplayer, MODE_Album_Tracks, mplayer->selected_album);
					}
				}
				
				V2_F32 track_name_center = range_center(track_name_rect); 
				V2_F32 track_name_dim = range_dim(track_name_rect); 
				push_rect(&group, track_name_center, track_name_dim, title_rect_color);
				
				V2_F32 text_pos = vec2(5 + track_name_rect.min_x, track_name_center.y);
				f32 text_width = font_compute_text_dim(&mplayer->font, mplayer->current_music->title).width;
				
				if (interaction.hover && text_width > track_name_dim.width)
				{
					mplayer->track_name_hover_t += mplayer->input.frame_dt;
					
					text_pos.x -= 10 * PI32 * mplayer->track_name_hover_t;
					draw_text(&group, &mplayer->font, text_pos, vec4(1, 1, 1, 1), fancy_str8(mplayer->current_music->title));
					text_pos.x += text_width + 10;
				}
				else
				{
					mplayer->track_name_hover_t = 0;
				}
				
				draw_text(&group, &mplayer->font, text_pos, vec4(1, 1, 1, 1), fancy_str8(mplayer->current_music->title));
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
		
		// NOTE(fakhri):  button  
		cut = range_cut_top(cut.bottom, 20);
		{
			cut = range_cut_top(cut.bottom, 20);
			if (ui_button(&mplayer->ui, &mplayer->font, str8_lit("Lyrics"), range_center(cut.top)).clicked)
			{
				mplayer_change_mode(mplayer, MODE_Lyrics, 0);
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
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(artist_label), Text_Render_Flag_Centered_Bit);
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			Mplayer_Item *selected_artist = ui_items_list(mplayer, &mplayer->ui, mplayer->artists, 
				vec2(350, 300), available_space, Links_Library, vec2(10, 10));
			if (selected_artist)
			{
				mplayer->selected_artist = selected_artist;
				mplayer_change_mode(mplayer, MODE_Artist_Albums, selected_artist);
			}
			
		} break;
		case MODE_Artist_Albums:
		{
			assert(mplayer->selected_artist);
			
			f32 header_height = 65;
			Range2_F32_Cut cut = range_cut_top(available_space, header_height);
			
			// NOTE(fakhri): header
			{
				Range2_F32 header = cut.top;
				push_rect(&group, range_center(header), range_dim(header), header_bg_color);
				Range2_F32_Cut cut2 = range_cut_left(header, 50); // padding
				
				cut2 = range_cut_left(cut2.right, 500);
				draw_text_left_side_fixed_width(&group, &mplayer->header_label_font, cut2.left, vec4(1, 1, 1, 1), mplayer->selected_artist->artist);
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			Mplayer_Item *selected_album = ui_items_list(mplayer, &mplayer->ui, mplayer->selected_artist->albums, 
				vec2(350, 300), available_space, Links_Artist, vec2(10, 10));
			if (selected_album)
			{
				mplayer->selected_album = selected_album;
				mplayer_change_mode(mplayer, MODE_Album_Tracks, selected_album);
			}
		} break;
		
		case MODE_Album_Tracks:
		{
			assert(mplayer->selected_album);
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
					
					mpalyer_draw_item_image(&group, mplayer, mplayer->selected_album, 
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
						draw_text_left_side_fixed_width(&group, &mplayer->header_label_font, cut3.top, vec4(1, 1, 1, 1), mplayer->selected_album->album);
					}
					
					cut3 = range_cut_top(cut3.bottom, 20); // padding
					// NOTE(fakhri): artist name
					{
						Mplayer_Font *artist_name_font = &mplayer->font;
						
						cut3 = range_cut_top(cut3.bottom, 30); // Album name
						
						V4_F32 text_color = vec4(0.4f, 0.4f, 0.4f, 1);
						b32 should_underline = 0;
						f32 underline_thickness = 0;
						
						String8 artist_name = mplayer->selected_album->artist;
						
						// NOTE(fakhri): artist button
						{
							V2_F32 artist_name_dim = font_compute_text_dim(artist_name_font, artist_name);
							
							V2_F32 button_dim = range_dim(cut3.top);
							button_dim.width = MIN(button_dim.width, artist_name_dim.width);
							
							V2_F32 button_pos = vec2(cut3.top.min_x + 0.5f * button_dim.width, range_center(cut3.top).y - 0.25f * artist_name_dim.height);
							
							Mplayer_UI_Interaction interaction = ui_widget_interaction(&mplayer->ui, button_pos, button_dim);
							
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
								assert(mplayer->selected_album->artist_ref);
								mplayer->selected_artist = mplayer->selected_album->artist_ref;
								mplayer_change_mode(mplayer, MODE_Artist_Albums, mplayer->selected_album->artist_ref);
							}
						}
						
						draw_text_left_side_fixed_width(&group, artist_name_font, cut3.top, text_color, artist_name, should_underline, underline_thickness);
						
					}
					
				}
			}
			
			available_space = cut.bottom;
			render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			
			Mplayer_Item *selected_track = ui_items_list(mplayer, &mplayer->ui, mplayer->selected_album->tracks, 
				vec2(available_space_dim.width, 50), available_space, Links_Album);
			mplayer_play_music_track(mplayer, selected_track);
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
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(album_label), Text_Render_Flag_Centered_Bit);
				
				// TODO(fakhri): display the number of albums and tracks the selected artist have
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			Mplayer_Item *selected_album = ui_items_list(mplayer, &mplayer->ui, mplayer->albums, 
				vec2(350, 300), available_space, Links_Library, vec2(10, 10));
			if (selected_album)
			{
				mplayer->selected_album = selected_album;
				mplayer_change_mode(mplayer, MODE_Album_Tracks, selected_album);
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
				draw_text(&group, &mplayer->header_label_font, range_center(label_rect), vec4(1, 1, 1, 1), fancy_str8(songs_label), Text_Render_Flag_Centered_Bit);
			}
			
			Range2_F32 library_header = cut.top;
			available_space = cut.bottom;
			render_group_update_config(&group, group.config.camera_pos, group.config.camera_dim, available_space);
			V2_F32 available_space_dim = range_dim(available_space);
			
			Mplayer_Item *selected_track = ui_items_list(mplayer, &mplayer->ui, mplayer->tracks, 
				vec2(available_space_dim.width, 50), available_space, Links_Library);
			mplayer_play_music_track(mplayer, selected_track);
		} break;
		
		case MODE_Lyrics:
		{
			draw_text(&group, &mplayer->font, range_center(available_space), vec4(1, 1, 1, 1), fancy_str8_lit("Lyrics not available"), Text_Render_Flag_Centered_Bit);
		} break;
	}
	
	// NOTE(fakhri): animate scroll
	if (mplayer->mode_stack)
	{
		f32 dt = mplayer->input.frame_dt;
		f32 current_scroll = mplayer->mode_stack->view_scroll;
		f32 target_scroll  = mplayer->mode_stack->view_target_scroll;
		f32 scroll_speed  = mplayer->mode_stack->view_scroll_speed;
		
		f32 freq = 5.0f;
		f32 zeta = 1.f;
		
		f32 K1 = zeta / (PI32 * freq);
		f32 K2 = 1.0f / SQUARE(2 * PI32 * freq);
		
		f32 scroll_accel = (1.0f / K2) * (target_scroll - current_scroll - K1 * scroll_speed);
		mplayer->mode_stack->view_scroll_speed += dt * scroll_accel;
		mplayer->mode_stack->view_scroll += dt * mplayer->mode_stack->view_scroll_speed + 0.5f * SQUARE(dt) * scroll_accel;
	}
}
