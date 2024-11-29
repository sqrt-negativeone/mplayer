/* date = November 18th 2024 4:04 pm */

#ifndef MPLAYER_RENDERER_H
#define MPLAYER_RENDERER_H

enum Texture_Flag
{
	TEXTURE_FLAG_GRAY_BIT,
	TEXTURE_FLAG_RGB_BIT,
};

enum Texture_State
{
	Texture_State_Invalid,
	Texture_State_Unloaded,
	Texture_State_Loading,
	Texture_State_Loaded,
};


#define NULL_TEXTURE Texture{}
union Texture
{
	u64 compact;
	struct
	{
		u8 state;
		u8 flags;
		u16 index;
		u16 width;
		u16 height;
	};
};

struct Rect_Vertex_Data
{
	V3_F32 pos;
	V2_F32 uv;
	V4_F32 color;
	
	// NOTE(fakhri): this is shared per instance
	V2_F32 rect_center;
	V2_F32 rect_dim;
	f32    roundness;
	i32 textured;
};

struct Texture_Upload_Entry
{
	Texture texture;
	Buffer tex_buf;
	b32 should_free;
};

struct Textures_Upload_Buffer
{
	Texture_Upload_Entry *entries;
	u64 capacity;
	
	volatile u64 head;
	volatile u64 tail;
	volatile u64 claimed_head;
};

struct Upload_Texture_To_GPU_Data
{
	Upload_Texture_To_GPU_Data *next;
	struct Render_Context *render_ctx;
	Texture *texture;
	Buffer texture_data;
};

struct Render_Context
{
	Memory_Arena *arena;
	Buffer commands;
	u64    command_offset;
	
	V2_F32 draw_dim;
	
	Textures_Upload_Buffer upload_buffer;
	u64 textures_count;
	u64 textures_capacity;
	
	Rect_Vertex_Data *vertex_array;
	u32 max_vertices_count;
	u32 vertices_count;
	
	u16 *index_array;
	u32 max_indices_count;
	u32 indices_count;
	
	u32 max_rects;
	u32 rects_count;
	
	Work_Proc *upload_texture_to_gpu_work;
	Upload_Texture_To_GPU_Data *free_texture_upload_data;
};

struct Render_Config
{
	V2_F32 camera_pos;
	V2_F32 camera_dim;
	Range2_F32 cull_range;
	M4_Inv proj;
};

struct Render_Entry_Textured_Rects
{
	Texture texture;
	Render_Config config;
	u32 rects_count;
	
	u32 rect_offset;
	u32 vertex_offset;
	u32 index_offset;
};

struct Render_Group
{
	Render_Context *render_ctx;
	Render_Config config;
	
	Render_Entry_Textured_Rects *textured_rects;
};

#endif //MPLAYER_RENDERER_H
