/* date = November 18th 2024 4:00 pm */

#ifndef MPLAYER_H
#define MPLAYER_H

#include "mplayer_base.h"
#include "mplayer_math.h"
#include "mplayer_random.h"

#include "mplayer_memory.h"
#include "mplayer_string.cpp"
#include "mplayer_buffer.h"
#include "mplayer_thread_context.cpp"

enum Cursor_Shape
{
	Cursor_Arrow,
	Cursor_Hand,
	Cursor_Horizontal_Resize,
	Cursor_Vertical_Resize,
	Cursor_Wait,
	Cursor_TextSelect,
	Cursor_Unavailable,
	
	Cursor_COUNT,
};

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
	FileFlag_Directory = (1 << 0),
	FileFlag_Valid     = (1 << 1),
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
	File_Open_Read     = (1 << 0),
	File_Open_Write    = (1 << 1),
	File_Create_Always = (1 << 2),
};

typedef void File_Handle;

struct Directory
{
	File_Info *files;
	u32 count;
};

typedef void Set_Cursor_Proc(Cursor_Shape cursor);

typedef i64 Atomic_Increment64_Proc(volatile i64 *addend);
typedef i64 Atomic_Decrement64_Proc(volatile i64 *addend);
typedef b32 Atomic_Compare_And_Exchange64_Proc(i64 volatile *dst, i64 expect, i64 exchange);
typedef b32 Atomic_Compare_And_Exchange_Pointer_Proc(Address volatile *dst, Address expect, Address exchange);

typedef void     *Allocate_Memory_Proc(u64 size);
typedef void      Deallocate_Memory_Proc(void *memory);
typedef Directory Read_Directory_Proc(Memory_Arena *arena, String8 path);
typedef String8   Get_Current_Directory_Proc(Memory_Arena *arena);
typedef File_Handle *Open_File_Proc(String8 path, u32 flags);
typedef void   Close_File_Proc(File_Handle *file);
typedef Buffer File_Read_block_Proc(File_Handle *file, void *data, u64 size);
typedef b32 File_Write_block_Proc(File_Handle *file, void *buf, u64 size);

typedef Buffer Load_Entire_File(String8 file_path, Memory_Arena *arena);

struct Mplayer_OS_Vtable
{
	Set_Cursor_Proc *set_cursor;
	
	Read_Directory_Proc  *read_directory;
	Get_Current_Directory_Proc *get_current_directory;
	Load_Entire_File     *load_entire_file;
	Open_File_Proc       *open_file;
	Close_File_Proc      *close_file;
	File_Read_block_Proc *read_block;
	File_Write_block_Proc *write_block;
	
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

#include "mplayer_input.cpp"

#include "mplayer_renderer.h"
#include "mplayer_bitstream.h"
#include "mplayer_flac.h"

#include "mplayer_font.h"

struct Mplayer_UI;

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

#define NULL_TRACK_ID Mplayer_Track_ID{}
#define NULL_ALBUM_ID Mplayer_Album_ID{}
#define NULL_ARTIST_ID Mplayer_Artist_ID{}

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

#define MAX_ARTISTS_COUNT 512
#define MAX_ALBUMS_COUNT 1024
#define MAX_TRACKS_COUNT 8192

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
	
	MODE_Lyrics,
	MODE_Settings,
};

union Mplayer_Item_ID
{
	Mplayer_Track_ID  track_id;
	Mplayer_Album_ID  album_id;
	Mplayer_Artist_ID artist_id;
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


struct Mplayer_Context
{
	Memory_Arena main_arena;
	Memory_Arena frame_arena;
	Render_Context *render_ctx;
	Mplayer_UI *ui;
	Mplayer_Input input;
	Fonts_Context *fonts_ctx;
	Random_Generator entropy;
	
	Font *font;
	
	f32 time_since_last_play;
	f32 volume;
	
	Mplayer_OS_Vtable os;
	
	Mplayer_Mode_Stack *mode_stack;
	Mplayer_Mode_Stack *mode_stack_free_list_first;
	Mplayer_Mode_Stack *mode_stack_free_list_last;
	// Mplayer_Mode mode;
	
	Mplayer_Library library;
	
	Mplayer_Artist_ID selected_artist_id;
	Mplayer_Album_ID selected_album_id;
	
	f32 track_name_hover_t;
	
	b32 show_path_modal;
	Mplayer_Path_Lister path_lister;
	
	Mplayer_Queue queue;
	
	b32 show_library_locations;
	MPlayer_Settings settings;
};

typedef void Mplayer_Initialize_Proc(Mplayer_Context *mplayer);
typedef void Mplayer_Hotload_Proc(Mplayer_Context *mplayer);
typedef void Mplayer_Update_And_Render_Proc(Mplayer_Context *mplayer);
typedef void Mplayer_Get_Audio_Samples_Proc(Sound_Config device_config, Mplayer_Context *mplayer, void *output_buf, u32 frame_count);

internal void mplayer_initialize_stub(Mplayer_Context *mplayer) {}
internal void mplayer_hotload_stub(Mplayer_Context *mplayer) {}
internal void mplayer_update_and_render_stub(Mplayer_Context *mplayer) {}
internal void mplayer_get_audio_samples_stub(Sound_Config device_config, Mplayer_Context *mplayer, void *output_buf, u32 frame_count) {}

#endif //MPLAYER_H
