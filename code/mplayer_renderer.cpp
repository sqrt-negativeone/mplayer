
enum Texture_Flag
{
	Gray_Bit,
};

union Texture
{
	u64 compact;
	struct
	{
	u16 flags;
	u16 index;
	u16 width;
	u16 height;
	};
};

struct Texture_Upload_Entry
{
	Texture texture;
	Buffer tex_buf;
};

struct Textures_Upload_Buffer
{
	Texture_Upload_Entry *entries;
	u64 capacity;
	u64 count;
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
	
	Texture white_texture;
};

enum Render_Command_Kind
{
	Render_Command_Kind_Render_Command_Clear_Color,
};

struct Render_Command_Header
{
	Render_Command_Kind kind;
};

struct Render_Command_Clear_Color
{
	V4_F32 color;
};


internal Texture
	reserve_texture_handle(Render_Context *render_ctx, u16 width, u16 height)
{
	Texture texture;
	assert(render_ctx->textures_count < render_ctx->textures_capacity);
	
	texture.index  = u16(render_ctx->textures_count);
	texture.width  = width;
	texture.height = height;
	render_ctx->textures_count += 1;
	
	return texture;
}


internal void
init_renderer(Render_Context *render_ctx, Memory_Arena *arena)
{
	assert(render_ctx);
	assert(arena);
	render_ctx->arena = arena;
	
	render_ctx->commands = arena_push_buffer(arena, megabytes(16));
	render_ctx->command_offset = 0;
	
	render_ctx->upload_buffer.count    = 0;
	render_ctx->upload_buffer.capacity = 64;
	render_ctx->upload_buffer.entries  = m_arena_push_array(arena, Texture_Upload_Entry, 
		render_ctx->upload_buffer.capacity);
	
render_ctx->white_texture = reserve_texture_handle(render_ctx, 1, 1);
}


// returns a region where to draw, the region has the same aspect ratio as
// render_dim, and is centered inside the window_dim
 internal Range2_I32
compute_draw_region_aspect_ratio_fit(V2_I32 render_dim, V2_I32 window_dim)
{
	Range2_I32 result = ZERO_STRUCT;
  
	f32 render_aspect_ratio = (f32)render_dim.width / (f32)render_dim.height;
	f32 optimal_width  = (f32)window_dim.height * render_aspect_ratio;
	f32 optimal_height = (f32)window_dim.width * (1.0f / render_aspect_ratio);
  
	if (optimal_width > (f32)window_dim.width)
	{
		result.minp.x = 0;
		result.maxp.x = window_dim.width;
		
		i32 half_empty = round_f32_i32(0.5f * ((f32)window_dim.height - optimal_height));
		i32 use_height = round_f32_i32(optimal_height);
		
		result.minp.y = half_empty;
		result.maxp.y = half_empty + use_height;
	}
	else
	{
		result.minp.y = 0;
		result.maxp.y = window_dim.height;
		
		i32 half_empty = round_f32_i32(0.5f * ((f32)window_dim.width - optimal_width));
		i32 use_width  = round_f32_i32(optimal_width);
		
		result.minp.x = half_empty;
		result.maxp.x = half_empty + use_width;
	}
  
	return result;
}



 internal Render_Command_Header *
rndr_push_cmd_buffer(Render_Context *render_ctx, u64 size)
{
	Render_Command_Header *result = 0;
	assert(render_ctx->command_offset + size <= render_ctx->commands.size);
	if (render_ctx->command_offset + size <= render_ctx->commands.size)
	{
		result = (Render_Command_Header *)(render_ctx->commands.data + render_ctx->command_offset);
		render_ctx->command_offset += size;
	}
	return result;
}


#define rndr_push_cmd(render_ctx, Type) (Type*)__rndr_push_cmd(render_ctx, sizeof(Type), Render_Command_Kind_##Type)
 internal void *
__rndr_push_cmd(Render_Context *render_ctx, u64 size, Render_Command_Kind kind)
{
	void *result = 0;
  
	Render_Command_Header *header = rndr_push_cmd_buffer(render_ctx, sizeof(Render_Command_Header) + size);
	assert(header);
	if (header)
	{
		header->kind = kind;
		result = ((u8 *)header + sizeof(Render_Command_Header));
	}
  
	return result;
}



 internal void
push_clear_color(Render_Context *render_ctx, V4_F32 color)
{
	Render_Command_Clear_Color *cmd = rndr_push_cmd(render_ctx, Render_Command_Clear_Color);
	if (cmd)
	{
		cmd->color = color;
	}
}
