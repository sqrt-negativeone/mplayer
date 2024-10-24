
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

enum Texture_Flag
{
	TEXTURE_FLAG_GRAY_BIT,
	TEXTURE_FLAG_RGB_BIT,
};

#define NULL_TEXTURE Texture{}
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
	
	Rect_Vertex_Data *vertex_array;
	u32 max_vertices_count;
	u32 vertices_count;
	
	u16 *index_array;
	u32 max_indices_count;
	u32 indices_count;
	
	u32 max_rects;
	u32 rects_count;
};

enum Render_Entry_Kind
{
	Render_Entry_Kind_Render_Entry_Clear_Color,
	Render_Entry_Kind_Render_Entry_Textured_Rects,
};

struct Render_Entry_Header
{
	Render_Entry_Kind kind;
};

struct Render_Entry_Clear_Color
{
	V4_F32 color;
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

internal b32
is_texture_valid(Texture texure)
{
	b32 result = texure.width && texure.height;
	return result;
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

internal Texture
reserve_texture_handle(Render_Context *render_ctx, u16 width, u16 height)
{
	Texture texture;
	assert(render_ctx->textures_count < render_ctx->textures_capacity);
	
	texture.flags  = 0;
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



#define rndr_push_cmd(render_ctx, Type) (Type*)__rndr_push_cmd(render_ctx, sizeof(Type), Render_Entry_Kind_##Type)
internal void *
__rndr_push_cmd(Render_Context *render_ctx, u64 size, Render_Entry_Kind kind)
{
	void *result = 0;
	
	size += sizeof(Render_Entry_Header);
	Render_Entry_Header *header = 0;
	assert(render_ctx->command_offset + size <= render_ctx->commands.size);
	if (render_ctx->command_offset + size <= render_ctx->commands.size)
	{
		header = (Render_Entry_Header *)(render_ctx->commands.data + render_ctx->command_offset);
		render_ctx->command_offset += size;
	}
  
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
render_group_flush(Render_Group *group)
{
	group->textured_rects = 0;
}

internal void
render_group_update_config(Render_Group *group, V2_F32 camera_pos, V2_F32 camera_dim, Range2_F32 cull_range)
{
	render_group_flush(group);
	
	group->config.camera_pos = camera_pos;
	group->config.camera_dim = camera_dim;
	group->config.cull_range = cull_range;
	group->config.proj = compute_clip_matrix(camera_pos, camera_dim);
}

internal Render_Group
begin_render_group(Render_Context *render_ctx, V2_F32 camera_pos, V2_F32 camera_dim, Range2_F32 cull_range)
{
	Render_Group group = ZERO_STRUCT;
	group.render_ctx = render_ctx;
	render_group_update_config(&group, camera_pos, camera_dim, cull_range);
	return group;
}

internal void
push_image(Render_Group *group, V3_F32 pos, V2_F32 dim, Texture texture, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0f, V2_F32 uv_scale = vec2(1, 1), V2_F32 uv_offset = vec2(0, 0))
{
	Render_Context *render_ctx = group->render_ctx;
	
	if (!is_range_intersect(group->config.cull_range, range_center_dim(pos.xy, dim)))
	{
		return;
	}
	
	if (group->textured_rects)
	{
		if (!is_texture_valid(group->textured_rects->texture))
		{
			group->textured_rects->texture = texture;
		}
		else if (is_texture_valid(texture) && (group->textured_rects->texture.compact != texture.compact))
		{
			render_group_flush(group);
		}
	}
	
	if (!group->textured_rects)
	{
		group->textured_rects = rndr_push_cmd(render_ctx, Render_Entry_Textured_Rects);
		group->textured_rects->config = group->config;
		group->textured_rects->texture = texture;
		group->textured_rects->rect_offset   = render_ctx->rects_count;
		group->textured_rects->vertex_offset = render_ctx->vertices_count;
		group->textured_rects->index_offset  = render_ctx->indices_count;
		group->textured_rects->rects_count = 0;
	}
	
	Render_Entry_Textured_Rects *entry = group->textured_rects;
	
	render_ctx->rects_count    += 1;
	render_ctx->vertices_count += 4;
	render_ctx->indices_count  += 6;
	
	u32 rect_index   = entry->rect_offset   +     entry->rects_count;
	u32 vertex_index = entry->vertex_offset + 4 * entry->rects_count;
	u32 index_index  = entry->index_offset  + 6 * entry->rects_count;
	
	assert(rect_index   < render_ctx->max_rects);
	assert(vertex_index < render_ctx->max_vertices_count);
	assert(index_index  < render_ctx->max_indices_count);
	entry->rects_count += 1;
	
	u32 textured = is_texture_valid(texture);
	
	Rect_Vertex_Data *vertex = render_ctx->vertex_array + vertex_index;
	
	// NOTE(fakhri): top-left
	vertex[0].pos   = vec3(pos.x - 0.5f * dim.width, pos.y + 0.5f * dim.height, pos.z);
	vertex[0].uv    = uv_offset;
	vertex[0].color = color;
	vertex[0].rect_center = pos.xy;
	vertex[0].rect_dim = dim;
	vertex[0].roundness = roundness;
	vertex[0].textured = textured;
	
	// NOTE(fakhri): bottom-left
	vertex[1].pos   = vec3(pos.x - 0.5f * dim.width, pos.y - 0.5f * dim.height, pos.z);
	vertex[1].uv    = vec2(uv_offset.x, uv_offset.y + uv_scale.height);
	vertex[1].color = color;
	vertex[1].rect_center = pos.xy;
	vertex[1].rect_dim = dim;
	vertex[1].roundness = roundness;
	vertex[1].textured = textured;
	
	// NOTE(fakhri): bottom-right
	vertex[2].pos   = vec3(pos.x + 0.5f * dim.width, pos.y - 0.5f * dim.height, pos.z);
	vertex[2].uv    = vec2(uv_offset.x + uv_scale.width, uv_offset.y + uv_scale.height);
	vertex[2].color = color;
	vertex[2].rect_center = pos.xy;
	vertex[2].rect_dim = dim;
	vertex[2].roundness = roundness;
	vertex[2].textured = textured;
	
	// NOTE(fakhri): top-right
	vertex[3].pos   = vec3(pos.x + 0.5f * dim.width, pos.y + 0.5f * dim.height, pos.z);
	vertex[3].uv    = vec2(uv_offset.x + uv_scale.width, uv_offset.y);
	vertex[3].color = color;
	vertex[3].rect_center = pos.xy;
	vertex[3].rect_dim = dim;
	vertex[3].roundness = roundness;
	vertex[3].textured = textured;
	
	u16 *indices = render_ctx->index_array + index_index;
	
	// NOTE(fakhri): first triangle
	indices[0] = u16(vertex_index + 0);
	indices[1] = u16(vertex_index + 1);
	indices[2] = u16(vertex_index + 2);
	
	// NOTE(fakhri): second triangle
	indices[3] = u16(vertex_index + 0);
	indices[4] = u16(vertex_index + 2);
	indices[5] = u16(vertex_index + 3);
}


internal void
push_image(Render_Group *group, V2_F32 pos, V2_F32 dim, Texture texture, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0f, V2_F32 uv_scale = vec2(1, 1), V2_F32 uv_offset = vec2(0, 0))
{
	push_image(group, vec3(pos, 0), dim, texture, color, roundness, uv_scale, uv_offset);
}


internal void
push_rect(Render_Group *group, V3_F32 pos, V2_F32 dim, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, pos, dim, NULL_TEXTURE, color, roundness);
}

internal void
push_rect(Render_Group *group, V2_F32 pos, V2_F32 dim, V4_F32 color = vec4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, vec3(pos, 0), dim, NULL_TEXTURE, color, roundness);
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
