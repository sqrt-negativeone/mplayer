
enum Texture_Flag
{
	TEXTURE_FLAG_GRAY_BIT,
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

enum Render_Entry_Kind
{
	Render_Entry_Kind_Render_Entry_Clear_Color,
	Render_Entry_Kind_Render_Entry_Textured_Rect,
};

struct Render_Entry_Header
{
	Render_Entry_Kind kind;
};

struct Render_Entry_Clear_Color
{
	V4_F32 color;
};

struct Render_Entry_Textured_Rect
{
	Texture texture;
	V3_F32 pos;
	V2_F32 dim;
	V4_F32 color;
	V2_F32 uv_scale;
	V2_F32 uv_offset;
	M4_Inv clip;
	f32 roundness;
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



internal Render_Entry_Header *
rndr_push_cmd_buffer(Render_Context *render_ctx, u64 size)
{
	Render_Entry_Header *result = 0;
	assert(render_ctx->command_offset + size <= render_ctx->commands.size);
	if (render_ctx->command_offset + size <= render_ctx->commands.size)
	{
		result = (Render_Entry_Header *)(render_ctx->commands.data + render_ctx->command_offset);
		render_ctx->command_offset += size;
	}
	return result;
}


#define rndr_push_cmd(render_ctx, Type) (Type*)__rndr_push_cmd(render_ctx, sizeof(Type), Render_Entry_Kind_##Type)
internal void *
__rndr_push_cmd(Render_Context *render_ctx, u64 size, Render_Entry_Kind kind)
{
	void *result = 0;
  
	Render_Entry_Header *header = rndr_push_cmd_buffer(render_ctx, sizeof(Render_Entry_Header) + size);
	assert(header);
	if (header)
	{
		header->kind = kind;
		result = (header + 1);
	}
  
	return result;
}

internal void
push_clear_color(Render_Context *render_ctx, V4_F32 color)
{
	Render_Entry_Clear_Color *cmd = rndr_push_cmd(render_ctx, Render_Entry_Clear_Color);
	if (cmd)
	{
		cmd->color = color;
	}
}

internal void
push_image(Render_Context *render_ctx, V3_F32 pos, V2_F32 dim, Texture texture, M4_Inv clip, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0f, V2_F32 uv_scale = vec2(1, 1), V2_F32 uv_offset = vec2(0, 0))
{
	Render_Entry_Textured_Rect *cmd = rndr_push_cmd(render_ctx, Render_Entry_Textured_Rect);
	assert(cmd);
	if (cmd)
	{
		cmd->texture   = texture;
		cmd->pos       = pos;
		cmd->dim       = dim;
		cmd->color     = color;
		cmd->uv_scale  = uv_scale;
		cmd->uv_offset = uv_offset;
		cmd->clip      = clip;
		cmd->roundness = roundness;
	}
}

internal void
push_rect(Render_Context *render_ctx, V3_F32 pos, V2_F32 dim, M4_Inv clip, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(render_ctx, pos, dim, render_ctx->white_texture, clip, color, roundness);
}

internal void
push_rect(Render_Context *render_ctx, V2_F32 pos, V2_F32 dim, M4_Inv clip, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(render_ctx, vec3(pos, 0), dim, render_ctx->white_texture, clip, color, roundness);
}



internal M4_Inv
compute_clip_matrix(V2_F32 pos, V2_F32 dim)
{
	M4_Inv result; 
	M4 proj = m4_ortho3d(-0.5f * dim.width,  0.5f * dim.width,
		-0.5f * dim.height, 0.5f * dim.height,
		-100, 100);
	
	M4 t_matrix = m4_translate(vec3(-pos.x, -pos.y, 0));
	M4 view = t_matrix;
  
	M4 inv_proj = m4_inv_ortho(proj);
	M4 inv_view = m4_inv_translate(t_matrix);
  
	result.mat = proj * view;
	result.inv = inv_view * inv_proj;
	
	return result;
}


internal void
push_texture_upload_request(Textures_Upload_Buffer *upload_buffer, Texture texture, Buffer tex_buf)
{
	assert(upload_buffer);
	assert(upload_buffer->count < upload_buffer->capacity);
	
	Texture_Upload_Entry *entry = upload_buffer->entries + upload_buffer->count;
	entry->texture = texture;
	entry->tex_buf = tex_buf;
	
	upload_buffer->count += 1;
}