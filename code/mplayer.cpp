
#include <time.h>


#include "mplayer.h"
#include "mplayer_ui.h"

#include "mplayer_input.cpp"

#include "mplayer_renderer.h"
#include "mplayer_bitstream.h"
#include "mplayer_flac.h"

#include "mplayer_font.h"

enum Image_Data_State
{
	Image_State_Unloaded,
	Image_State_Loading,
	Image_State_Loaded,
	Image_State_Invalid,
};

struct Mplayer_Image_ID
{
	u32 index;
};

struct Mplayer_Item_Image
{
	Image_Data_State state;
	b32 in_disk;
	String8 path;
	Buffer texture_data;
	// NOTE(fakhri): for cover and artist picture if we have it
	Texture texture;
};

struct Mplayer_Track_ID{u64 v[2];};
struct Mplayer_Album_ID{u64 v[2];};
struct Mplayer_Artist_ID{u64 v[2];};
struct Mplayer_Playlist_ID{u64 v[2];};

#define NULL_TRACK_ID Mplayer_Track_ID{}
#define NULL_ALBUM_ID Mplayer_Album_ID{}
#define NULL_ARTIST_ID Mplayer_Artist_ID{}
#define NULL_PLAYLIST_ID Mplayer_Playlist_ID{}

struct Seek_Table_Work_Data
{
	volatile b32 cancel_req;
	volatile b32 running;
	u32 seek_points_count;
	Flac_Seek_Point *seek_table;
	Flac_Stream *flac_stream;
	u64 samples_count;
	u64 first_block_offset;
	Bit_Stream bitstream;
	u8 nb_channels;
	u8 bits_per_sample;
};

struct Mplayer_Track
{
	// NOTE(fakhri): album links
	Mplayer_Track *next_album;
	Mplayer_Track *prev_album;
	
	// NOTE(fakhri): artist links
	Mplayer_Track *next_artist;
	Mplayer_Track *prev_artist;
	
	// NOTE(fakhri): hash table links
	Mplayer_Track *next_hash;
	Mplayer_Track *prev_hash;
	Mplayer_Track_ID hash;
	
	Mplayer_Image_ID image_id;
	
	String8 path;
	String8 title;
	String8 album;
	String8 artist;
	String8 genre;
	String8 date;
	String8 track_number;
	
	Mplayer_Artist_ID artist_id;
	Mplayer_Album_ID album_id;
	
	Flac_Stream *flac_stream;
	b32 file_loaded;
	Buffer flac_file_buffer;
	
	Seek_Table_Work_Data build_seektable_work_data;
	
	// NOTE(fakhri): tracks can be parts of a file
	u64 start_sample_offset;
	u64 end_sample_offset;
};

struct Mplayer_Track_List
{
	Mplayer_Track *first;
	Mplayer_Track *last;
	u32 count;
};

struct Mplayer_Album
{
	// NOTE(fakhri): artist links
	Mplayer_Album *next_artist;
	Mplayer_Album *prev_artist;
	
	// NOTE(fakhri): hash table links
	Mplayer_Album *next_hash;
	Mplayer_Album *prev_hash;
	Mplayer_Album_ID hash;
	
	Mplayer_Image_ID image_id;
	
	String8 name;
	String8 artist;
	
	Mplayer_Artist_ID artist_id;
	
	Mplayer_Track_List tracks;
};
struct Mplayer_Album_List
{
	Mplayer_Album *first;
	Mplayer_Album *last;
	u32 count;
};

struct Mplayer_Artist
{
	// NOTE(fakhri): hash table links
	Mplayer_Artist *next_hash;
	Mplayer_Artist *prev_hash;
	Mplayer_Artist_ID hash;
	
	Mplayer_Image_ID image_id;
	
	String8 name;
	
	Mplayer_Track_List tracks;
	Mplayer_Album_List albums;
};
struct Mplayer_Artist_List
{
	Mplayer_Artist *first;
	Mplayer_Artist *last;
	u32 count;
};

struct Mplayer_Track_ID_Entry
{
	Mplayer_Track_ID_Entry *next;
	Mplayer_Track_ID_Entry *prev;
	Mplayer_Track_ID track_id;
};
struct Mplayer_Track_ID_List
{
	Mplayer_Track_ID_Entry *first;
	Mplayer_Track_ID_Entry *last;
	u32 count;
};
struct Mplayer_Playlist
{
	Mplayer_Playlist *next_hash;
	Mplayer_Playlist *prev_hash;
	Mplayer_Playlist_ID id;
	String8 name;
	Mplayer_Track_ID_List tracks;
};
struct Mplayer_Playlist_List
{
	Mplayer_Playlist *first;
	Mplayer_Playlist *last;
	u32 count;
};

#define MAX_ARTISTS_COUNT 512
#define MAX_ALBUMS_COUNT 1024
#define MAX_TRACKS_COUNT 65536
#define MAX_PLAYLISTS_COUNT 1024

struct Mplayer_Playlists
{
	Memory_Arena arena;
	u32 playlists_count;
	
	Mplayer_Track_ID_List fav_tracks;
	Mplayer_Playlist_List playlists_table[1024];
	Mplayer_Playlist_ID  playlist_ids[MAX_PLAYLISTS_COUNT];
	
	Mplayer_Track_ID_Entry *track_id_entries_free_list;
};

struct Mplayer_Library
{
	Memory_Arena arena;
	Memory_Arena track_transient_arena;
	
	u32 tracks_count;
	u32 albums_count;
	u32 artists_count;
	
	Mplayer_Track_List tracks_table[1024];
	Mplayer_Album_List albums_table[1024];
	Mplayer_Artist_List artists_table[1024];
	
	
	Mplayer_Artist_ID artist_ids[MAX_ARTISTS_COUNT];
	Mplayer_Album_ID  album_ids[MAX_ALBUMS_COUNT];
	Mplayer_Track_ID  track_ids[MAX_TRACKS_COUNT];
	
	Mplayer_Item_Image images[8192];
	u32 images_count;
};

struct Mplayer_Library_Location
{
	u8 path[512];
	u32 path_len;
};

#define MAX_LOCATION_COUNT 64
struct MPlayer_Settings
{
	u32 version;
	u32 locations_count;
	Mplayer_Library_Location locations[MAX_LOCATION_COUNT];
};

#define DEFAUTL_SETTINGS MPlayer_Settings{.version = 1, .locations_count = 0, .locations = {}}

enum Mplayer_Mode
{
	MODE_Track_Library,
	MODE_Artist_Library,
	MODE_Album_Library,
	
	MODE_Artist_Albums,
	MODE_Album_Tracks,
	
	MODE_Queue,
	MODE_Favorites,
	MODE_Playlists,
	MODE_Playlist_Tracks,
	
	MODE_Lyrics,
	MODE_Settings,
};

union Mplayer_Item_ID
{
	Mplayer_Track_ID  track_id;
	Mplayer_Album_ID  album_id;
	Mplayer_Artist_ID artist_id;
	Mplayer_Playlist_ID playlist_id;
};

struct Mplayer_Mode_Stack
{
	Mplayer_Mode_Stack *next;
	Mplayer_Mode_Stack *prev;
	
	Mplayer_Mode mode;
	Mplayer_Item_ID id;
};

struct Mplayer_Path_Lister
{
	Memory_Arena arena;
	String8 user_path; // what the user typed
	u8 user_path_buffer[512];
	
	String8 base_dir;
	Directory dir;
	
	String8 *filtered_paths;
	u32 filtered_paths_count;
};

typedef u16 Mplayer_Queue_Index;

struct Mplayer_Queue
{
	b32 playing;
	Mplayer_Queue_Index current_index;
	u16 count;
	Mplayer_Track_ID tracks[65536];
};

enum Mplayer_Ctx_Menu
{
	Track_Context_Menu,
	Queue_Track_Context_Menu,
	Context_Menu_COUNT,
};

enum Mplayer_Modal_Menu
{
	MODAL_Path_Lister,
	MODAL_Add_To_Playlist,
	MODAL_Create_Playlist,
	Modal_Menu_COUNT,
};

struct Context_Menu_Data
{
	Mplayer_Item_ID item_id;
	Mplayer_Queue_Index queue_index;
};

struct Mplayer_Context
{
	Memory_Arena main_arena;
	Memory_Arena *frame_arena;
	Render_Context *render_ctx;
	struct Mplayer_UI *ui;
	Fonts_Context *fonts_ctx;
	Random_Generator entropy;
	
	Font *font;
	
	f32 time_since_last_play;
	f32 volume;
	
	Mplayer_Mode_Stack *mode_stack;
	Mplayer_Mode_Stack *mode_stack_free_list_first;
	Mplayer_Mode_Stack *mode_stack_free_list_last;
	
	Mplayer_Playlists playlists;
	Mplayer_Library library;
	
	f32 track_name_hover_t;
	
	Mplayer_Path_Lister path_lister;
	
	Mplayer_Queue queue;
	
	b32 show_library_locations;
	MPlayer_Settings settings;
	
	UI_ID modal_menu_ids[Modal_Menu_COUNT];
	
	UI_ID ctx_menu_ids[Context_Menu_COUNT];
	Context_Menu_Data ctx_menu_data;
	
	b32 add_track_to_new_playlist;
	Mplayer_Track_ID track_to_add_to_playlist;
	u8 new_playlist_name_buffer[256];
	String8 new_playlist_name;
	
	// NOTE(fakhri): UI List/Grid data
	// {
	u32 list_items_count;
	u32 list_item_index;
	u32 list_first_visible_row;
	u32 list_max_rows_count;
	u32 list_row_index;
	u32 list_visible_rows_count;
	f32 list_vpadding;
	f32 list_item_height;
	
	u32 grid_item_index;
	u32 grid_items_count;
	u32 grid_first_visible_row;
	u32 grid_item_count_per_row;
	u32 grid_max_rows_count;
	u32 grid_visible_rows_count;
	u32 grid_item_index_in_row;
	i32 grid_row_index;
	f32 grid_vpadding;
	V2_F32 grid_item_dim;
	// }
};


#define STBI_ASSERT(x) assert(x)
#define STBI_SSE2
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include "mplayer_renderer.cpp"
#include "mplayer_bitstream.cpp"
#include "mplayer_flac.cpp"

#include "mplayer_byte_stream.h"

#include "third_party/meow_hash_x64_aesni.h"

global Mplayer_Input *g_input;
global Mplayer_Context *mplayer_ctx;

//~ NOTE(fakhri): Animation timer stuff
internal void
mplayer_animate_next_frame()
{
	g_input->next_animation_timer_request = 0;
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

internal b32
is_valid(Mplayer_Track_ID id)
{
	b32 result = id.v[0] && id.v[1];
	return result;
}

internal b32
is_equal(Mplayer_Track_ID id1, Mplayer_Track_ID id2)
{
	b32 result = id1.v[0] == id2.v[0] && id1.v[1] == id2.v[1];
	return result;
}
internal b32
is_equal(Mplayer_Album_ID id1, Mplayer_Album_ID id2)
{
	b32 result = id1.v[0] == id2.v[0] && id1.v[1] == id2.v[1];
	return result;
}
internal b32
is_equal(Mplayer_Artist_ID id1, Mplayer_Artist_ID id2)
{
	b32 result = id1.v[0] == id2.v[0] && id1.v[1] == id2.v[1];
	return result;
}
internal b32
is_equal(Mplayer_Playlist_ID id1, Mplayer_Playlist_ID id2)
{
	b32 result = id1.v[0] == id2.v[0] && id1.v[1] == id2.v[1];
	return result;
}

internal Mplayer_Item_ID
to_item_id(Mplayer_Artist_ID id)
{
	Mplayer_Item_ID item_id;
	item_id.artist_id = id;
	return item_id;
}
internal Mplayer_Item_ID
to_item_id(Mplayer_Track_ID id)
{
	Mplayer_Item_ID item_id;
	item_id.track_id = id;
	return item_id;
}
internal Mplayer_Item_ID
to_item_id(Mplayer_Album_ID id)
{
	Mplayer_Item_ID item_id;
	item_id.album_id = id;
	return item_id;
}
internal Mplayer_Item_ID
to_item_id(Mplayer_Playlist_ID id)
{
	Mplayer_Item_ID item_id;
	item_id.playlist_id = id;
	return item_id;
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
mplayer_track_by_id(Mplayer_Library *library, Mplayer_Track_ID id)
{
	u32 slot_index = id.v[1] % array_count(library->tracks_table);
	
	Mplayer_Track *track = 0;
	for (Mplayer_Track *entry = library->tracks_table[slot_index].first; entry; entry = entry->next_hash)
	{
		if (is_equal(entry->hash, id))
		{
			track = entry;
			break;
		}
	}
	
	return track;
}

internal Mplayer_Album *
mplayer_album_by_id(Mplayer_Library *library, Mplayer_Album_ID id)
{
	u32 slot_index = id.v[1] % array_count(library->albums_table);
	
	Mplayer_Album *album = 0;
	for (Mplayer_Album *entry = library->albums_table[slot_index].first; entry; entry = entry->next_hash)
	{
		if (is_equal(entry->hash, id))
		{
			album = entry;
			break;
		}
	}
	
	return album;
}

internal Mplayer_Artist *
mplayer_artist_by_id(Mplayer_Library *library, Mplayer_Artist_ID id)
{
	u32 slot_index = id.v[1] % array_count(library->artists_table);
	
	Mplayer_Artist *artist = 0;
	for (Mplayer_Artist *entry = library->artists_table[slot_index].first; entry; entry = entry->next_hash)
	{
		if (is_equal(entry->hash, id))
		{
			artist = entry;
			break;
		}
	}
	
	return artist;
}

internal Mplayer_Playlist_ID
mplayer_compute_playlist_id(String8 playlist_name)
{
	Mplayer_Playlist_ID id = NULL_PLAYLIST_ID;
	
	meow_u128 meow_hash = MeowHash(MeowDefaultSeed, playlist_name.len, playlist_name.str);
	memory_copy(&id, &meow_hash, sizeof(meow_hash));
	
	return id;
}

internal Mplayer_Playlist *
mplayer_playlist_by_id(Mplayer_Playlists *playlists, Mplayer_Playlist_ID id)
{
	u32 slot_index = id.v[1] % array_count(playlists->playlists_table);
	
	Mplayer_Playlist *playlist = 0;
	for (Mplayer_Playlist *entry = playlists->playlists_table[slot_index].first; entry; entry = entry->next_hash)
	{
		if (is_equal(entry->id, id))
		{
			playlist = entry;
			break;
		}
	}
	
	return playlist;
}

internal Mplayer_Track_ID
mplayer_compute_track_id(String8 track_path, u64 samples_offset, u64 samples_end)
{
	Mplayer_Track_ID id = NULL_TRACK_ID;
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	
	Buffer buffer = arena_push_buffer(scratch.arena, track_path.len + sizeof(samples_offset) + sizeof(samples_end));
	((u64*)buffer.data)[0] = samples_offset;
	((u64*)buffer.data)[1] = samples_end;
	memory_copy(buffer.data + 2 * sizeof(u64), track_path.str, track_path.len);
	
	meow_u128 meow_hash = MeowHash(MeowDefaultSeed, buffer.size, buffer.data);
	memory_copy(&id, &meow_hash, sizeof(meow_hash));
	
	return id;
}

internal Mplayer_Album_ID
mplayer_compute_album_id(String8 artist_name, String8 album_name)
{
	Mplayer_Album_ID id = NULL_ALBUM_ID;
	
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	
	String8 data = str8_f(scratch.arena, "%.*s%.*s", STR8_EXPAND(artist_name), STR8_EXPAND(album_name));
	meow_u128 meow_hash = MeowHash(MeowDefaultSeed, data.len, data.str);
	memory_copy(&id, &meow_hash, sizeof(meow_hash));
	
	return id;
}

internal Mplayer_Artist_ID
mplayer_compute_artist_id(String8 artist_name)
{
	Mplayer_Artist_ID id = NULL_ARTIST_ID;
	
	meow_u128 meow_hash = MeowHash(MeowDefaultSeed, artist_name.len, artist_name.str);
	memory_copy(&id, &meow_hash, sizeof(meow_hash));
	
	return id;
}

internal void
mplayer_insert_track(Mplayer_Library *library, Mplayer_Track *track)
{
	assert(!is_equal(track->hash, NULL_TRACK_ID));
	Mplayer_Track_ID id = track->hash;
	
	// NOTE(fakhri): sanity check that we don't have another track
	// with the same id
	#if DEBUG_BUILD
	{
		Mplayer_Track *test_track = mplayer_track_by_id(library, id);
		assert(test_track == 0);
	}
	#endif
		
		u32 slot_index = id.v[1] % array_count(library->tracks_table);
	DLLPushBack_NP(library->tracks_table[slot_index].first, library->tracks_table[slot_index].last, 
		track, 
		next_hash, prev_hash);
	
	library->tracks_table[slot_index].count += 1;
	
	assert(library->tracks_count < array_count(library->track_ids));
	library->track_ids[library->tracks_count++] = id;
}

internal void
mplayer_insert_album(Mplayer_Library *library, Mplayer_Album *album)
{
	assert(!is_equal(album->hash, NULL_ALBUM_ID));
	Mplayer_Album_ID id = album->hash;
	
	// NOTE(fakhri): sanity check that we don't have another album
	// with the same id
	#if DEBUG_BUILD
	{
		Mplayer_Album *test_album = mplayer_album_by_id(library, id);
		assert(test_album == 0);
	}
	#endif
		
		u32 slot_index = id.v[1] % array_count(library->albums_table);
	DLLPushBack_NP(library->albums_table[slot_index].first, library->albums_table[slot_index].last,
		album, 
		next_hash, prev_hash);
	
	library->albums_table[slot_index].count += 1;
	
	assert(library->albums_count < array_count(library->album_ids));
	library->album_ids[library->albums_count++] = id;
}

internal void
mplayer_insert_artist(Mplayer_Library *library, Mplayer_Artist *artist)
{
	assert(!is_equal(artist->hash, NULL_ARTIST_ID));
	Mplayer_Artist_ID id = artist->hash;
	
	// NOTE(fakhri): sanity check that we don't have another album
	// with the same id
	#if DEBUG_BUILD
	{
		Mplayer_Artist *test_artist = mplayer_artist_by_id(library, id);
		assert(test_artist == 0);
	}
	#endif
		
		u32 slot_index = id.v[1] % array_count(library->artists_table);
	DLLPushBack_NP(library->artists_table[slot_index].first, library->artists_table[slot_index].last,
		artist, 
		next_hash, prev_hash);
	
	library->artists_table[slot_index].count += 1;
	
	assert(library->artists_count < array_count(library->artist_ids));
	library->artist_ids[library->artists_count++] = id;
}


internal void
mplayer_add_playlist(Mplayer_Playlists *playlists, Mplayer_Playlist *playlist)
{
	assert(!is_equal(playlist->id, NULL_PLAYLIST_ID));
	Mplayer_Playlist_ID id = playlist->id;
	
	// NOTE(fakhri): sanity check that we don't have another playlist
	// with the same id
	#if DEBUG_BUILD
	{
		Mplayer_Playlist *test_playlist = mplayer_playlist_by_id(playlists, id);
		assert(test_playlist == 0);
	}
	#endif
		
		u32 slot_index = id.v[1] % array_count(playlists->playlists_table);
	DLLPushBack_NP(playlists->playlists_table[slot_index].first, playlists->playlists_table[slot_index].last, 
		playlist, 
		next_hash, prev_hash);
	
	playlists->playlists_table[slot_index].count += 1;
	
	assert(playlists->playlists_count < array_count(playlists->playlist_ids));
	playlists->playlist_ids[playlists->playlists_count++] = id;
}

//~ NOTE(fakhri): Mplayer Image stuff

struct Decode_Texture_Data_Input
{
	Memory_Arena work_arena;
	Mplayer_Item_Image *image;
};

internal Decode_Texture_Data_Input *
mplayer_make_texure_upload_data(Mplayer_Item_Image *image)
{
	Decode_Texture_Data_Input *result = m_arena_bootstrap_push(Decode_Texture_Data_Input, work_arena);
	
	assert(result);
	result->image   = image;
	
	return result;
}

WORK_SIG(decode_texture_data_work)
{
	assert(input);
	Decode_Texture_Data_Input *upload_data = (Decode_Texture_Data_Input *)input;
	
	Mplayer_Item_Image *image = upload_data->image;
	Render_Context *render_ctx = mplayer_ctx->render_ctx;
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
				set_flag(image->texture.flags, TEXTURE_FLAG_SRGB_BIT);
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
mplayer_load_item_image(Mplayer_Item_Image *image)
{
	if (!is_texture_valid(image->texture))
	{
		image->texture = reserve_texture_handle(mplayer_ctx->render_ctx);
	}
	
	if (image->state == Image_State_Unloaded)
	{
		image->state = Image_State_Loading;
		
		platform->push_work(decode_texture_data_work, 
			mplayer_make_texure_upload_data(image));
	}
}

internal Mplayer_Item_Image *
mplayer_get_image_by_id(Mplayer_Image_ID image_id, b32 load)
{
	assert(image_id.index < array_count(mplayer_ctx->library.images));
	Mplayer_Item_Image *image = 0;
	
	image = mplayer_ctx->library.images + image_id.index;
	if (load)
	{
		mplayer_load_item_image(image);
	}
	
	return image;
}

//~ NOTE(fakhri): track control stuff

internal void
mplayer_track_reset(Mplayer_Track *track)
{
	if (track && track->flac_stream)
	{
		flac_seek_stream(track->flac_stream, track->start_sample_offset);
	}
}

internal void
mplayer_load_track(Mplayer_Track *track)
{
	if (!track) return;
	
	if (!track->file_loaded)
	{
		track->flac_file_buffer = platform->load_entire_file(track->path, &mplayer_ctx->library.track_transient_arena);
		track->file_loaded = true;
	}
	
	if (!track->flac_stream)
	{
		track->flac_stream = m_arena_push_struct_z(&mplayer_ctx->library.track_transient_arena, Flac_Stream);
		assert(init_flac_stream(track->flac_stream, &mplayer_ctx->library.track_transient_arena, track->flac_file_buffer));
		if (!track->flac_stream->seek_table)
		{
			flac_build_seek_table(track->flac_stream, &mplayer_ctx->library.track_transient_arena, &track->build_seektable_work_data);
		}
		
		if (!is_valid(track->image_id))
		{
			Flac_Picture *front_cover = track->flac_stream->front_cover;
			if (front_cover)
			{
				track->image_id = mplayer_reserve_image_id(&mplayer_ctx->library);
				Mplayer_Item_Image *cover_image = mplayer_get_image_by_id(track->image_id, 0);
				
				cover_image->in_disk = false;
				cover_image->texture_data = clone_buffer(&mplayer_ctx->library.arena, front_cover->buffer);
			}
			else
			{
				Mplayer_Album *album = mplayer_album_by_id(&mplayer_ctx->library, track->album_id);
				if (album)
				{
					track->image_id = album->image_id;
				}
			}
		}
	}
	
	mplayer_track_reset(track);
}

internal void
mplayer_unload_track(Mplayer_Track *track)
{
	if (!track) return;
	
	track->build_seektable_work_data.cancel_req = 1;
	for (;track->build_seektable_work_data.running;)
	{
		// NOTE(fakhri): help worker threads instead of just waiting
		platform->do_next_work();
	}
	uninit_flac_stream(track->flac_stream);
	m_arena_free_all(&mplayer_ctx->library.track_transient_arena);
	track->file_loaded = false;
	track->flac_stream = 0;
	track->flac_file_buffer = ZERO_STRUCT;
}

internal b32
is_track_still_playing(Mplayer_Track *track)
{
	b32 result = (track->flac_stream && 
		!bitstream_is_empty(&track->flac_stream->bitstream) && 
		track->flac_stream->next_sample_number < track->end_sample_offset);
	
	return result;
}

//~ NOTE(fakhri): Font stuff

#include "mplayer_font.cpp"

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
is_valid(Mplayer_Queue_Index index)
{
	b32 result = (index && (index < mplayer_ctx->queue.count) && 
		is_valid(mplayer_ctx->queue.tracks[index]));
	return result;
}

internal void
mplayer_queue_shuffle()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	Mplayer_Queue_Index current_new_index = queue->current_index;
	for (Mplayer_Queue_Index i = 1; i < queue->count; i += 1)
	{
		u16 swap_index = (u16)rng_next_minmax(&mplayer_ctx->entropy, i, queue->count);
		SWAP(Mplayer_Track_ID, queue->tracks[i], queue->tracks[swap_index]);
		
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
mplayer_queue_get_track_from_queue_index(Mplayer_Queue_Index index)
{
	assert(index < mplayer_ctx->queue.count);
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	Mplayer_Track *result = mplayer_track_by_id(&mplayer_ctx->library, queue->tracks[index]);
	return result;
}

internal Mplayer_Track *
mplayer_queue_get_current_track()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	
	Mplayer_Track *result = 0;
	if (is_valid(queue->current_index))
	{
		result = mplayer_queue_get_track_from_queue_index(queue->current_index);
	}
	return result;
}

internal void
mplayer_set_current(Mplayer_Queue_Index index)
{
	mplayer_ctx->time_since_last_play = g_input->time;
	
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	
	if (index >= queue->count) 
		index = 0;
	
	// TODO(fakhri): Since now we have tracks that point to 
	// parts of a file, when we play these track we seek
	// to wherever the start offset of the track is, if the file
	// that contains them doesn't have a seek table then when we
	// seek we rebuild the seektable, building the seektable
	// can be slow, which cause some files to have noticeable
	// delay when played...
	
	if (is_valid(queue->current_index))
	{
		mplayer_unload_track(mplayer_queue_get_current_track());
	}
	
	queue->current_index = index;
	
	if (is_valid(queue->current_index))
	{
		mplayer_load_track(mplayer_queue_get_current_track());
	}
}

internal void
mplayer_queue_reset_current()
{
	mplayer_set_current(mplayer_ctx->queue.current_index);
}

internal void
mplayer_clear_queue()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	
	mplayer_set_current(NULL_QUEUE_INDEX);
	queue->tracks[0] = NULL_TRACK_ID;
	queue->playing = 0;
	queue->count = 1;
}

internal void
mplayer_init_queue()
{
	memory_zero_struct(&mplayer_ctx->queue);
	mplayer_clear_queue();
}

internal void
mplayer_play_next_in_queue()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	
	mplayer_set_current(queue->current_index + 1);
}

internal void
mplayer_play_prev_in_queue()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	mplayer_set_current(queue->current_index - 1);
}


internal void
mplayer_queue_pause()
{
	mplayer_ctx->queue.playing = 0;
	mplayer_animate_next_frame();
}

internal void
mplayer_update_queue()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	if (!is_valid(queue->current_index))
	{
		mplayer_set_current(1);
	}
	
	if (queue->playing)
	{
		Mplayer_Track *current_track = mplayer_queue_get_current_track();
		if (current_track && !is_track_still_playing(current_track))
		{
			mplayer_play_next_in_queue();
		}
	}
	
	if (queue->count == 0)
	{
		mplayer_queue_pause();
	}
}

internal void
mplayer_queue_resume()
{
	mplayer_ctx->queue.playing = 1;
	mplayer_animate_next_frame();
}

internal void
mplayer_queue_toggle_play()
{
	if (mplayer_ctx->queue.playing)
	{
		mplayer_queue_pause();
	}
	else 
	{
		mplayer_queue_resume();
	}
}

internal b32
mplayer_is_queue_playing()
{
	b32 result = mplayer_ctx->queue.playing;
	return result;
}


internal void
mplayer_queue_remove_at(Mplayer_Queue_Index index)
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	if (!queue->count || index >= queue->count) return;
	
	if (index == queue->current_index && is_valid(index))
	{
		Mplayer_Track *track = mplayer_queue_get_track_from_queue_index(index);
		mplayer_unload_track(track);
	}
	
	if (index < queue->count - 1)
	{
		memory_move(queue->tracks + index, 
			queue->tracks + index + 1,
			sizeof(queue->tracks[0]) * (queue->count - index - 1));
		
	}
	
	queue->count -= 1;
	
	if (index == queue->current_index && is_valid(index))
	{
		Mplayer_Track *track = mplayer_queue_get_track_from_queue_index(index);
		mplayer_load_track(track);
	}
	
}

internal void
mplayer_queue_insert_at(Mplayer_Queue_Index index, Mplayer_Track_ID track_id)
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	
	if (index > queue->count)
	{
		index = queue->count;
	}
	
	if (queue->count < array_count(queue->tracks))
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
mplayer_queue_next(Mplayer_Track_ID track_id)
{
	if (!is_valid(track_id)) return;
	mplayer_queue_insert_at(mplayer_ctx->queue.current_index + 1, track_id);
}

internal void
mplayer_queue_last(Mplayer_Track_ID track_id)
{
	if (!is_valid(track_id)) return;
	mplayer_queue_insert_at(mplayer_ctx->queue.count, track_id);
}

internal void
mplayer_empty_queue_after_current()
{
	Mplayer_Queue *queue = &mplayer_ctx->queue;
	
	if (queue->count > queue->current_index)
	{
		queue->count = queue->current_index + 1;
	}
}

internal void
mplayer_queue_play_track(Mplayer_Track_ID track_id)
{
	mplayer_queue_next(track_id);
	mplayer_play_next_in_queue();
}

internal Mplayer_Track_ID
mplayer_queue_current_track_id()
{
	Mplayer_Track_ID result = NULL_TRACK_ID;
	if (is_valid(mplayer_ctx->queue.current_index))
	{
		result = mplayer_ctx->queue.tracks[mplayer_ctx->queue.current_index];
	}
	return result;
}

//~ NOTE(fakhri): Library Indexing, Serializing and stuff

internal void
mplayer_reset_library()
{
	Mplayer_Library *library = &mplayer_ctx->library;
	mplayer_clear_queue();
	
	// NOTE(fakhri): wait for worker threads to end
	for (;platform->do_next_work();) {}
	m_arena_free_all(&library->arena);
	
	library->artists_count = 0;
	library->albums_count = 0;
	library->tracks_count = 0;
	
	memory_zero_array(library->tracks_table);
	memory_zero_array(library->albums_table);
	memory_zero_array(library->artists_table);
	
	for (u32 i = 1; i < library->images_count; i += 1)
	{
		Mplayer_Item_Image *image = library->images + i;
		
		push_texture_release_request(&mplayer_ctx->render_ctx->release_buffer, image->texture);
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
mplayer_serialize(File_Handle *file, Mplayer_Track_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}
internal void
mplayer_serialize(File_Handle *file, Mplayer_Artist_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}
internal void
mplayer_serialize(File_Handle *file, Mplayer_Album_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}
internal void
mplayer_serialize(File_Handle *file, Mplayer_Playlist_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}

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
mplayer_serialize(File_Handle *file, Mplayer_Track_ID_List tracks)
{
	mplayer_serialize(file, tracks.count);
	for(Mplayer_Track_ID_Entry *entry = tracks.first;
		entry;
		entry = entry->next)
	{
		mplayer_serialize(file, entry->track_id);
	}
}

internal void
mplayer_serialize_track(File_Handle *file, Mplayer_Track *track)
{
	mplayer_serialize(file, track->hash);
	mplayer_serialize(file, track->path);
	mplayer_serialize(file, track->title);
	mplayer_serialize(file, track->album);
	mplayer_serialize(file, track->artist);
	mplayer_serialize(file, track->genre);
	mplayer_serialize(file, track->date);
	mplayer_serialize(file, track->track_number);
	mplayer_serialize(file, track->start_sample_offset);
	mplayer_serialize(file, track->end_sample_offset);
}

internal void
mplayer_serialize_playlist(File_Handle *file, Mplayer_Playlist *playlist)
{
	mplayer_serialize(file, playlist->name);
	mplayer_serialize(file, playlist->tracks);
}

internal void
mplayer_serialize_artist(File_Handle *file, Mplayer_Artist *artist)
{
	mplayer_serialize(file, artist->hash);
	mplayer_serialize(file, artist->name);
}

internal void
mplayer_serialize_album(File_Handle *file, Mplayer_Album *album)
{
	mplayer_serialize(file, album->hash);
	mplayer_serialize(file, album->name);
	mplayer_serialize(file, album->artist);
	
	Mplayer_Item_Image *image = mplayer_get_image_by_id(album->image_id, 0);
	mplayer_serialize(file, *image);
}

internal void
mplayer_deserialize_track(Byte_Stream *bs, Mplayer_Track *track)
{
	byte_stream_read(bs, &track->hash);
	byte_stream_read(bs, &track->path);
	byte_stream_read(bs, &track->title);
	byte_stream_read(bs, &track->album);
	byte_stream_read(bs, &track->artist);
	byte_stream_read(bs, &track->genre);
	byte_stream_read(bs, &track->date);
	byte_stream_read(bs, &track->track_number);
	byte_stream_read(bs, &track->start_sample_offset);
	byte_stream_read(bs, &track->end_sample_offset);
}

internal void
mplayer_deserialize_artist(Byte_Stream *bs, Mplayer_Artist *artist)
{
	byte_stream_read(bs, &artist->hash);
	byte_stream_read(bs, &artist->name);
}

internal void
mplayer_deserialize_album(Byte_Stream *bs, Mplayer_Album *album)
{
	byte_stream_read(bs, &album->hash);
	byte_stream_read(bs, &album->name);
	byte_stream_read(bs, &album->artist);
	
	Mplayer_Item_Image *image = mplayer_get_image_by_id(album->image_id, 0);
	byte_stream_read(bs, image);
	if (image->in_disk)
	{
		image->path = str8_clone(&mplayer_ctx->library.arena, image->path);
	}
	else
	{
		image->texture_data = clone_buffer(&mplayer_ctx->library.arena, image->texture_data);
	}
}

internal void
mplayer_save_indexed_library()
{
	Mplayer_Library *library = &mplayer_ctx->library;
	File_Handle *file = platform->open_file(MPLAYER_INDEX_FILENAME, File_Open_Write | File_Create_Always);
	
	mplayer_serialize(file, library->artists_count);
	mplayer_serialize(file, library->albums_count);
	mplayer_serialize(file, library->tracks_count);
	
	for (u32 artist_index = 0; artist_index < library->artists_count; artist_index += 1)
	{
		Mplayer_Artist_ID artist_id = library->artist_ids[artist_index];
		Mplayer_Artist *artist = mplayer_artist_by_id(library, artist_id);
		mplayer_serialize_artist(file, artist);
	}
	
	for (u32 album_index = 0; album_index < library->albums_count; album_index += 1)
	{
		Mplayer_Album_ID album_id = library->album_ids[album_index];
		Mplayer_Album *album = mplayer_album_by_id(library, album_id);
		mplayer_serialize_album(file, album);
	}
	
	for (u32 track_index = 0; track_index < library->tracks_count; track_index += 1)
	{
		Mplayer_Track_ID track_id = library->track_ids[track_index];
		Mplayer_Track *track = mplayer_track_by_id(library, track_id);
		mplayer_serialize_track(file, track);
	}
	
	platform->close_file(file);
}

internal Mplayer_Artist *
mplayer_setup_track_artist(Mplayer_Library *library, String8 artist_name)
{
	Mplayer_Artist_ID artist_hash = mplayer_compute_artist_id(artist_name);
	Mplayer_Artist *artist = mplayer_artist_by_id(library, artist_hash);
	if (!artist)
	{
		artist = m_arena_push_struct_z(&library->arena, Mplayer_Artist);
		artist->hash   = artist_hash;
		artist->name   = artist_name;
		mplayer_insert_artist(library, artist);
	}
	
	return artist;
}

internal Mplayer_Album *
mplayer_setup_track_album(Mplayer_Library *library, String8 album_name, Mplayer_Artist *artist)
{
	Mplayer_Album_ID album_hash = mplayer_compute_album_id(artist->name, album_name);
	Mplayer_Album *album = mplayer_album_by_id(library, album_hash);
	if (!album)
	{
		album = m_arena_push_struct_z(&library->arena, Mplayer_Album);
		album->hash   = album_hash;
		album->name   = album_name;
		album->artist = artist->name;
		album->image_id = mplayer_reserve_image_id(library);
		mplayer_insert_album(library, album);
		
		DLLPushBack_NP(artist->albums.first, artist->albums.last, album, next_artist, prev_artist);
		artist->albums.count += 1;
		album->artist_id = artist->hash;
	}
	
	return album;
}

internal b32
mplayer_load_indexed_library()
{
	b32 success = 0;
	mplayer_reset_library();
	
	Mplayer_Library *library = &mplayer_ctx->library;
	Buffer index_content = platform->load_entire_file(MPLAYER_INDEX_FILENAME, &library->arena);
	if (is_valid(index_content))
	{
		Byte_Stream bs = make_byte_stream(index_content);
		success = 1;
		
		u32 artists_count;
		u32 albums_count;
		u32 tracks_count;
		byte_stream_read(&bs, &artists_count);
		byte_stream_read(&bs, &albums_count);
		byte_stream_read(&bs, &tracks_count);
		
		for (u32 artist_index = 0; artist_index < artists_count; artist_index += 1)
		{
			Mplayer_Artist *artist = m_arena_push_struct_z(&library->arena, Mplayer_Artist);
			mplayer_deserialize_artist(&bs, artist);
			mplayer_insert_artist(library, artist);
		}
		
		for (u32 album_index = 0; album_index < albums_count; album_index += 1)
		{
			Mplayer_Album *album = m_arena_push_struct_z(&library->arena, Mplayer_Album);
			album->image_id = mplayer_reserve_image_id(library);
			mplayer_deserialize_album(&bs, album);
			mplayer_insert_album(library, album);
			
			Mplayer_Artist_ID artist_hash = mplayer_compute_artist_id(album->artist);
			Mplayer_Artist *artist = mplayer_artist_by_id(library, artist_hash);
			DLLPushBack_NP(artist->albums.first, artist->albums.last, album, next_artist, prev_artist);
			artist->albums.count += 1;
			album->artist_id = artist->hash;
		}
		
		for (u32 track_index = 0; track_index < tracks_count; track_index += 1)
		{
			Mplayer_Track *track = m_arena_push_struct_z(&library->arena, Mplayer_Track);
			mplayer_deserialize_track(&bs, track);
			mplayer_insert_track(library, track);
			
			// NOTE(fakhri): add track to artist
			{
				Mplayer_Artist_ID artist_hash = mplayer_compute_artist_id(track->artist);
				Mplayer_Artist *artist = mplayer_artist_by_id(library, artist_hash);
				assert(artist);
				DLLPushBack_NP(artist->tracks.first, artist->tracks.last, track, next_artist, prev_artist);
				artist->tracks.count += 1;
				track->artist_id = artist->hash;
			}
			
			// NOTE(fakhri): add track to album
			{
				Mplayer_Album_ID album_hash = mplayer_compute_album_id(track->artist, track->album);
				Mplayer_Album *album = mplayer_album_by_id(library, album_hash);
				
				DLLPushBack_NP(album->tracks.first, album->tracks.last, track, next_album, prev_album);
				album->tracks.count += 1;
				track->album_id = album->hash;
			}
		}
	}
	return success;
}

#define CUESHEET_FRAMES_PER_SECOND 75
struct Cuesheet_Track_Index
{
	u32 minutes;
	u32 seconds;
	u32 frames;
};
struct Cuesheet_Track
{
	Cuesheet_Track *next;
	Cuesheet_Track *prev;
	String8 title;
	String8 performer;
	Cuesheet_Track_Index index;
};

struct Cuesheet_File
{
	Cuesheet_File *next;
	Cuesheet_File *prev;
	
	String8 performer;
	String8 title;
	String8 file;
	
	Cuesheet_Track *first_tracks;
	Cuesheet_Track *last_tracks;
};

struct Cuesheet_List
{
	Cuesheet_File *first;
	Cuesheet_File *last;
};

internal u64 
mplayer_sample_number_from_cue_index(Cuesheet_Track_Index index, u64 samples_per_second)
{
	u64 seconds = index.minutes * 60 + index.seconds;
	u64 samples_count = seconds * samples_per_second;
	samples_count += (index.frames * samples_per_second / CUESHEET_FRAMES_PER_SECOND);
	return samples_count;
}

internal Cuesheet_File *
mplayer_find_cuesheet_for_file(Cuesheet_List cuesheet_list, String8 file_name)
{
	Cuesheet_File *result = 0;
	for (Cuesheet_File *cuesheet = cuesheet_list.first; cuesheet; cuesheet = cuesheet->next)
	{
		if (find_substr8(cuesheet->file, file_name, 0, MatchFlag_CaseInsensitive) < cuesheet->file.len)
		{
			result = cuesheet;
			break;
		}
	}
	return result;
}

internal void
mplayer_make_track(File_Info info, u64 samples_start, u64 samples_end, Mplayer_Album *album,
	Mplayer_Artist *artist, 
	String8 track_title, String8 track_date, String8 track_genre, String8 track_track_number)
{
	Mplayer_Library *library = &mplayer_ctx->library;
	Mplayer_Track_ID track_hash = mplayer_compute_track_id(info.path, samples_start, samples_end);
	Mplayer_Track *track = mplayer_track_by_id(library, track_hash);
	if (!track)
	{
		track = m_arena_push_struct_z(&library->arena, Mplayer_Track);
		track->path = str8_clone(&library->arena, info.path);
		track->hash = track_hash;
		mplayer_insert_track(library, track);
		
		track->start_sample_offset = samples_start;
		track->end_sample_offset = samples_end;
		
		
		if (!track_title.len)
		{
			track_title = str8_clone(&library->arena, info.name);
		}
		
		track->title        = track_title;
		track->album        = album->name;
		track->artist       = artist->name;
		track->date         = track_date;
		track->genre        = track_genre;
		track->track_number = track_track_number;
		
		// TODO(fakhri): use parent directory name as album name if the track doesn't contain an album name
		
		DLLPushBack_NP(artist->tracks.first, artist->tracks.last, track, next_artist, prev_artist);
		artist->tracks.count += 1;
		track->artist_id = artist->hash;
		
		// NOTE(fakhri): setup album
		{
			DLLPushBack_NP(album->tracks.first, album->tracks.last, track, next_album, prev_album);
			album->tracks.count += 1;
			track->album_id = album->hash;
			
		}
	}
}

internal void
mplayer_setup_album_image(Memory_Arena *arena, Mplayer_Image_ID image_id, String8 cover_image_path, Flac_Picture *front_cover)
{
	Mplayer_Item_Image *image = mplayer_get_image_by_id(image_id, 0);
	if (!image->in_disk && !image->texture_data.size)
	{
		if (cover_image_path.len)
		{
			image->in_disk = true;
			image->path = str8_clone(arena, cover_image_path);
		}
		else if (front_cover)
		{
			image->in_disk = false;
			image->texture_data = clone_buffer(arena, front_cover->buffer);
		}
	}
}

internal void
mplayer_parse_flac_track_file(File_Info info, Directory dir, Cuesheet_List cuesheet_list, String8 cover_image_path)
{
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	Cuesheet_File *cuesheet = mplayer_find_cuesheet_for_file(cuesheet_list, info.name);
	if (cuesheet)
	{
		Log("cuesheet found: %.*s", STR8_EXPAND(cuesheet->file));
	}
	
	Buffer tmp_file_block = arena_push_buffer(scratch.arena, megabytes(1));
	File_Handle *file = platform->open_file(info.path, File_Open_Read);
	Flac_Stream tmp_flac_stream = ZERO_STRUCT;
	assert(file);
	if (file)
	{
		Mplayer_Library *library = &mplayer_ctx->library;
		Buffer buffer = platform->read_block(file, tmp_file_block.data, tmp_file_block.size);
		platform->close_file(file);
		// NOTE(fakhri): init flac stream
		{
			tmp_flac_stream.bitstream.buffer = buffer;
			tmp_flac_stream.bitstream.pos.byte_index = 0;
			tmp_flac_stream.bitstream.pos.bits_left  = 8;
		}
		
		if (flac_process_metadata(&tmp_flac_stream, scratch.arena))
		{
			String8 track_title        = str8_lit("");
			String8 track_album        = str8_lit("");
			String8 track_artist       = str8_lit("");
			String8 track_date         = str8_lit("");
			String8 track_genre        = str8_lit("");
			String8 track_track_number = str8_lit("");
			
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
					track_title = str8_clone(&library->arena, value);
				}
				else if (str8_match(key, str8_lit("album"), MatchFlag_CaseInsensitive))
				{
					track_album = str8_clone(&library->arena, value);
				}
				else if (str8_match(key, str8_lit("artist"), MatchFlag_CaseInsensitive))
				{
					track_artist = str8_clone(&library->arena, value);
				}
				else if (str8_match(key, str8_lit("genre"), MatchFlag_CaseInsensitive))
				{
					track_genre = str8_clone(&library->arena, value);
				}
				else if (str8_match(key, str8_lit("data"), MatchFlag_CaseInsensitive))
				{
					track_date = str8_clone(&library->arena, value);
				}
				else if (str8_match(key, str8_lit("tracknumber"), MatchFlag_CaseInsensitive))
				{
					track_track_number = str8_clone(&library->arena, value);
				}
			}
			
			Flac_Picture *front_cover = tmp_flac_stream.front_cover;
			
			if (cuesheet)
			{
				for (Cuesheet_Track *cuesheet_track = cuesheet->first_tracks; cuesheet_track; cuesheet_track = cuesheet_track->next)
				{
					u64 samples_start = mplayer_sample_number_from_cue_index(cuesheet_track->index, tmp_flac_stream.streaminfo.sample_rate);
					u64 samples_end   = tmp_flac_stream.streaminfo.samples_count;
					if (cuesheet_track->next)
					{
						samples_end = mplayer_sample_number_from_cue_index(cuesheet_track->next->index, tmp_flac_stream.streaminfo.sample_rate);
					}
					
					String8 cue_track_title = track_title;
					if (cuesheet_track->title.len)
					{
						cue_track_title = str8_clone(&library->arena, cuesheet_track->title);
					}
					
					if (cuesheet_track->performer.len && !track_artist.len)
					{
						track_artist = str8_clone(&library->arena, cuesheet_track->performer);
					}
					
					Mplayer_Artist *artist = mplayer_setup_track_artist(library, track_artist);
					Mplayer_Album *album = mplayer_setup_track_album(library, track_album, artist);
					mplayer_setup_album_image(&library->arena, album->image_id, cover_image_path, front_cover);
					
					mplayer_make_track(info, samples_start, samples_end, album, artist,
						cue_track_title, track_date, track_genre, track_track_number);
					
				}
			}
			else
			{
				Mplayer_Artist *artist = mplayer_setup_track_artist(library, track_artist);
				Mplayer_Album *album = mplayer_setup_track_album(library, track_album, artist);
				mplayer_setup_album_image(&library->arena, album->image_id, cover_image_path, front_cover);
				
				mplayer_make_track(info, 0, tmp_flac_stream.streaminfo.samples_count, album, artist,
					track_title, track_date, track_genre, track_track_number);
			}
			
		}
	}
}



internal String8
str8_chop_quotes(String8 str)
{
	String8 result = str;
	if (result.str[0] == '"')
	{
		result = str8_skip_first(result, 1);
		result = str8_chop_last(result, 1);
	}
	return result;
}

internal void
mplayer_load_tracks_in_directory(String8 library_path)
{
	Memory_Checkpoint_Scoped temp_mem(mplayer_ctx->frame_arena);
	
	Directory dir = platform->read_directory(temp_mem.arena, library_path);
	
	// NOTE(fakhri): extract cuesheets if exist
	String8 cover_image_path = ZERO_STRUCT;
	Cuesheet_List cuesheet_list = ZERO_STRUCT;
	
	for (u32 info_index = 0; info_index < dir.count; info_index += 1)
	{
		File_Info info = dir.files[info_index];
		if (has_flag(info.flags, FileFlag_Directory)) continue;
		
		if (!cover_image_path.len && (str8_ends_with(info.name, str8_lit(".jpg"), MatchFlag_CaseInsensitive)  ||
				str8_ends_with(info.name, str8_lit(".jpeg"), MatchFlag_CaseInsensitive) ||
				str8_ends_with(info.name, str8_lit(".png"), MatchFlag_CaseInsensitive)))
		{
			cover_image_path = info.path;
		}
		
		if (str8_ends_with(info.path, str8_lit(".cue"), MatchFlag_CaseInsensitive))
		{
			enum Cuesheet_Context_Kind
			{
				Cuesheet_Context_None,
				Cuesheet_Context_File,
				Cuesheet_Context_Track,
			} current_ctx_kind = Cuesheet_Context_None;
			
			String8 global_performer = ZERO_STRUCT;
			String8 global_title = ZERO_STRUCT;
			
			Cuesheet_File *current_cuesheet_file = 0;
			Cuesheet_Track *current_cuesheet_track = 0;
			
			// TODO(fakhri): parse cue file
			String8 content = to_string(platform->load_entire_file(info.path, temp_mem.arena));
			
			for (u32 line_start_offset = 0; line_start_offset < content.len;)
			{
				u32 line_len = 0;
				for (; line_start_offset + line_len < content.len; line_len += 1)
				{
					if (content.str[line_start_offset + line_len] == '\n' || 
						content.str[line_start_offset + line_len] == '\r' && content.str[line_start_offset + line_len + 1] == '\n')
					{
						String8 line = str8(content.str + line_start_offset, line_len);
						line_len += ((content.str[line_start_offset + line_len] == '\r')? 2:1);
						
						// NOTE(fakhri): skip whitespaces at the begining of the line
						line = str8_skip_first(line, string_find_first_non_whitespace(line));
						
						String8 command = prefix8(line, string_find_first_whitespace(line));
						if (0) {}
						else if (str8_match(command, str8_lit("REM"), MatchFlag_CaseInsensitive)) {/*NOTE(fakhri): do nothing*/}
						else if (str8_match(command, str8_lit("FILE"), MatchFlag_CaseInsensitive)) 
						{
							String8 file_cmd_str = str8_skip_first(line, command.len);
							String8 file_name_str = str8_skip_leading_spaces(file_cmd_str);
							if (file_name_str.str[0] == '"')
							{
								file_name_str = str8_skip_first(file_name_str, 1);
								file_name_str = prefix8(file_name_str, string_find_first_characer(file_name_str, '"'));
							}
							
							Cuesheet_File *cuesheet = m_arena_push_struct_z(temp_mem.arena, Cuesheet_File);
							DLLPushBack(cuesheet_list.first, cuesheet_list.last, cuesheet);
							current_cuesheet_file = cuesheet;
							current_ctx_kind = Cuesheet_Context_File;
							
							cuesheet->file = file_name_str;
						}
						else if (str8_match(command, str8_lit("TRACK"), MatchFlag_CaseInsensitive)) 
						{
							String8 track_cmd_str = str8_skip_first(line, command.len);
							// TODO(fakhri): parse track number
							assert(current_cuesheet_file);
							if (current_cuesheet_file)
							{
								Cuesheet_Track *cuesheet_track = m_arena_push_struct_z(temp_mem.arena, Cuesheet_Track);
								DLLPushBack(current_cuesheet_file->first_tracks, current_cuesheet_file->last_tracks, cuesheet_track);
								current_cuesheet_track = cuesheet_track;
								current_ctx_kind = Cuesheet_Context_Track;
							}
						}
						else if (str8_match(command, str8_lit("PERFORMER"), MatchFlag_CaseInsensitive)) 
						{
							String8 performer_str = str8_skip_first(line, command.len);
							performer_str = str8_skip_leading_spaces(performer_str);
							performer_str = str8_chop_quotes(performer_str);
							
							switch(current_ctx_kind)
							{
								case Cuesheet_Context_None:
								{
									global_performer = performer_str;
								} break;
								case Cuesheet_Context_Track:
								{
									assert(current_cuesheet_track);
									if (current_cuesheet_track)
									{
										current_cuesheet_track->performer = performer_str;
									}
								} break;
								case Cuesheet_Context_File:
								{
									// invalid_code_path("PERFORMER is not allowed in FILE context");
								} break;
							}
						}
						else if (str8_match(command, str8_lit("TITLE"), MatchFlag_CaseInsensitive)) 
						{
							String8 title_str = str8_skip_first(line, command.len);
							title_str = str8_skip_leading_spaces(title_str);
							title_str = str8_chop_quotes(title_str);
							
							switch(current_ctx_kind)
							{
								case Cuesheet_Context_None:
								{
									global_title = title_str;
								} break;
								case Cuesheet_Context_Track:
								{
									assert(current_cuesheet_track);
									if (current_cuesheet_track)
									{
										current_cuesheet_track->title = title_str;
									}
								} break;
								case Cuesheet_Context_File:
								{
									//invalid_code_path("TITLE is not allowed in FILE context");
								} break;
							}
						}
						else if (str8_match(command, str8_lit("INDEX"), MatchFlag_CaseInsensitive))
						{
							line = str8_skip_leading_spaces(str8_skip_first(line, command.len));
							String8 index_number = prefix8(line, string_find_first_whitespace(line));
							
							// TODO(fakhri): handle multiple index numbers
							
							Cuesheet_Track_Index track_index_ts = ZERO_STRUCT;
							if (u64_from_str8_base10(index_number) == 1)
							{
								String8 index_timestamp = str8_skip_leading_spaces(str8_skip_first(line, index_number.len));
								
								String8 minutes_str = prefix8(index_timestamp, string_find_first_characer(index_timestamp, ':'));
								index_timestamp = str8_skip_first(index_timestamp, minutes_str.len + 1);
								
								String8 seconds_str = prefix8(index_timestamp, string_find_first_characer(index_timestamp, ':'));
								index_timestamp = str8_skip_first(index_timestamp, seconds_str.len + 1);
								
								String8 frames_str = index_timestamp;
								
								track_index_ts.minutes = (u32)u64_from_str8_base10(minutes_str);
								track_index_ts.seconds = (u32)u64_from_str8_base10(seconds_str);
								track_index_ts.frames  = (u32)u64_from_str8_base10(frames_str);
							}
							
							if (current_ctx_kind == Cuesheet_Context_File)
							{
								// NOTE(fakhri): altho it's not allowed in official specification, some files 
								// allow index command in file context, for these cases we create an implecit track
								assert(current_cuesheet_file);
								if (current_cuesheet_file)
								{
									Cuesheet_Track *cuesheet_track = m_arena_push_struct_z(temp_mem.arena, Cuesheet_Track);
									DLLPushBack(current_cuesheet_file->first_tracks, current_cuesheet_file->last_tracks, cuesheet_track);
									current_cuesheet_track = cuesheet_track;
									current_ctx_kind = Cuesheet_Context_Track;
								}
							}
							
							if (current_ctx_kind == Cuesheet_Context_Track)
							{
								assert(current_cuesheet_track);
								if (current_cuesheet_track)
								{
									current_cuesheet_track->index = track_index_ts;
								}
							}
							else 
							{
								invalid_code_path("INDEX command is not allowed in this context");
							}
							
						}
						
						break;
					}
				}
				
				line_start_offset += line_len;
			}
			
		}
	}
	
	for (u32 info_index = 0; info_index < dir.count; info_index += 1)
	{
		File_Info info = dir.files[info_index];
		
		if (has_flag(info.flags, FileFlag_Directory))
		{
			// NOTE(fakhri): directory
			if (info.name.str[0] != '.')
			{
				mplayer_load_tracks_in_directory(info.path);
			}
		}
		else
		{
			if (str8_ends_with(info.name, str8_lit(".flac"), MatchFlag_CaseInsensitive))
			{
				mplayer_parse_flac_track_file(info, dir, cuesheet_list, cover_image_path);
			}
		}
	}
}

internal void
mplayer_index_library()
{
	Mplayer_Library *library = &mplayer_ctx->library;
	
	mplayer_reset_library();
	
	for (u32 i = 0; i < mplayer_ctx->settings.locations_count; i += 1)
	{
		Mplayer_Library_Location *location = mplayer_ctx->settings.locations + i;
		mplayer_load_tracks_in_directory(str8(location->path, location->path_len));
	}
	
	mplayer_save_indexed_library();
}


internal void
mplayer_load_library()
{
	if (!mplayer_load_indexed_library())
	{
		mplayer_index_library();
	}
}

//~ NOTE(fakhri): Mode stack stuff

internal void
mplayer_change_previous_mode()
{
	if (mplayer_ctx->mode_stack && mplayer_ctx->mode_stack->prev)
	{
		mplayer_ctx->mode_stack = mplayer_ctx->mode_stack->prev;
	}
}

internal void
mplayer_change_next_mode()
{
	if (mplayer_ctx->mode_stack && mplayer_ctx->mode_stack->next)
	{
		mplayer_ctx->mode_stack = mplayer_ctx->mode_stack->next;
	}
}

internal void
mplayer_change_mode(Mplayer_Mode mode, Mplayer_Item_ID id = ZERO_STRUCT)
{
	if (mplayer_ctx->mode_stack && mplayer_ctx->mode_stack->next)
	{
		QueuePush(mplayer_ctx->mode_stack_free_list_first, mplayer_ctx->mode_stack_free_list_last, mplayer_ctx->mode_stack->next);
		mplayer_ctx->mode_stack->next = 0;
	}
	
	Mplayer_Mode_Stack *new_mode_node = 0;
	if (mplayer_ctx->mode_stack_free_list_first)
	{
		new_mode_node = mplayer_ctx->mode_stack_free_list_first;
		QueuePop(mplayer_ctx->mode_stack_free_list_first, mplayer_ctx->mode_stack_free_list_last);
	}
	
	if (!new_mode_node)
	{
		new_mode_node = m_arena_push_struct_z(&mplayer_ctx->main_arena, Mplayer_Mode_Stack);
	}
	
	new_mode_node->prev = mplayer_ctx->mode_stack;
	new_mode_node->mode = mode;
	new_mode_node->id = id;
	
	if (mplayer_ctx->mode_stack)
	{
		mplayer_ctx->mode_stack->next = new_mode_node;
	}
	mplayer_ctx->mode_stack = new_mode_node;
}


//~ NOTE(fakhri): Settings stuff

internal void
mplayer_add_location(String8 location_path)
{
	assert(mplayer_ctx->settings.locations_count < array_count(mplayer_ctx->settings.locations));
	u32 i = mplayer_ctx->settings.locations_count++;
	Mplayer_Library_Location *location = mplayer_ctx->settings.locations + i;
	assert(location_path.len < array_count(location->path));
	
	location->path_len = u32(location_path.len);
	memory_copy(location->path, location_path.str, location_path.len);
}

#define SETTINGS_FILE_NAME str8_lit("settings.mplayer")
internal void
mplayer_load_settings()
{
	// TODO(fakhri): save the config in user directory
	String8 config_path = SETTINGS_FILE_NAME;
	Buffer config_content = platform->load_entire_file(config_path, mplayer_ctx->frame_arena);
	
	if (config_content.size)
	{
		assert(config_content.size == sizeof(MPlayer_Settings));
		memory_copy(&mplayer_ctx->settings, config_content.data, config_content.size);
	}
	else
	{
		mplayer_ctx->settings = DEFAUTL_SETTINGS;
	}
}

internal void
mplayer_save_settings()
{
	// TODO(fakhri): save the config in user directory
	String8 config_path = SETTINGS_FILE_NAME;
	File_Handle *config_file = platform->open_file(config_path, File_Open_Write | File_Create_Always);
	if (config_file)
	{
		platform->write_block(config_file, &mplayer_ctx->settings, sizeof(mplayer_ctx->settings));
		platform->close_file(config_file);
	}
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
	platform->fix_path_slashes(path_lister->user_path);
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
mplayer_open_path_lister(Mplayer_Path_Lister *path_lister)
{
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	
	path_lister->user_path = str8(path_lister->user_path_buffer, 0);
	String8 cwd = platform->get_current_directory(scratch.arena);
	mplayer_set_path_lister_path(path_lister, cwd);
	ui_open_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Path_Lister]);
}

internal void
mplayer_close_path_lister(Mplayer_Path_Lister *path_lister)
{
	m_arena_free_all(&path_lister->arena);
	path_lister->filtered_paths_count = 0;
	path_lister->base_dir = ZERO_STRUCT;
	ui_close_modal_menu();
}

//~ NOTE(fakhri): Custom UI stuff


internal void
ui_grid_push_row()
{
	ui_next_height(ui_size_pixel(mplayer_ctx->grid_item_dim.height, 1));
	ui_push_parent(ui_layout(Axis2_X));
	ui_spacer(ui_size_parent_remaining());
	mplayer_ctx->grid_item_index_in_row = 0;
}

internal b32
ui_grid_item_begin()
{
	b32 good = 0;
	if (mplayer_ctx->grid_item_index < mplayer_ctx->grid_items_count && 
		mplayer_ctx->grid_item_index_in_row < mplayer_ctx->grid_item_count_per_row)
	{
		good = 1;
		
		mplayer_ctx->grid_item_index += 1;
		mplayer_ctx->grid_item_index_in_row += 1;
		ui_next_width(ui_size_pixel(mplayer_ctx->grid_item_dim.width, 1));
		ui_next_height(ui_size_pixel(mplayer_ctx->grid_item_dim.height, 1));
		UI_Element *grid_item = ui_element({0}, 0);
		ui_push_parent(grid_item);
	}
	return good;
}


internal void
ui_grid_item_end()
{
	// NOTE(fakhri): pop grid item
	ui_pop_parent();
	ui_spacer(ui_size_parent_remaining());
	
	// NOTE(fakhri): check if row is full
	if (mplayer_ctx->grid_item_index_in_row == mplayer_ctx->grid_item_count_per_row)
	{
		ui_pop_parent(); // pop parent row
		ui_spacer_pixels(mplayer_ctx->grid_vpadding, 1);
		mplayer_ctx->grid_row_index += 1;
		
		if (mplayer_ctx->grid_row_index < (i32)mplayer_ctx->grid_visible_rows_count)
		{
			ui_grid_push_row();
		}
	}
}

internal u32
ui_grid_begin(String8 string, u32 items_count, V2_F32 grid_item_dim, f32 vpadding)
{
	ui_next_childs_axis(Axis2_Y);
	UI_Element *grid = ui_element(string, UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Animate_Scroll | UI_FLAG_Clip);
	ui_interaction_from_element(grid);
	ui_push_parent(grid);
	
	u32 item_count_per_row = (u32)((grid->computed_dim.width) / grid_item_dim.width);
	item_count_per_row = MAX(item_count_per_row, 1);
	
	u32 max_rows_count = ((items_count + item_count_per_row - 1) / item_count_per_row);
	
	u32 visible_rows_count = u32(grid->computed_dim.height / (grid_item_dim.height + vpadding)) + 2;
	
	f32 space_before_first_row = 0;
	{
		UI_Element_Persistent_State *persistent_state = ui_get_persistent_state_for_element(grid->id);
		if (persistent_state)
			space_before_first_row = ABS(persistent_state->view_scroll.y);
	}
	u32 first_visible_row = (u32)(space_before_first_row / (grid_item_dim.height + vpadding));
	first_visible_row = MIN(first_visible_row, max_rows_count-1);
	
	ui_spacer_pixels(first_visible_row * (grid_item_dim.height + vpadding), 1);
	
	mplayer_ctx->grid_items_count = items_count;
	mplayer_ctx->grid_first_visible_row = first_visible_row;
	mplayer_ctx->grid_item_count_per_row = item_count_per_row;
	mplayer_ctx->grid_max_rows_count = max_rows_count;
	mplayer_ctx->grid_item_index_in_row = 0;
	mplayer_ctx->grid_row_index = 0;
	mplayer_ctx->grid_visible_rows_count = visible_rows_count;
	mplayer_ctx->grid_vpadding = vpadding;
	mplayer_ctx->grid_item_dim = grid_item_dim;
	
	if (items_count)
	{
		ui_grid_push_row();
	}
	
	mplayer_ctx->grid_item_index = first_visible_row * item_count_per_row;
	return mplayer_ctx->grid_item_index;
}

internal void
ui_grid_end()
{
	// NOTE(fakhri): pop incomplete rows
	if (mplayer_ctx->grid_items_count && mplayer_ctx->grid_row_index < (i32)mplayer_ctx->grid_visible_rows_count && 
		mplayer_ctx->grid_item_index_in_row < mplayer_ctx->grid_item_count_per_row)
	{
		ui_pop_parent(); // pop parent row
	}
	
	u32 rows_remaining = mplayer_ctx->grid_max_rows_count - MIN(mplayer_ctx->grid_first_visible_row + mplayer_ctx->grid_visible_rows_count, mplayer_ctx->grid_max_rows_count);
	ui_spacer_pixels(rows_remaining * (mplayer_ctx->grid_item_dim.height + mplayer_ctx->grid_vpadding), 1);
	
	// NOTE(fakhri): pop grid
	ui_pop_parent();
}

#define ui_for_each_grid_item(string, items_count, item_dim, vpadding, item_index) \
defer(ui_grid_end()) for (u32 item_index = ui_grid_begin(string, items_count, item_dim, vpadding); \
ui_grid_item_begin();\
(item_index += 1, ui_grid_item_end()))

#define ui_for_each_list_item(string, items_count, item_height, vpadding, item_index) \
defer(ui_list_end()) for (u32 item_index = ui_list_begin(string, items_count, item_height, vpadding); \
ui_list_item_begin();\
(item_index += 1, ui_list_item_end()))


internal b32
ui_list_item_begin()
{
	b32 good = 0;
	if (mplayer_ctx->list_item_index < mplayer_ctx->list_items_count && 
		mplayer_ctx->list_row_index < mplayer_ctx->list_visible_rows_count)
	{
		good = 1;
		
		mplayer_ctx->list_item_index += 1;
		mplayer_ctx->list_row_index += 1;
		
		ui_next_width(ui_size_parent_remaining());
		ui_next_height(ui_size_pixel(mplayer_ctx->list_item_height, 1));
		UI_Element *list_item = ui_element({0}, 0);
		
		ui_push_parent(list_item);
	}
	return good;
}

internal void
ui_list_item_end()
{
	ui_pop_parent();
	ui_spacer_pixels(mplayer_ctx->list_vpadding, 1);
}

internal u32
ui_list_begin(String8 string, u32 items_count, f32 item_height, f32 vpadding)
{
	UI_Element *list = ui_element(string, UI_FLAG_Draw_Border | UI_FLAG_Draw_Background | UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Animate_Scroll | UI_FLAG_Clip);
	ui_interaction_from_element(list);
	list->child_layout_axis = Axis2_Y;
	ui_push_parent(list);
	
	u32 visible_rows_count = u32(list->computed_dim.height / (item_height + vpadding)) + 2;
	
	f32 space_before_first_row = 0;
	{
		UI_Element_Persistent_State *persistent_state = ui_get_persistent_state_for_element(list->id);
		if (persistent_state)
			space_before_first_row = ABS(persistent_state->view_scroll.y);
	}
	u32 first_visible_row = (u32)(space_before_first_row / (item_height + vpadding));
	first_visible_row = MIN(first_visible_row, items_count);
	
	ui_spacer_pixels(first_visible_row * (item_height + vpadding), 1);
	
	mplayer_ctx->list_items_count = items_count;
	mplayer_ctx->list_first_visible_row = first_visible_row;
	mplayer_ctx->list_max_rows_count = items_count;
	mplayer_ctx->list_row_index = 0;
	mplayer_ctx->list_visible_rows_count = visible_rows_count;
	mplayer_ctx->list_vpadding = vpadding;
	mplayer_ctx->list_item_height = item_height;
	
	mplayer_ctx->list_item_index = first_visible_row;
	return mplayer_ctx->list_item_index;
}

internal void
ui_list_end()
{
	u32 rows_remaining = mplayer_ctx->list_max_rows_count - MIN(mplayer_ctx->list_first_visible_row + mplayer_ctx->list_visible_rows_count, mplayer_ctx->list_max_rows_count);
	ui_spacer_pixels(rows_remaining * (mplayer_ctx->list_item_height + mplayer_ctx->list_vpadding), 1);
	
	// NOTE(fakhri): pop list
	ui_pop_parent();
}


internal Mplayer_UI_Interaction
mplayer_ui_underlined_button(String8 string)
{
	ui_next_hover_cursor(Cursor_Hand);
	UI_Element *button = ui_element(string, UI_FLAG_Draw_Text | UI_FLAG_Clickable);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(button);
	if (interaction.hover) ui_pref_childs_axis(Axis2_Y) ui_parent(button)
	{
		ui_spacer(ui_size_percent(0.85f, 1));
		
		ui_next_roundness(0);
		ui_next_width(ui_size_percent(1, 0));
		ui_next_height(ui_size_pixel(1, 0));
		ui_next_background_color(button->text_color);
		ui_element({}, UI_FLAG_Draw_Background);
	}
	return interaction;
}

internal Mplayer_UI_Interaction
mplayer_ui_underlined_button_f(const char *fmt, ...)
{
	va_list args;
  va_start(args, fmt);
	String8 string = str8_fv(g_ui->frame_arena, fmt, args);
	va_end(args);
	
	return mplayer_ui_underlined_button(string);
}

internal Mplayer_UI_Interaction 
mplayer_ui_track_item(Mplayer_Track_ID track_id)
{
	Mplayer_Track *track = mplayer_track_by_id(&mplayer_ctx->library, track_id);
	UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
	
	V4_F32 bg_color = vec4(0.02f, 0.02f,0.02f, 1);
	if (is_equal(track_id, mplayer_queue_current_track_id()))
	{
		bg_color = vec4(0, 0, 0, 1);
	}
	
	ui_next_background_color(bg_color);
	ui_next_hover_cursor(Cursor_Hand);
	UI_Element *track_el = ui_element_f(flags, "library_track_%p", track);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(track_el);
	#if 0
		if (interaction.clicked_left)
	{
		mplayer_queue_play_track(track_id);
		mplayer_queue_resume();
	}
	if (interaction.clicked_right)
	{
		ui_open_ctx_menu(g_ui->mouse_pos, mplayer_ctx->ctx_menu_ids[Track_Context_Menu]);
		mplayer_ctx->ctx_menu_item_id = to_item_id(track_id);
	}
	#endif
		
		ui_parent(track_el) ui_vertical_layout() ui_padding(ui_size_parent_remaining())
		ui_horizontal_layout()
	{
		ui_spacer_pixels(10, 1);
		
		ui_next_width(ui_size_text_dim(1));
		ui_next_height(ui_size_text_dim(1));
		ui_element_f(UI_FLAG_Draw_Text, "%.*s##library_track_name_%p", 
			STR8_EXPAND(track->title), track);
	}
	
	return interaction;
}


internal void
mplayer_ui_album_item(Mplayer_Album_ID album_id)
{
	Mplayer_Album *album = mplayer_album_by_id(&mplayer_ctx->library, album_id);
	
	Mplayer_Item_Image *image = mplayer_get_image_by_id(album->image_id, 1);
	
	UI_Element_Flags flags = UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim | UI_FLAG_Clickable | UI_FLAG_Clip;
	b32 image_ready = mplayer_item_image_ready(*image);
	if (image_ready)
	{
		flags |= UI_FLAG_Draw_Image;
		ui_next_texture_tint_color(vec4(1, 1, 1, 0.35f));
		ui_next_texture(image->texture);
	}
	else
	{
		flags |= UI_FLAG_Draw_Background;
		//ui_next_background_color(vec4(1, 1, 1, 0.35f));
		ui_next_background_color(vec4(0.25f, 0.25f, 0.25f, 0.35f));
	}
	
	ui_next_hover_cursor(Cursor_Hand);
	ui_next_roundness(10);
	UI_Element *album_el = ui_element_f(flags, "library_album_%p", album);
	album_el->child_layout_axis = Axis2_Y;
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(album_el);
	if (interaction.clicked_left)
	{
		mplayer_change_mode(MODE_Album_Tracks, to_item_id(album_id));
	}
	
	if (interaction.hover && image_ready)
	{
		album_el->texture_tint_color = vec4(1, 1, 1, 0.9f);
	}
	
	ui_parent(album_el)
	{
		ui_next_width(ui_size_percent(1, 1));
		ui_next_height(ui_size_pixel(50, 1));
		ui_next_background_color(vec4(0.3f, 0.3f, 0.3f, 0.3f));
		ui_next_roundness(5);
		ui_element_f(UI_FLAG_Draw_Background|UI_FLAG_Draw_Text | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim, "%.*s##library_album_name_%p", 
			STR8_EXPAND(album->name), album);
	}
}

internal Mplayer_UI_Interaction 
mplayer_ui_playlist_item(Mplayer_Playlist_ID playlist_id)
{
	Mplayer_Playlist *playlist = mplayer_playlist_by_id(&mplayer_ctx->playlists, playlist_id);
	
	UI_Element_Flags flags = UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim | UI_FLAG_Clickable | UI_FLAG_Clip | UI_FLAG_Draw_Background;
	ui_next_background_color(vec4(0.25f, 0.25f, 0.25f, 0.35f));
	
	ui_next_width(ui_size_percent(1,1));
	ui_next_height(ui_size_percent(1,1));
	ui_next_hover_cursor(Cursor_Hand);
	ui_next_roundness(10);
	UI_Element *playlist_el = ui_element_f(flags, "library_playlist_%p", playlist);
	playlist_el->child_layout_axis = Axis2_Y;
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(playlist_el);
	
	ui_parent(playlist_el)
	{
		ui_next_width(ui_size_percent(1, 1));
		ui_next_height(ui_size_pixel(50, 1));
		ui_next_background_color(vec4(0.2f, 0.2f, 0.2f, 0.5f));
		ui_next_roundness(5);
		ui_element_f(UI_FLAG_Draw_Background|UI_FLAG_Draw_Text | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim, "%.*s##library_playlist_name_%p", 
			STR8_EXPAND(playlist->name), playlist);
	}
	
	return interaction;
}

internal void
mplayer_ui_aritst_item(Mplayer_Artist_ID artist_id)
{
	Mplayer_Artist *artist = mplayer_artist_by_id(&mplayer_ctx->library, artist_id);
	Mplayer_Item_Image *image = mplayer_get_image_by_id(artist->image_id, 1);
	
	UI_Element_Flags flags = 0;
	b32 image_ready = mplayer_item_image_ready(*image);
	if (image_ready)
	{
		flags |= UI_FLAG_Draw_Image;
		ui_next_texture_tint_color(vec4(1.f, 1.f, 1.f, 0.35f));
		ui_next_texture(image->texture);
	}
	else
	{
		flags |= UI_FLAG_Draw_Background;
		ui_next_background_color(vec4(0.25f, 0.25f, 0.25f, 0.35f));
	}
	
	ui_next_hover_cursor(Cursor_Hand);
	ui_next_roundness(10);
	ui_next_width(ui_size_percent(1, 1));
	ui_next_height(ui_size_percent(1, 1));
	flags |= UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim | UI_FLAG_Clickable | UI_FLAG_Clip;
	UI_Element *artist_el = ui_element_f(flags, "library_artist_%p", artist);
	artist_el->child_layout_axis = Axis2_Y;
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(artist_el);
	if (interaction.clicked_left)
	{
		mplayer_change_mode(MODE_Artist_Albums, to_item_id(artist_id));
	}
	if (interaction.hover && image_ready)
	{
		artist_el->texture_tint_color = vec4(1, 1, 1, 1);
	}
	
	ui_parent(artist_el)
	{
		ui_spacer_pixels(10, 1);
		
		ui_next_width(ui_size_percent(1, 1));
		ui_next_height(ui_size_text_dim(1));
		ui_element_f(UI_FLAG_Draw_Text | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim, "%.*s##library_artist_name_%p", 
			STR8_EXPAND(artist->name), artist);
	}
}


internal void
mplayer_ui_side_bar_button(String8 string, Mplayer_Mode target_mode)
{
	Mplayer_UI_Interaction interaction = ui_button(string);
	if (mplayer_ctx->mode_stack->mode != target_mode && interaction.clicked_left)
	{
		mplayer_change_mode(target_mode);
	}
	
	if (mplayer_ctx->mode_stack->mode == target_mode) ui_parent(interaction.element) 
		ui_pref_width(ui_size_by_childs(1)) ui_horizontal_layout()
	{
		ui_spacer_pixels(5, 1);
		ui_vertical_layout() ui_padding(ui_size_parent_remaining())
		{
			ui_next_width(ui_size_pixel(5, 1));
			ui_next_height(ui_size_percent(0.9f, 1));
			ui_next_background_color(vec4(1, 0, 0, 1));
			ui_next_roundness(2.5);
			ui_next_layer(UI_Layer_Indicators);
			ui_element(str8_lit("mode-indicator"), UI_FLAG_Draw_Background | UI_FLAG_Animate_Pos | UI_FLAG_Animate_Dim);
		}
	}
}

//~ NOTE(fakhri): Playlists stuff

internal void
mplayer_add_track_id_to_list(Mplayer_Playlists *playlists, Mplayer_Track_ID_List *list, Mplayer_Track_ID track_id)
{
	Mplayer_Track_ID_Entry *entry = 0;
	if (playlists->track_id_entries_free_list)
	{
		entry = playlists->track_id_entries_free_list;
		playlists->track_id_entries_free_list = playlists->track_id_entries_free_list->next;
		memory_zero_struct(entry);
	}
	if (!entry)
	{
		entry = m_arena_push_struct_z(&playlists->arena, Mplayer_Track_ID_Entry);
	}
	assert(entry);
	entry->track_id = track_id;
	DLLPushBack(list->first, list->last, entry);
	
	list->count += 1;
}

internal void
mplayer_byte_stream_read_track_ids_list(Mplayer_Playlists *playlists, Byte_Stream *bs, Mplayer_Track_ID_List *tracks)
{
	u32 count;
	byte_stream_read(bs, &count);
	for (u32 i = 0; i < count; i += 1)
	{
		Mplayer_Track_ID track_id;
		byte_stream_read(bs, &track_id);
		mplayer_add_track_id_to_list(playlists, tracks, track_id);
	}
}

internal void
mplayer_reset_playlists(Mplayer_Playlists *playlists)
{
	m_arena_free_all(&playlists->arena);
	memory_zero_struct(playlists);
}


#define MPLAYER_PLAYLISTS_PATH str8_lit("playlists.mplayer")
internal void
mplayer_save_playlists(Mplayer_Playlists *playlists)
{
	File_Handle *file = platform->open_file(MPLAYER_PLAYLISTS_PATH, File_Open_Write | File_Create_Always);
	mplayer_serialize(file, playlists->fav_tracks);
	mplayer_serialize(file, playlists->playlists_count);
	for (u32 playlist_index = 0; playlist_index < playlists->playlists_count; playlist_index += 1)
	{
		Mplayer_Playlist_ID playlist_id = playlists->playlist_ids[playlist_index];
		Mplayer_Playlist *playlist = mplayer_playlist_by_id(playlists, playlist_id);
		mplayer_serialize_playlist(file, playlist);
	}
	platform->close_file(file);
}


internal void
mplayer_load_playlists(Mplayer_Playlists *playlists)
{
	mplayer_reset_playlists(playlists);
	
	Buffer playlists_content = platform->load_entire_file(MPLAYER_PLAYLISTS_PATH, &playlists->arena);
	if (is_valid(playlists_content))
	{
		Byte_Stream bs = make_byte_stream(playlists_content);
		
		mplayer_byte_stream_read_track_ids_list(playlists, &bs, &playlists->fav_tracks);
		
		u32 playlists_count;
		byte_stream_read(&bs, &playlists_count);
		for (u32 playlist_index = 0; playlist_index < playlists_count; playlist_index += 1)
		{
			String8 playlist_name;
			byte_stream_read(&bs, &playlist_name);
			Mplayer_Playlist_ID playlist_id = mplayer_compute_playlist_id(playlist_name);
			Mplayer_Playlist *playlist = mplayer_playlist_by_id(playlists, playlist_id);
			
			assert(!playlist); // make sure there are no duplicate playlists
			playlist = m_arena_push_struct_z(&playlists->arena, Mplayer_Playlist);
			playlist->id = playlist_id;
			playlist->name = playlist_name;
			mplayer_add_playlist(playlists, playlist);
			
			mplayer_byte_stream_read_track_ids_list(playlists, &bs, &playlist->tracks);
		}
	}
}

internal b32
mplayer_track_in_list(Mplayer_Track_ID_List *list, Mplayer_Track_ID track_id)
{
	b32 in_list = 0;
	for(Mplayer_Track_ID_Entry *entry = list->first;
		entry;
		entry = entry->next)
	{
		if (is_equal(entry->track_id, track_id))
		{
			in_list = 1;
			break;
		}
	}
	return in_list;
}

internal void
mplayer_add_track_to_favorites(Mplayer_Playlists *playlists, Mplayer_Track_ID track_id)
{
	#if DEBUG_BUILD
		// NOTE(fakhri): assume that whoever calls us have checked that the track
		// was not already in the list
		assert(!mplayer_track_in_list(&playlists->fav_tracks, track_id));
	#endif
		mplayer_add_track_id_to_list(playlists, &playlists->fav_tracks, track_id);
	mplayer_save_playlists(playlists);
}

internal void
mplayer_remove_track_from_favorites(Mplayer_Playlists *playlists, Mplayer_Track_ID track_id)
{
	Mplayer_Track_ID_Entry *track_entry = 0;
	for(Mplayer_Track_ID_Entry *entry = playlists->fav_tracks.first;
		entry;
		entry = entry->next)
	{
		if (is_equal(entry->track_id, track_id))
		{
			track_entry = entry;
			break;
		}
	}
	
	if (track_entry)
	{
		DLLRemove(playlists->fav_tracks.first, playlists->fav_tracks.last, track_entry);
		playlists->fav_tracks.count -= 1;
		mplayer_save_playlists(playlists);
	}
}


internal
MPLAYER_GET_AUDIO_SAMPLES(mplayer_get_audio_samples)
{
	mplayer_update_queue();
	
	Mplayer_Track *track = mplayer_queue_get_current_track();
	if (track && mplayer_is_queue_playing())
	{
		Flac_Stream *flac_stream = track->flac_stream;
		f32 volume = mplayer_ctx->volume;
		if (flac_stream)
		{
			u32 channels_count = flac_stream->streaminfo.nb_channels;
			
			// TODO(fakhri): convert to device channel layout
			assert(channels_count == device_config.channels_count);
			
			Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
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

internal
MPLAYER_HOTLOAD(mplayer_hotload)
{
	mplayer_ctx = _mplayer;
	g_input = input;
	platform = vtable;
	fnt_set_fonts_context(mplayer_ctx->fonts_ctx);
	ui_set_context(mplayer_ctx->ui);
}

internal
MPLAYER_INITIALIZE(mplayer_initialize)
{
	platform = vtable;
	g_input = input;
	
	mplayer_ctx = (Mplayer_Context *)_m_arena_bootstrap_push(gigabytes(1), member_offset(Mplayer_Context, main_arena));
	mplayer_ctx->entropy = rng_make_linear((u32)time(0));
	
	mplayer_ctx->frame_arena = frame_arena;
	mplayer_ctx->render_ctx = render_ctx;
	mplayer_ctx->fonts_ctx = fnt_init(render_ctx);
	mplayer_ctx->font = fnt_open_font(str8_lit("data/fonts/arial.ttf"));
	mplayer_ctx->volume = 1.0f;
	
	mplayer_ctx->ui = ui_init(mplayer_ctx->font);
	
	// NOTE(fakhri): ctx menu ids
	{
		mplayer_ctx->ctx_menu_ids[Track_Context_Menu] = ui_id_from_string(UI_NULL_ID, str8_lit("track-ctx-menu-id"));
		mplayer_ctx->ctx_menu_ids[Queue_Track_Context_Menu] = ui_id_from_string(UI_NULL_ID, str8_lit("queue-track-ctx-menu-id"));
	}
	
	// NOTE(fakhri): modal menu ids
	{
		mplayer_ctx->modal_menu_ids[MODAL_Path_Lister] = ui_id_from_string(UI_NULL_ID, str8_lit("path-lister-modal-menu-id"));
		mplayer_ctx->modal_menu_ids[MODAL_Add_To_Playlist] = ui_id_from_string(UI_NULL_ID, str8_lit("add-to-playlist-modal-menu-id"));
		mplayer_ctx->modal_menu_ids[MODAL_Create_Playlist] = ui_id_from_string(UI_NULL_ID, str8_lit("create-playlist-modal-menu-id"));
	}
	
	// NOTE(fakhri): init buffer backed strings
	{
		mplayer_ctx->new_playlist_name = str8(mplayer_ctx->new_playlist_name_buffer, 0);
	}
	
	mplayer_init_queue();
	mplayer_load_settings();
	mplayer_load_library();
	mplayer_load_playlists(&mplayer_ctx->playlists);
	mplayer_change_mode(MODE_Track_Library);
	
	return mplayer_ctx;
}

exported
MPLAYER_UPDATE_AND_RENDER(mplayer_update_and_render)
{
	g_input->next_animation_timer_request = -1;
	if (mplayer_is_queue_playing())
	{
		// NOTE(fakhri): update each second
		g_input->next_animation_timer_request = 1;
	}
	
	Render_Context *render_ctx = mplayer_ctx->render_ctx;
	
	Range2_F32 screen_rect = range_center_dim(vec2(0, 0), render_ctx->draw_dim);
	Render_Group group = begin_render_group(render_ctx, vec2(0, 0), render_ctx->draw_dim, screen_rect);
	
	M4_Inv proj = group.config.proj;
	V2_F32 world_mouse_p = (proj.inv * vec4(g_input->mouse_clip_pos)).xy;
	push_clear_color(render_ctx, vec4(0.005f, 0.005f, 0.005f, 1));
	
	if (mplayer_mouse_button_clicked(g_input, Mouse_M4))
	{
		mplayer_change_previous_mode();
	}
	
	if (mplayer_mouse_button_clicked(g_input, Mouse_M5))
	{
		mplayer_change_next_mode();
	}
	
	ui_begin(&group, world_mouse_p);
	
	// NOTE(fakhri): context menus
	ui_pref_layer(UI_Layer_Ctx_Menu)
	{
		// NOTE(fakhri): track context menu
		ui_pref_seed({2109487})
		{
			ui_ctx_menu(mplayer_ctx->ctx_menu_ids[Track_Context_Menu]) 
				ui_pref_roundness(10) ui_pref_background_color(vec4(0.01f, 0.01f, 0.02f, 1.0f))
				ui_pref_width(ui_size_by_childs(1)) ui_pref_height(ui_size_by_childs(1))
				ui_horizontal_layout() ui_padding(ui_size_pixel(10,1))
			{
				Mplayer_Track_ID track_id = mplayer_ctx->ctx_menu_data.item_id.track_id;
				Mplayer_Track *track = mplayer_track_by_id(&mplayer_ctx->library, track_id);
				
				ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
					ui_pref_width(ui_size_text_dim(1)) ui_pref_height(ui_size_text_dim(1))
				{
					ui_next_font_size(25);
					ui_label(track->title);
					
					ui_spacer_pixels(20, 1);
					
					ui_pref_font_size(18)
					{
						if (mplayer_ui_underlined_button(str8_lit("Play")).clicked_left)
						{
							mplayer_queue_play_track(track_id);
							mplayer_queue_resume();
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Play Next")).clicked_left)
						{
							mplayer_queue_next(track_id);
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Queue")).clicked_left)
						{
							mplayer_queue_last(track_id);
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(15,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Album")).clicked_left)
						{
							mplayer_change_mode(MODE_Album_Tracks, to_item_id(track->album_id));
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Artist")).clicked_left)
						{
							mplayer_change_mode(MODE_Artist_Albums, to_item_id(track->artist_id));
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(15,1);
						
						if (!mplayer_track_in_list(&mplayer_ctx->playlists.fav_tracks, track_id))
						{
							if (mplayer_ui_underlined_button(str8_lit("Add to Favorites")).clicked_left)
							{
								mplayer_add_track_to_favorites(&mplayer_ctx->playlists, track_id);
								ui_close_ctx_menu();
							}
						}
						else
						{
							if (mplayer_ui_underlined_button(str8_lit("Remove From Favorites")).clicked_left)
							{
								mplayer_remove_track_from_favorites(&mplayer_ctx->playlists, track_id);
								ui_close_ctx_menu();
							}
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Add to Playlist")).clicked_left)
						{
							ui_close_ctx_menu();
							mplayer_ctx->track_to_add_to_playlist = track_id;
							ui_open_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Add_To_Playlist]);
						}
						ui_spacer_pixels(5,1);
						
						ui_spacer_pixels(20, 1);
						if (mplayer_ui_underlined_button(str8_lit("Edit Info")).clicked_left)
						{
							// TODO(fakhri): open a model to edit track info
						}
					}
				}
			}
			
			
			
			ui_ctx_menu(mplayer_ctx->ctx_menu_ids[Queue_Track_Context_Menu]) 
				ui_pref_roundness(10) ui_pref_background_color(vec4(0.01f, 0.01f, 0.02f, 1.0f))
				ui_pref_width(ui_size_by_childs(1)) ui_pref_height(ui_size_by_childs(1))
				ui_horizontal_layout() ui_padding(ui_size_pixel(10,1))
			{
				Mplayer_Queue_Index index = mplayer_ctx->ctx_menu_data.queue_index;;
				Mplayer_Track *track = mplayer_queue_get_track_from_queue_index(index);
				Mplayer_Track_ID track_id = track->hash;
				
				ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
					ui_pref_width(ui_size_text_dim(1)) ui_pref_height(ui_size_text_dim(1))
				{
					ui_next_font_size(25);
					ui_label(track->title);
					
					ui_spacer_pixels(20, 1);
					
					ui_pref_font_size(18)
					{
						if (mplayer_ui_underlined_button(str8_lit("Play")).clicked_left)
						{
							mplayer_queue_play_track(track_id);
							mplayer_queue_resume();
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Remove From Queue")).clicked_left)
						{
							mplayer_queue_remove_at(index);
							mplayer_queue_resume();
							ui_close_ctx_menu();
						}
						
						ui_spacer_pixels(5,1);
						if (mplayer_ui_underlined_button(str8_lit("Play Next")).clicked_left)
						{
							mplayer_queue_next(track_id);
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Queue")).clicked_left)
						{
							mplayer_queue_last(track_id);
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(15,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Album")).clicked_left)
						{
							mplayer_change_mode(MODE_Album_Tracks, to_item_id(track->album_id));
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Artist")).clicked_left)
						{
							mplayer_change_mode(MODE_Artist_Albums, to_item_id(track->artist_id));
							ui_close_ctx_menu();
						}
						ui_spacer_pixels(15,1);
						
						if (!mplayer_track_in_list(&mplayer_ctx->playlists.fav_tracks, track_id))
						{
							if (mplayer_ui_underlined_button(str8_lit("Add to Favorites")).clicked_left)
							{
								mplayer_add_track_to_favorites(&mplayer_ctx->playlists, track_id);
								ui_close_ctx_menu();
							}
						}
						else
						{
							if (mplayer_ui_underlined_button(str8_lit("Remove From Favorites")).clicked_left)
							{
								mplayer_remove_track_from_favorites(&mplayer_ctx->playlists, track_id);
								ui_close_ctx_menu();
							}
						}
						ui_spacer_pixels(5,1);
						
						if (mplayer_ui_underlined_button(str8_lit("Add to Playlist")).clicked_left)
						{
							ui_close_ctx_menu();
							mplayer_ctx->track_to_add_to_playlist = track_id;
							ui_open_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Add_To_Playlist]);
						}
						ui_spacer_pixels(5,1);
						
						ui_spacer_pixels(20, 1);
						if (mplayer_ui_underlined_button(str8_lit("Edit Info")).clicked_left)
						{
							// TODO(fakhri): open a model to edit track info
						}
					}
				}
			}
			
			
		}
	}
	
	// NOTE(fakhri): modal menus
	ui_pref_layer(UI_Layer_Popup)
	{
		// NOTE(fakhri): path lister
		ui_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Path_Lister]) 
			ui_pref_seed({982691})
			ui_vertical_layout() ui_padding(ui_size_parent_remaining())
			ui_pref_height(ui_size_by_childs(1)) ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
		{
			ui_next_border_color(vec4(0,0,0,1));
			ui_next_border_thickness(2);
			ui_next_background_color(vec4(0.01f, 0.01f, 0.01f, 1.0f));
			ui_next_width(ui_size_percent(0.9f, 1));
			ui_next_height(ui_size_percent(0.9f, 1));
			ui_next_flags(UI_FLAG_Clip);
			ui_vertical_layout() ui_padding(ui_size_pixel(30, 1))
			{
				ui_next_height(ui_size_pixel(30, 1));
				ui_horizontal_layout() ui_padding(ui_size_pixel(20, 1))
					ui_pref_height(ui_size_percent(1, 1))
				{
					ui_next_width(ui_size_text_dim(1));
					ui_label(str8_lit("Input path:"));
					
					ui_spacer_pixels(10, 1);
					
					ui_next_border_color(vec4(0,0,0,1));
					ui_next_border_thickness(2);
					ui_next_background_color(vec4(0.05f, 0.05f, 0.05f, 1.0f));
					if (ui_input_field(str8_lit("path-input-field"), &mplayer_ctx->path_lister.user_path, sizeof(mplayer_ctx->path_lister.user_path_buffer)))
					{
						mplayer_refresh_path_lister_dir(&mplayer_ctx->path_lister);
					}
				}
				ui_spacer_pixels(10, 1);
				
				ui_next_border_thickness(2);
				ui_next_height(ui_size_by_childs(1));
				ui_horizontal_layout() ui_padding(ui_size_pixel(50, 0))
				{
					u32 selected_option_index = mplayer_ctx->path_lister.filtered_paths_count;
					
					ui_next_height(ui_size_percent(0.75f, 0));
					ui_for_each_list_item(str8_lit("path-lister-options"), mplayer_ctx->path_lister.filtered_paths_count, 40.0f, 1.0f, option_index)
						ui_pref_height(ui_size_percent(1,0)) ui_pref_width(ui_size_percent(1, 0))
					{
						String8 option_path = mplayer_ctx->path_lister.filtered_paths[option_index];
						
						UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
						
						ui_next_roundness(5);
						ui_next_background_color(vec4(0.05f, 0.05f,0.05f, 1));
						ui_next_hover_cursor(Cursor_Hand);
						UI_Element *option_el = ui_element_f(flags, "path-option-%.*s", STR8_EXPAND(option_path));
						Mplayer_UI_Interaction interaction = ui_interaction_from_element(option_el);
						if (interaction.hover)
						{
							option_el->background_color = vec4(0.15f, 0.15f,0.15f, 1);
						}
						if (interaction.pressed_left)
						{
							option_el->background_color = vec4(0.12f, 0.12f,0.12f, 1);
							if (interaction.hover)
							{
								option_el->background_color = vec4(0.1f, 0.1f,0.1f, 1);
							}
						}
						if (interaction.clicked_left)
						{
							selected_option_index = option_index;
						}
						
						ui_parent(option_el) ui_horizontal_layout()
						{
							ui_spacer_pixels(10, 1);
							ui_next_width(ui_size_text_dim(1));
							ui_label(option_path);
						}
					}
					
					if (selected_option_index < mplayer_ctx->path_lister.filtered_paths_count)
					{
						mplayer_set_path_lister_path(&mplayer_ctx->path_lister, mplayer_ctx->path_lister.filtered_paths[selected_option_index]);
					}
				}
				
				ui_spacer(ui_size_parent_remaining());
				
				ui_next_height(ui_size_by_childs(1));
				ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
				{
					ui_next_height(ui_size_text_dim(1));
					ui_next_width(ui_size_text_dim(1));
					if (mplayer_ui_underlined_button(str8_lit("Ok")).clicked_left)
					{
						if (mplayer_ctx->settings.locations_count < MAX_LOCATION_COUNT)
						{
							Mplayer_Library_Location *location = mplayer_ctx->settings.locations + mplayer_ctx->settings.locations_count++;
							memory_copy(location->path, mplayer_ctx->path_lister.user_path.str, mplayer_ctx->path_lister.user_path.len);
							location->path_len = u32(mplayer_ctx->path_lister.user_path.len);
						}
						
						mplayer_close_path_lister(&mplayer_ctx->path_lister);
						mplayer_save_settings();
					}
					
					ui_spacer(ui_size_parent_remaining());
					ui_next_height(ui_size_text_dim(1));
					ui_next_width(ui_size_text_dim(1));
					if (mplayer_ui_underlined_button(str8_lit("Cancel")).clicked_left)
					{
						mplayer_close_path_lister(&mplayer_ctx->path_lister);
					}
					
				}
			}
		}
		
		ui_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Add_To_Playlist])
			ui_vertical_layout() ui_padding(ui_size_parent_remaining())
			ui_pref_height(ui_size_by_childs(1)) ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
		{
			ui_next_border_color(vec4(0,0,0,1));
			ui_next_border_thickness(2);
			ui_next_background_color(vec4(0.05f, 0.05f, 0.05f, 1.0f));
			ui_next_width(ui_size_percent(0.7f, 1));
			ui_next_height(ui_size_percent(0.7f, 1));
			ui_next_flags(UI_FLAG_Clip);
			ui_vertical_layout() ui_padding(ui_size_pixel(30, 1))
			{
				ui_horizontal_layout() 
					ui_padding(ui_size_pixel(100, 1))
					ui_pref_height(ui_size_text_dim(1))
				{
					ui_next_font_size(40);
					ui_label(str8_lit("Select Playlist"));
				}
				
				// TODO(fakhri): add a way to search for playlist
				
				ui_spacer(ui_size_pixel(5, 1));
				
				ui_next_height(ui_size_parent_remaining());
				ui_next_flags(UI_FLAG_Clip);
				ui_next_background_color(vec4(0.01f, 0.01f, 0.01f, 1));
				ui_horizontal_layout() ui_padding(ui_size_pixel(50, 1))
					ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
				{
					ui_for_each_grid_item(str8_lit("library-playlists-select-playlist-grid"), mplayer_ctx->playlists.playlists_count, vec2(200.0f, 200.0f), 10, playlist_index)
					{
						Mplayer_Playlist_ID playlist_id = mplayer_ctx->playlists.playlist_ids[playlist_index];
						Mplayer_UI_Interaction interaction = mplayer_ui_playlist_item(playlist_id);
						if (interaction.clicked_left)
						{
							Mplayer_Playlist *playlist = mplayer_playlist_by_id(&mplayer_ctx->playlists, playlist_id);
							mplayer_add_track_id_to_list(&mplayer_ctx->playlists, &playlist->tracks, mplayer_ctx->track_to_add_to_playlist);
							mplayer_ctx->track_to_add_to_playlist = NULL_TRACK_ID;
							
							mplayer_save_playlists(&mplayer_ctx->playlists);
							ui_close_modal_menu();
						}
					}
				}
				ui_spacer(ui_size_pixel(20, 1));
				
				ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
					ui_pref_height(ui_size_text_dim(1))
					ui_pref_width(ui_size_text_dim(1))
				{
					if (mplayer_ui_underlined_button(str8_lit("*Create New Playlist*")).clicked_left)
					{
						mplayer_ctx->add_track_to_new_playlist = 1;
						mplayer_ctx->new_playlist_name.len = 0;
						ui_open_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Create_Playlist]);
					}
					ui_spacer(ui_size_pixel(50, 1));
					
					if (mplayer_ui_underlined_button(str8_lit("Cancel")).clicked_left)
					{
						ui_close_modal_menu();
					}
				}
				
			}
		}
		
		ui_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Create_Playlist])
			ui_vertical_layout() ui_padding(ui_size_parent_remaining())
			ui_pref_height(ui_size_by_childs(1)) ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
		{
			ui_next_border_color(vec4(0,0,0,1));
			ui_next_border_thickness(2);
			ui_next_background_color(vec4(0.02f, 0.02f, 0.02f, 1.0f));
			ui_next_width(ui_size_percent(0.6f, 1));
			ui_next_height(ui_size_by_childs(1));
			ui_next_flags(UI_FLAG_Clip);
			ui_vertical_layout() ui_padding(ui_size_pixel(20, 1))
			{
				ui_horizontal_layout() 
					ui_padding(ui_size_pixel(100, 1))
					ui_pref_height(ui_size_text_dim(1))
				{
					ui_next_font_size(40);
					ui_label(str8_lit("Create New Playlist"));
				}
				
				ui_spacer(ui_size_pixel(20, 1));
				
				ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
					ui_pref_width(ui_size_percent(0.9f, 1))
				{
					ui_next_height(ui_size_pixel(30, 1));
					ui_next_roundness(5);
					ui_next_background_color(vec4(0.05f, 0.05f, 0.05f, 1.0f));
					ui_input_field(str8_lit("new-playlist-name-input-field"), &mplayer_ctx->new_playlist_name, sizeof(mplayer_ctx->new_playlist_name_buffer));
				}
				
				ui_spacer(ui_size_pixel(20, 1));
				
				ui_horizontal_layout() ui_padding(ui_size_parent_remaining())
					ui_pref_height(ui_size_text_dim(1))
					ui_pref_width(ui_size_text_dim(1))
				{
					if (mplayer_ui_underlined_button(str8_lit("Create")).clicked_left && mplayer_ctx->new_playlist_name.len)
					{
						b32 should_save_library = 0;
						
						Mplayer_Playlist_ID playlist_id = mplayer_compute_playlist_id(mplayer_ctx->new_playlist_name);
						Mplayer_Playlist *playlist = mplayer_playlist_by_id(&mplayer_ctx->playlists, playlist_id);
						if (!playlist)
						{
							playlist = m_arena_push_struct_z(&mplayer_ctx->library.arena, Mplayer_Playlist);
							playlist->id = playlist_id;
							playlist->name = str8_clone(&mplayer_ctx->library.arena, mplayer_ctx->new_playlist_name);
							mplayer_add_playlist(&mplayer_ctx->playlists, playlist);
							should_save_library = 1;
						}
						
						if (mplayer_ctx->add_track_to_new_playlist)
						{
							mplayer_add_track_id_to_list(&mplayer_ctx->playlists, &playlist->tracks, mplayer_ctx->track_to_add_to_playlist);
							
							mplayer_ctx->track_to_add_to_playlist = NULL_TRACK_ID;
							mplayer_ctx->add_track_to_new_playlist = 0;
							should_save_library = 1;
						}
						
						if (should_save_library)
						{
							mplayer_save_playlists(&mplayer_ctx->playlists);
						}
						ui_close_modal_menu();
					}
					
					ui_spacer(ui_size_pixel(20, 1));
					if (mplayer_ui_underlined_button(str8_lit("Cancel")).clicked_left)
					{
						ui_close_modal_menu();
					}
				}
				
			}
		}
	}
	
	ui_vertical_layout()
	{
		ui_pref_height(ui_size_parent_remaining())
			ui_horizontal_layout()
		{
			ui_next_background_color(vec4(0.015f, 0.015f, 0.015f, 1.0f));
			ui_next_width(ui_size_pixel(150, 1));
			ui_next_border_color(vec4(0.f, 0.f, 0.f, 1.0f));
			ui_next_border_thickness(2);
			ui_vertical_layout()
			{
				ui_pref_background_color(vec4(0.04f, 0.04f, 0.04f, 1)) ui_pref_text_color(vec4(1, 1, 1, 1))
					ui_pref_width(ui_size_percent(1, 1)) ui_pref_height(ui_size_pixel(30, 1)) ui_pref_roundness(15)
				{
					ui_spacer_pixels(20, 1);
					mplayer_ui_side_bar_button(str8_lit("Tracks"), MODE_Track_Library);
					
					ui_spacer_pixels(2, 1);
					mplayer_ui_side_bar_button(str8_lit("Favorites"), MODE_Favorites);
					
					ui_spacer_pixels(2, 1);
					mplayer_ui_side_bar_button(str8_lit("Playlists"), MODE_Playlists);
					
					ui_spacer_pixels(2, 1);
					mplayer_ui_side_bar_button(str8_lit("Artists"), MODE_Artist_Library);
					
					ui_spacer_pixels(2, 1);
					mplayer_ui_side_bar_button(str8_lit("Albums"), MODE_Album_Library);
					
					ui_spacer_pixels(2, 1);
					mplayer_ui_side_bar_button(str8_lit("Lyrics"), MODE_Lyrics);
					
					ui_spacer_pixels(2, 1);
					mplayer_ui_side_bar_button(str8_lit("Queue"), MODE_Queue);
					
					ui_spacer(ui_size_parent_remaining());
					mplayer_ui_side_bar_button(str8_lit("Settings"), MODE_Settings);
					
					ui_spacer_pixels(20, 1);
				}
			}
			
			V4_F32 header_bg_color = vec4(0.009f, 0.009f, 0.009f, 1);
			ui_next_width(ui_size_parent_remaining());
			ui_vertical_layout()
			{
				Mplayer_Item_ID item_id = mplayer_ctx->mode_stack->id;
				switch(mplayer_ctx->mode_stack->mode)
				{
					case MODE_Artist_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(69, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 0);
							
							ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
							{
								ui_next_background_color(vec4(1, 1, 1, 0));
								ui_next_text_color(vec4(1, 1, 1, 1));
								ui_next_font_size(50);
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								ui_label(str8_lit("Artists"));
							}
						}
						
						
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 1))
						{
							u32 selected_artist_id = 0;
							ui_for_each_grid_item(str8_lit("library-artists-grid"), mplayer_ctx->library.artists_count, vec2(250.0f, 220.0f), 10, artist_index)
							{
								Mplayer_Artist_ID artist_id = mplayer_ctx->library.artist_ids[artist_index];
								mplayer_ui_aritst_item(artist_id);
							}
						}
						
					} break;
					
					case MODE_Album_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(69, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 1);
							
							ui_next_background_color(vec4(1, 1, 1, 0));
							ui_next_text_color(vec4(1, 1, 1, 1));
							ui_next_width(ui_size_text_dim(1));
							ui_next_font_size(50);
							ui_label(str8_lit("Albums"));
						}
						
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 1))
						{
							
							ui_for_each_grid_item(str8_lit("library-albums-grid"), mplayer_ctx->library.albums_count, vec2(300.0f, 300.0f), 10, album_index)
							{
								Mplayer_Album_ID album_id = mplayer_ctx->library.album_ids[album_index];
								mplayer_ui_album_item(album_id);
							}
						}
						
					} break;
					
					case MODE_Playlists:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(100, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 1);
							
							ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
							{
								ui_next_background_color(vec4(1, 1, 1, 0));
								ui_next_text_color(vec4(1, 1, 1, 1));
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								ui_next_font_size(50);
								ui_label(str8_lit("Playlists"));
								
								ui_spacer(ui_size_parent_remaining());
								ui_next_height(ui_size_by_childs(1));
								ui_horizontal_layout()
									ui_pref_height(ui_size_text_dim(1))
									ui_pref_width(ui_size_text_dim(1))
								{
									ui_spacer_pixels(10, 1);
									if (mplayer_ui_underlined_button(str8_lit("Create Playlist")).clicked_left)
									{
										mplayer_ctx->new_playlist_name.len = 0;
										ui_open_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Create_Playlist]);
									}
									
									ui_spacer_pixels(10, 1);
								}
							}
						}
						
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 1))
						{
							ui_for_each_grid_item(str8_lit("library-playlists-grid"), mplayer_ctx->playlists.playlists_count, vec2(300.0f, 300.0f), 10, playlist_index)
							{
								Mplayer_Playlist_ID playlist_id = mplayer_ctx->playlists.playlist_ids[playlist_index];
								Mplayer_UI_Interaction interaction = mplayer_ui_playlist_item(playlist_id);
								if (interaction.clicked_left)
								{
									mplayer_change_mode(MODE_Playlist_Tracks, to_item_id(playlist_id));
								}
							}
						}
						
					} break;
					
					case MODE_Track_Library:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(90, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 1);
							
							ui_next_height(ui_size_by_childs(1));
							ui_next_width(ui_size_parent_remaining());
							ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
							{
								ui_next_background_color(vec4(1, 1, 1, 0));
								ui_next_text_color(vec4(1, 1, 1, 1));
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								ui_next_font_size(50);
								ui_label(str8_lit("Tracks"));
								
								ui_spacer_pixels(5, 1);
								
								ui_next_height(ui_size_by_childs(1));
								ui_horizontal_layout()
									ui_pref_font_size(20)
									ui_pref_height(ui_size_text_dim(1))
									ui_pref_width(ui_size_text_dim(1))
								{
									ui_spacer_pixels(20, 1);
									if (mplayer_ui_underlined_button(str8_lit("Queue All")).clicked_left)
									{
										for (u32 track_index = 0; track_index < mplayer_ctx->library.tracks_count; track_index += 1)
										{
											mplayer_queue_last(mplayer_ctx->library.track_ids[track_index]);
										}
										mplayer_queue_resume();
									}
									
									ui_spacer_pixels(10, 1);
								}
							}
						}
						
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 0))
						{
							ui_for_each_list_item(str8_lit("library-tracks-list"), mplayer_ctx->library.tracks_count, 50.0f, 1.0f, track_index)
							{
								Mplayer_Track_ID track_id = mplayer_ctx->library.track_ids[track_index];
								Mplayer_UI_Interaction interaction = mplayer_ui_track_item(track_id);
								if (interaction.clicked_left)
								{
									mplayer_queue_play_track(track_id);
									mplayer_queue_resume();
								}
								if (interaction.clicked_right)
								{
									ui_open_ctx_menu(g_ui->mouse_pos, mplayer_ctx->ctx_menu_ids[Track_Context_Menu]);
									mplayer_ctx->ctx_menu_data.item_id = to_item_id(track_id);
								}
							}
						}
					} break;
					
					case MODE_Artist_Albums:
					{
						Mplayer_Artist *artist = mplayer_artist_by_id(&mplayer_ctx->library, item_id.artist_id);
						
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(90, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 1);
							
							ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
							{
								ui_next_background_color(vec4(1, 1, 1, 0));
								ui_next_text_color(vec4(1, 1, 1, 1));
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								ui_next_font_size(50);
								ui_label(artist->name);
								
								ui_spacer(ui_size_parent_remaining());
								
								ui_next_height(ui_size_by_childs(1));
								ui_horizontal_layout()
									ui_pref_height(ui_size_text_dim(1))
									ui_pref_width(ui_size_text_dim(1))
								{
									ui_spacer_pixels(10, 1);
									if (mplayer_ui_underlined_button(str8_lit("Queue All")).clicked_left)
									{
										for (Mplayer_Track *track = artist->tracks.first; 
											track; 
											track = track->next_artist)
										{
											mplayer_queue_last(track->hash);
										}
										mplayer_queue_resume();
									}
									
									ui_spacer_pixels(10, 1);
								}
							}
						}
						
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 1))
						{
							u32 item_index = ui_grid_begin(str8_lit("artist-albums-grid"), artist->albums.count, vec2(300.0f, 300.0f), 10);
							defer(ui_grid_end())
							{
								Mplayer_Album *first_album = 0;
								{
									u32 count; for(count = 0, first_album = artist->albums.first; 
										count < item_index && first_album; 
										count += 1, first_album = first_album->next_artist);
								}
								for(Mplayer_Album *album = first_album; ui_grid_item_begin(); (album = album->next_artist, ui_grid_item_end()))
								{
									mplayer_ui_album_item(album->hash);
								}
							}
						}
						
					} break;
					
					case MODE_Album_Tracks:
					{
						Mplayer_Album *album = mplayer_album_by_id(&mplayer_ctx->library, item_id.album_id);
						Mplayer_Item_Image *image = mplayer_get_image_by_id(album->image_id, 1);
						
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(200, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(10, 1);
							
							ui_vertical_layout()
							{
								ui_spacer_pixels(10, 1);
								
								ui_horizontal_layout()
								{
									// NOTE(fakhri): album cover image
									{
										UI_Element_Flags flags = 0;
										if (mplayer_item_image_ready(*image))
										{
											flags |= UI_FLAG_Draw_Image;
											ui_next_texture_tint_color(vec4(1, 1, 1, 1));
											ui_next_texture(image->texture);
										}
										else
										{
											flags |= UI_FLAG_Draw_Background;
											ui_next_background_color(vec4(0.2f, 0.2f, 0.2f, 1.0f));
										}
										
										ui_next_roundness(10);
										ui_next_width(ui_size_pixel(180, 1));
										ui_next_height(ui_size_pixel(180, 1));
										ui_element({}, flags);
									}
									
									ui_spacer_pixels(10, 1);
									ui_vertical_layout()
									{
										ui_spacer_pixels(30, 1);
										ui_next_width(ui_size_text_dim(1));
										ui_next_height(ui_size_text_dim(1));
										ui_next_font_size(19);
										ui_label(str8_lit("Album"));
										
										ui_next_width(ui_size_text_dim(1));
										ui_next_height(ui_size_text_dim(1));
										ui_next_font_size(40);
										ui_label(album->name);
										
										//ui_spacer_pixels(15, 1);
										
										// NOTE(fakhri): artist button
										{
											ui_next_width(ui_size_text_dim(1));
											ui_next_height(ui_size_text_dim(1));
											ui_next_font_size(20);
											ui_next_text_color(vec4(0.4f, 0.4f, 0.4f, 1));
											Mplayer_UI_Interaction interaction = mplayer_ui_underlined_button_f("%.*s###_album_artist_button", STR8_EXPAND(album->artist));
											if (interaction.hover)
											{
												interaction.element->text_color = vec4(0.6f, 0.6f, 0.6f, 1);
											}
											if (interaction.pressed_left)
											{
												interaction.element->text_color = vec4(1, 1, 1, 1);
											}
											
											if (interaction.clicked_left)
											{
												mplayer_change_mode(MODE_Artist_Albums, to_item_id(album->artist_id));
											}
										}
										
										ui_spacer(ui_size_parent_remaining());
										
										// NOTE(fakhri): queue album button
										{
											ui_next_width(ui_size_text_dim(1));
											ui_next_height(ui_size_text_dim(1));
											ui_next_text_color(vec4(0.6f, 0.6f, 0.6f, 1));
											ui_next_font_size(18);
											Mplayer_UI_Interaction interaction = mplayer_ui_underlined_button_f("Queue Album");
											if (interaction.pressed_left)
											{
												interaction.element->text_color = vec4(1, 1, 1, 1);
											}
											
											if (interaction.clicked_left)
											{
												for (Mplayer_Track *track = album->tracks.first; 
													track; 
													track = track->next_album)
												{
													mplayer_queue_last(track->hash);
												}
												mplayer_queue_resume();
											}
										}
										ui_spacer_pixels(10, 1);
									}
								}
								ui_spacer_pixels(10, 1);
							}
						}
						ui_spacer_pixels(20, 0);
						
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 0);
							
							u32 item_index = ui_list_begin(str8_lit("album-tracks-list"), album->tracks.count, 50.0f, 1.0f);
							defer(ui_list_end())
							{
								Mplayer_Track *first_track = 0;
								{
									u32 count; for(count = 0, first_track = album->tracks.first; 
										count < item_index && first_track; 
										count += 1, first_track = first_track->next_album);
								}
								for(Mplayer_Track *track = first_track; ui_list_item_begin(); (track = track->next_album, ui_list_item_end()))
								{
									Mplayer_Track_ID track_id = track->hash;
									Mplayer_UI_Interaction interaction = mplayer_ui_track_item(track_id);
									if (interaction.clicked_left)
									{
										mplayer_queue_play_track(track_id);
										mplayer_queue_resume();
									}
									if (interaction.clicked_right)
									{
										ui_open_ctx_menu(g_ui->mouse_pos, mplayer_ctx->ctx_menu_ids[Track_Context_Menu]);
										mplayer_ctx->ctx_menu_data.item_id = to_item_id(track_id);
									}
								}
							}
							
							ui_spacer_pixels(50, 0);
						}
						ui_spacer_pixels(10, 1);
					} break;
					
					case MODE_Playlist_Tracks:
					{
						Mplayer_Playlist *playlist = mplayer_playlist_by_id(&mplayer_ctx->playlists, item_id.playlist_id);
						
						// TODO(fakhri): playlist image 
						
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(200, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(10, 1);
							
							ui_vertical_layout()
							{
								ui_spacer_pixels(10, 1);
								
								ui_horizontal_layout()
								{
									// NOTE(fakhri): playlist cover image
									{
										UI_Element_Flags flags = 0;
										flags |= UI_FLAG_Draw_Background;
										ui_next_background_color(vec4(0.2f, 0.2f, 0.2f, 1.0f));
										
										ui_next_roundness(10);
										ui_next_width(ui_size_pixel(180, 1));
										ui_next_height(ui_size_pixel(180, 1));
										ui_element({}, flags);
									}
									
									ui_spacer_pixels(10, 1);
									ui_vertical_layout() 
									{
										ui_spacer_pixels(30, 1);
										ui_next_width(ui_size_text_dim(1));
										ui_next_height(ui_size_text_dim(1));
										ui_next_font_size(19);
										ui_label(str8_lit("Playlist"));
										
										ui_padding(ui_size_parent_remaining())
										{
											ui_next_width(ui_size_text_dim(1));
											ui_next_height(ui_size_text_dim(1));
											ui_next_font_size(40);
											ui_label(playlist->name);
											
											ui_spacer_pixels(10, 1);
											
											// NOTE(fakhri): queue playlist button
											{
												ui_next_width(ui_size_text_dim(1));
												ui_next_height(ui_size_text_dim(1));
												ui_next_text_color(vec4(0.6f, 0.6f, 0.6f, 1));
												ui_next_font_size(18);
												Mplayer_UI_Interaction interaction = mplayer_ui_underlined_button_f("Queue Playlist");
												if (interaction.pressed_left)
												{
													interaction.element->text_color = vec4(1, 1, 1, 1);
												}
												
												if (interaction.clicked_left)
												{
													mplayer_clear_queue();
													for(Mplayer_Track_ID_Entry *entry = playlist->tracks.first; entry; entry = entry->next)
													{
														mplayer_queue_last(entry->track_id);
													}
													mplayer_queue_resume();
												}
											}
										}
									}
								}
								ui_spacer_pixels(10, 1);
							}
						}
						ui_spacer_pixels(20, 0);
						
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 0);
							
							u32 item_index = ui_list_begin(str8_lit("playlist-tracks-list"), playlist->tracks.count, 50.0f, 1.0f);
							defer(ui_list_end())
							{
								Mplayer_Track_ID_Entry *first_entry = 0;
								{
									u32 count; for(count = 0, first_entry = playlist->tracks.first; 
										count < item_index && first_entry; 
										count += 1, first_entry = first_entry->next);
								}
								for(Mplayer_Track_ID_Entry *entry = first_entry; ui_list_item_begin(); (entry = entry->next, ui_list_item_end()))
								{
									Mplayer_Track_ID track_id = entry->track_id;
									Mplayer_Track *track = mplayer_track_by_id(&mplayer_ctx->library, track_id);
									UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
									
									V4_F32 bg_color = vec4(0.02f, 0.02f,0.02f, 1);
									#if 1
										if (is_equal(track_id, mplayer_queue_current_track_id()))
									{
										bg_color = vec4(0, 0, 0, 1);
									}
									#endif
										
										ui_next_background_color(bg_color);
									ui_next_hover_cursor(Cursor_Hand);
									UI_Element *track_el = ui_element_f(flags, "library_track_%p", entry);
									Mplayer_UI_Interaction interaction = ui_interaction_from_element(track_el);
									if (interaction.clicked_left)
									{
										mplayer_queue_play_track(track_id);
										mplayer_queue_resume();
									}
									if (interaction.clicked_right)
									{
										ui_open_ctx_menu(g_ui->mouse_pos, mplayer_ctx->ctx_menu_ids[Track_Context_Menu]);
										mplayer_ctx->ctx_menu_data.item_id = to_item_id(track_id);
									}
									
									ui_parent(track_el) ui_vertical_layout() ui_padding(ui_size_parent_remaining())
										ui_horizontal_layout()
									{
										ui_spacer_pixels(10, 1);
										
										ui_next_width(ui_size_text_dim(1));
										ui_next_height(ui_size_text_dim(1));
										ui_element_f(UI_FLAG_Draw_Text, "%.*s##library_track_name_%p", 
											STR8_EXPAND(track->title), track);
									}
								}
							}
							
							ui_spacer_pixels(50, 0);
						}
						ui_spacer_pixels(10, 1);
					} break;
					
					case MODE_Lyrics:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(69, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 0);
							
							ui_next_background_color(vec4(1, 1, 1, 0));
							ui_next_text_color(vec4(1, 1, 1, 1));
							ui_next_width(ui_size_text_dim(1));
							ui_next_font_size(50);
							ui_label(str8_lit("Lyrics"));
						}
						
					} break;
					
					case MODE_Settings:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(69, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 0);
							
							ui_next_background_color(vec4(1, 1, 1, 0));
							ui_next_text_color(vec4(1, 1, 1, 1));
							ui_next_width(ui_size_text_dim(1));
							ui_next_font_size(50);
							ui_label(str8_lit("Settings"));
						}
						
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 1))
						{
							ui_next_childs_axis(Axis2_Y);
							
							UI_Element *setting_root_ui = ui_element(str8_lit("setting_scrollable_view_test"), UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Clip | UI_FLAG_Animate_Scroll);
							ui_parent(setting_root_ui)
								ui_padding(ui_size_pixel(20, 1))
							{
								ui_next_height(ui_size_by_childs(1));
								ui_horizontal_layout()
								{
									ui_next_width(ui_size_text_dim(1));
									ui_next_height(ui_size_text_dim(1));
									ui_label(str8_lit("Library"));
								}
								
								ui_spacer_pixels(10, 1);
								
								ui_next_width(ui_size_parent_remaining());
								ui_next_height(ui_size_by_childs(1));
								ui_parent(ui_element({}, 0))
								{
									ui_next_background_color(vec4(0.02f,0.02f,0.02f,1));
									ui_next_height(ui_size_pixel(60, 1));
									ui_next_roundness(5);
									UI_Element *add_location_el = ui_element(str8_lit("setting-track-library"), UI_FLAG_Draw_Background|UI_FLAG_Clickable);
									ui_parent(add_location_el) ui_padding(ui_size_parent_remaining()) ui_pref_height(ui_size_by_childs(1))
										ui_horizontal_layout() ui_padding(ui_size_pixel(20, 1))
									{
										Mplayer_UI_Interaction add_loc_interaction = ui_interaction_from_element(add_location_el);
										if (add_loc_interaction.hover)
										{
											add_location_el->background_color = vec4(0.069f,0.069f,0.069f,1);
										}
										if (add_loc_interaction.pressed_left)
										{
											add_location_el->background_color = vec4(0.12f,0.12f,0.12f,1);
										}
										if (add_loc_interaction.clicked_left)
										{
											mplayer_ctx->show_library_locations = !mplayer_ctx->show_library_locations;
										}
										
										ui_next_width(ui_size_text_dim(1));
										ui_next_height(ui_size_text_dim(1));
										ui_label(str8_lit("Track Library Locations"));
										
										ui_spacer(ui_size_parent_remaining());
										
										ui_next_roundness(5);
										ui_next_background_color(vec4(0.05f,0.05f,0.05f,1));
										ui_next_width(ui_size_by_childs(1));
										ui_next_height(ui_size_by_childs(1));
										ui_next_childs_axis(Axis2_Y);
										ui_next_hover_cursor(Cursor_Hand);
										UI_Element *loc_el = ui_element(str8_lit("add-location-button"), UI_FLAG_Draw_Background | UI_FLAG_Clickable);
										if (ui_interaction_from_element(loc_el).clicked_left)
										{
											mplayer_open_path_lister(&mplayer_ctx->path_lister);
										}
										
										ui_parent(loc_el) 
											ui_padding(ui_size_pixel(5, 1)) ui_pref_width(ui_size_by_childs(1))
											ui_horizontal_layout() ui_padding(ui_size_pixel(5, 1))
										{
											ui_next_width(ui_size_text_dim(1));
											ui_next_height(ui_size_text_dim(1));
											ui_label(str8_lit("Add Location"));
										}
									}
									
									u32 location_to_delete = mplayer_ctx->settings.locations_count;
									
									if (mplayer_ctx->show_library_locations) 
										ui_pref_background_color(vec4(0.05f,0.05f,0.05f,1))
										ui_pref_flags(UI_FLAG_Animate_Dim)
										ui_pref_height(ui_size_by_childs(1))
										ui_pref_roundness(10)
										ui_for_each_list_item(str8_lit("settings-location-list"), mplayer_ctx->settings.locations_count, 40.0f, 1.0f, location_index)
										
										ui_pref_height(ui_size_parent_remaining()) ui_pref_flags(0) ui_pref_roundness(100)
									{
										Mplayer_Library_Location *location = mplayer_ctx->settings.locations + location_index;
										ui_next_border_thickness(2);
										u32 flags = UI_FLAG_Animate_Dim | UI_FLAG_Clip | UI_FLAG_Draw_Background;
										UI_Element *location_el = ui_element({}, flags);
										location_el->child_layout_axis = Axis2_X;
										
										ui_parent(location_el) ui_padding(ui_size_pixel(10, 1))
										{
											ui_next_width(ui_size_text_dim(1));
											ui_label(str8(location->path, location->path_len));
											
											ui_spacer(ui_size_parent_remaining());
											
											ui_next_width(ui_size_pixel(20, 1));
											ui_vertical_layout() ui_padding(ui_size_parent_remaining())
											{
												ui_next_height(ui_size_pixel(20, 1));
												ui_next_roundness(10);
												ui_next_background_color(vec4(0.2f, 0.2f, 0.2f, 1));
												if (ui_button_f("X###delete-location-%p", location).clicked_left)
												{
													location_to_delete = location_index;
													mplayer_save_settings();
												}
											}
										}
									}
									
									if (location_to_delete < mplayer_ctx->settings.locations_count)
									{
										for (u32 i = location_to_delete; i < mplayer_ctx->settings.locations_count - 1; i += 1)
										{
											memory_copy_struct(&mplayer_ctx->settings.locations[i], &mplayer_ctx->settings.locations[i + 1]);
										}
										
										mplayer_ctx->settings.locations_count -= 1;
									}
								}
								
								ui_spacer_pixels(20,1);
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								if (mplayer_ui_underlined_button(str8_lit("Reindex Library")).clicked_left)
								{
									platform->set_cursor(Cursor_Wait);
									mplayer_index_library();
									platform->set_cursor(Cursor_Arrow);
								}
							}
							
							ui_interaction_from_element(setting_root_ui);
						}
					} break;
					
					case MODE_Queue:
					{
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(100, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 1);
							
							ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
							{
								ui_next_background_color(vec4(1, 1, 1, 0));
								ui_next_text_color(vec4(1, 1, 1, 1));
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								ui_next_font_size(50);
								String8 title = str8_f(mplayer_ctx->frame_arena, "Queue(%d)", mplayer_ctx->queue.count - 1);
								ui_label(title);
								
								ui_spacer(ui_size_parent_remaining());
								
								ui_next_height(ui_size_by_childs(1));
								ui_horizontal_layout()
									ui_pref_font_size(19)
									ui_pref_height(ui_size_text_dim(1))
									ui_pref_width(ui_size_text_dim(1))
								{
									if (mplayer_ui_underlined_button(str8_lit("Shuffle Queue")).clicked_left)
									{
										mplayer_queue_shuffle();
										mplayer_set_current(1);
									}
									ui_spacer_pixels(30, 1);
									
									if (mplayer_ui_underlined_button(str8_lit("Clear Queue")).clicked_left)
									{
										mplayer_clear_queue();
									}
									ui_spacer_pixels(10, 1);
									
								}
							}
						}
						
						ui_next_width(ui_size_parent_remaining());
						ui_next_height(ui_size_parent_remaining());
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 0))
						{
							ui_for_each_list_item(str8_lit("queue-tracks-list"), mplayer_ctx->queue.count-1, 50.0f, 1.0f, index)
							{
								Mplayer_Queue_Index queue_index = (Mplayer_Queue_Index)index + 1;
								Mplayer_Track *track = mplayer_track_by_id(&mplayer_ctx->library, mplayer_ctx->queue.tracks[queue_index]);
								
								V4_F32 bg_color = vec4(0.02f, 0.02f,0.02f, 1);
								if (queue_index == mplayer_ctx->queue.current_index)
								{
									bg_color = vec4(0, 0, 0, 1);
								}
								
								UI_Element_Flags flags = UI_FLAG_Draw_Background | UI_FLAG_Clickable | UI_FLAG_Draw_Border | UI_FLAG_Animate_Dim | UI_FLAG_Clip;
								
								ui_next_background_color(bg_color);
								ui_next_border_color(vec4(0, 0, 0, 1));
								ui_next_border_thickness(1);
								ui_next_hover_cursor(Cursor_Hand);
								UI_Element *track_el = ui_element_f(flags, "queue_track_%p%d", track, queue_index);
								Mplayer_UI_Interaction interaction = ui_interaction_from_element(track_el);
								if (interaction.clicked_left)
								{
									mplayer_set_current(Mplayer_Queue_Index(queue_index));
								}
								if (interaction.clicked_right)
								{
									ui_open_ctx_menu(g_ui->mouse_pos, mplayer_ctx->ctx_menu_ids[Queue_Track_Context_Menu]);
									mplayer_ctx->ctx_menu_data.queue_index = queue_index;
								}
								ui_parent(track_el) ui_vertical_layout() ui_padding(ui_size_parent_remaining())
									ui_horizontal_layout()
								{
									ui_spacer_pixels(10, 1);
									
									ui_next_width(ui_size_text_dim(1));
									ui_next_height(ui_size_text_dim(1));
									ui_element_f(UI_FLAG_Draw_Text, "%.*s##library_track_name_%p", 
										STR8_EXPAND(track->title), track);
								}
							}
						}
						
					} break;
					
					case MODE_Favorites:
					{
						Mplayer_Playlists *playlists = &mplayer_ctx->playlists;
						
						// NOTE(fakhri): header
						ui_next_background_color(header_bg_color);
						ui_next_height(ui_size_pixel(100, 1));
						ui_horizontal_layout()
						{
							ui_spacer_pixels(50, 1);
							
							ui_vertical_layout() ui_padding(ui_size_pixel(10, 1))
							{
								ui_next_background_color(vec4(1, 1, 1, 0));
								ui_next_text_color(vec4(1, 1, 1, 1));
								ui_next_width(ui_size_text_dim(1));
								ui_next_height(ui_size_text_dim(1));
								ui_next_font_size(50);
								ui_label(str8_f(mplayer_ctx->frame_arena, "Favorites(%d)", playlists->fav_tracks.count));
								
								ui_spacer(ui_size_parent_remaining());
								
								ui_next_height(ui_size_by_childs(1));
								ui_horizontal_layout()
									ui_pref_height(ui_size_text_dim(1))
									ui_pref_width(ui_size_text_dim(1))
								{
									ui_spacer_pixels(10, 1);
									if (mplayer_ui_underlined_button(str8_lit("Queue All")).clicked_left)
									{
										mplayer_clear_queue();
										for(Mplayer_Track_ID_Entry *entry = playlists->fav_tracks.first; entry; entry = entry->next)
										{
											mplayer_queue_last(entry->track_id);
										}
										mplayer_queue_resume();
									}
									
									ui_spacer_pixels(10, 1);
								}
							}
						}
						
						ui_next_width(ui_size_parent_remaining());
						ui_next_height(ui_size_parent_remaining());
						ui_horizontal_layout() ui_padding(ui_size_pixel(50, 0))
						{
							u32 item_index = ui_list_begin(str8_lit("favorites-tracks-list"), playlists->fav_tracks.count, 50.0f, 1.0f);
							defer(ui_list_end())
							{
								Mplayer_Track_ID_Entry *first_entry = 0;
								{
									u32 count; for(count = 0, first_entry = playlists->fav_tracks.first; 
										count < item_index && first_entry; 
										count += 1, first_entry = first_entry->next);
								}
								for(Mplayer_Track_ID_Entry *entry = first_entry; ui_list_item_begin(); (entry = entry->next, ui_list_item_end()))
								{
									Mplayer_Track_ID track_id = entry->track_id;
									Mplayer_UI_Interaction interaction = mplayer_ui_track_item(track_id);
									if (interaction.clicked_left)
									{
										mplayer_queue_play_track(track_id);
										mplayer_queue_resume();
									}
									if (interaction.clicked_right)
									{
										ui_open_ctx_menu(g_ui->mouse_pos, mplayer_ctx->ctx_menu_ids[Track_Context_Menu]);
										mplayer_ctx->ctx_menu_data.item_id = to_item_id(track_id);
									}
								}
							}
							
							ui_spacer_pixels(50, 0);
						}
					} break;
				}
			}
		}
		
		// NOTE(fakhri): track control
		ui_next_background_color(vec4(0.f, 0.f, 0.f, 1));
		ui_pref_height(ui_size_pixel(125, 1))
			ui_vertical_layout()
		{
			ui_spacer_pixels(10, 1);
			
			//- NOTE(fakhri): track 
			//ui_next_background_color(vec4(1, 0, 1, 1));
			ui_pref_height(ui_size_pixel(20, 1))
				ui_horizontal_layout()
			{
				ui_spacer_pixels(10, 1);
				
				Mplayer_Track *current_track = mplayer_queue_get_current_track();
				// NOTE(fakhri): timestamp
				{
					ui_next_width(ui_size_text_dim(1));
					
					String8 timestamp_string = str8_lit("--:--:--");
					if (current_track && current_track->flac_stream)
					{
						u64 sample_number = current_track->flac_stream->next_sample_number - current_track->start_sample_offset;
						Mplayer_Timestamp timestamp = flac_get_current_timestap(sample_number, u64(current_track->flac_stream->streaminfo.sample_rate));
						timestamp_string = str8_f(mplayer_ctx->frame_arena, "%.2d:%.2d:%.2d", timestamp.hours, timestamp.minutes, timestamp.seconds);
					}
					
					ui_label(timestamp_string);
				}
				
				ui_spacer_pixels(20, 1);
				
				// NOTE(fakhri): track progres slider
				ui_next_width(ui_size_percent(1.0f, 0.25f));
				ui_next_background_color(vec4(0.2f, 0.2f, 0.2f, 1.0f));
				ui_next_roundness(5);
				if (current_track && current_track->flac_stream)
				{
					u64 current_playing_sample = 0;
					u64 samples_count = current_track->end_sample_offset - current_track->start_sample_offset;
					current_playing_sample = current_track->flac_stream->next_sample_number - current_track->start_sample_offset;
					
					Mplayer_UI_Interaction progress = ui_slider_u64(str8_lit("track_progress_slider"), &current_playing_sample, 0, samples_count);
					if (progress.pressed_left)
					{
						mplayer_queue_pause();
						flac_seek_stream(current_track->flac_stream, current_track->start_sample_offset + (u64)current_playing_sample);
					}
					else if (progress.released_left)
					{
						mplayer_queue_resume();
					}
				}
				else
				{
					u64 current_playing_sample = 0;
					ui_slider_u64(str8_lit("track_progress_slider"), &current_playing_sample, 0, 0);
				}
				
				ui_spacer_pixels(10, 1);
				
				{
					ui_next_width(ui_size_text_dim(1));
					
					String8 timestamp_string = str8_lit("--:--:--");
					if (current_track && current_track->flac_stream)
					{
						u64 samples_count = current_track->end_sample_offset - current_track->start_sample_offset;
						Mplayer_Timestamp timestamp = flac_get_current_timestap(samples_count, u64(current_track->flac_stream->streaminfo.sample_rate));
						timestamp_string = str8_f(mplayer_ctx->frame_arena, "%.2d:%.2d:%.2d", timestamp.hours, timestamp.minutes, timestamp.seconds);
					}
					ui_label(timestamp_string);
				}
				
				ui_spacer_pixels(10, 1);
			}
			
			ui_pref_height(ui_size_parent_remaining())
				ui_horizontal_layout()
			{
				ui_next_width(ui_size_percent(1.0f/3.0f, 1));
				ui_horizontal_layout()
				{
					ui_spacer_pixels(10, 1);
					
					Mplayer_Track *current_track = mplayer_queue_get_current_track();
					// NOTE(fakhri): cover image
					if (current_track)
					{
						Mplayer_Item_Image *cover_image = mplayer_get_image_by_id(current_track->image_id, 1);
						
						// NOTE(fakhri): cover
						{
							u32 flags = 0;
							if (mplayer_item_image_ready(*cover_image))
							{
								flags |= UI_FLAG_Draw_Image;
								ui_next_texture_tint_color(vec4(1, 1, 1, 1));
								ui_next_texture(cover_image->texture);
							}
							
							ui_next_width(ui_size_pixel(90, 1));
							ui_next_height(ui_size_pixel(90, 1));
							ui_element({}, flags);
						}
						
						ui_spacer_pixels(10, 1);
						
						// NOTE(fakhri): track info
						ui_next_width(ui_size_parent_remaining());
						ui_pref_flags(UI_FLAG_Clip)
							ui_vertical_layout()
						{
							ui_spacer_pixels(25, 0);
							
							ui_next_height(ui_size_text_dim(1));
							ui_next_width(ui_size_text_dim(1));
							ui_next_font_size(20);
							if (mplayer_ui_underlined_button_f("%.*s##_track_title_button", STR8_EXPAND(current_track->title)).clicked_left)
							{
								mplayer_change_mode(MODE_Album_Tracks, to_item_id(current_track->album_id));
							}
							
							ui_spacer_pixels(5, 0);
							
							ui_next_height(ui_size_text_dim(1));
							ui_next_width(ui_size_text_dim(1));
							ui_next_font_size(15);
							if (mplayer_ui_underlined_button_f("%.*s##_track_artist_button", STR8_EXPAND(current_track->artist)).clicked_left)
							{
								mplayer_change_mode(MODE_Artist_Albums, to_item_id(current_track->artist_id));
							}
						}
					}
				}
				
				// NOTE(fakhri): next/prevplay/pause buttons
				ui_next_width(ui_size_percent(1.0f/3.0f, 1));
				ui_vertical_layout()
				{
					ui_spacer_pixels(25, 1);
					
					ui_next_height(ui_size_parent_remaining());
					ui_next_width(ui_size_parent_remaining());
					ui_horizontal_layout()
					{
						ui_spacer(ui_size_parent_remaining());
						
						ui_pref_width(ui_size_text_dim(1))
							ui_pref_height(ui_size_text_dim(1))
							ui_pref_background_color(vec4(0.15f, 0.15f, 0.15f, 1))
						{
							ui_spacer(ui_size_parent_remaining());
							
							ui_next_roundness(20);
							ui_next_width(ui_size_pixel(40, 1));
							ui_next_height(ui_size_pixel(40, 1));
							if (ui_button(str8_lit("<<")).clicked_left && is_valid(mplayer_ctx->queue.current_index))
							{
								if (g_input->time - mplayer_ctx->time_since_last_play <= 1.f)
									mplayer_play_prev_in_queue();
								else
								{
									mplayer_queue_reset_current();
								}
								mplayer_queue_resume();
							}
							
							ui_spacer_pixels(10, 1);
							
							ui_next_roundness(25);
							ui_next_width(ui_size_pixel(50, 1));
							ui_next_height(ui_size_pixel(50, 1));
							if (ui_button(mplayer_is_queue_playing()? str8_lit("II") : str8_lit("I>")).clicked_left)
							{
								mplayer_queue_toggle_play();
							}
							ui_spacer_pixels(10, 1);
							
							ui_next_roundness(20);
							ui_next_width(ui_size_pixel(40, 1));
							ui_next_height(ui_size_pixel(40, 1));
							if (ui_button(str8_lit(">>")).clicked_left)
							{
								mplayer_play_next_in_queue();
							}
							
							ui_spacer(ui_size_parent_remaining());
						}
						
						ui_spacer(ui_size_parent_remaining());
					}
					
					ui_spacer(ui_size_parent_remaining());
				}
				
				// NOTE(fakhri): volume
				ui_next_width(ui_size_parent_remaining());
				ui_horizontal_layout()
				{
					ui_spacer(ui_size_parent_remaining());
					
					ui_next_width(ui_size_pixel(150, 1));
					ui_vertical_layout()
					{
						ui_spacer(ui_size_parent_remaining());
						
						// NOTE(fakhri): volume slider
						{
							ui_next_height(ui_size_pixel(12, 1));
							ui_next_background_color(vec4(0.2f, 0.2f, 0.2f, 1.0f));
							ui_next_roundness(5);
							ui_slider_f32(str8_lit("volume-control-slider"), &mplayer_ctx->volume, 0, 1);
						}
						
						ui_spacer(ui_size_parent_remaining());
					}
					
					ui_spacer_pixels(30, 1);
				}
			}
		}
	}
	
	ui_end();
	
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

//~ NOTE(fakhri): Mplayer Exported API
exported
MPLAYER_EXPORT(mplayer_get_vtable)
{
	Mplayer_Exported_Vtable result = {
		.mplayer_initialize = mplayer_initialize,
		.mplayer_hotload = mplayer_hotload,
		.mplayer_update_and_render = mplayer_update_and_render,
		.mplayer_get_audio_samples = mplayer_get_audio_samples,
	};
	return result;
}
