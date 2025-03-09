internal b32
is_texture_valid(Texture texure)
{
	b32 result = texure.state != Texture_State_Invalid;
	return result;
}



internal b32
is_nil(Render_Context *render_ctx, Texture_Available_Index *index)
{
	b32 result = (index == &render_ctx->nil_available_index);
	return result;
}


internal M4_Inv
compute_clip_matrix(V2_F32 pos, V2_F32 dim)
{
	M4_Inv result; 
	M4 proj = m4_ortho3d_rh_no(-0.5f * dim.width,  0.5f * dim.width,
		-0.5f * dim.height, 0.5f * dim.height,
		-100, 100);
	
	M4 t_matrix = m4_translate(v3(-pos.x, -pos.y, 0));
	M4 view = t_matrix;
  
	M4 inv_proj = m4_inv_ortho(proj);
	M4 inv_view = m4_inv_translate(t_matrix);
  
	result.mat = proj * view;
	result.inv = inv_view * inv_proj;
	
	return result;
}

internal Texture
reserve_texture_handle(Render_Context *render_ctx, u16 width, u16 height, u8 flags)
{
	assert(render_ctx->textures_count);
	
	Texture texture = ZERO_STRUCT;
	
	u16 index = 0;
	if (!is_nil(render_ctx, render_ctx->first_available_index))
	{
		index = render_ctx->first_available_index->index;
		
		// NOTE(fakhri): push to the free list
		render_ctx->first_available_index->next = render_ctx->free_available_indices;
		render_ctx->free_available_indices = render_ctx->first_available_index;
		
		// NOTE(fakhri): update available indices list
		render_ctx->first_available_index = render_ctx->first_available_index->next;
	}
	
	if (!index)
	{
		assert(render_ctx->textures_count < render_ctx->textures_capacity);
		index = (u16)render_ctx->textures_count;
		render_ctx->textures_count += 1;
	}
	
	texture.flags  = flags;
	texture.state  = Texture_State_Unloaded;
	texture.index  = index;
	texture.width  = width;
	texture.height = height;
	
	return texture;
}

internal Texture
reserve_texture_handle(Render_Context *render_ctx)
{
	Texture texture = reserve_texture_handle(render_ctx, 0, 0, 0);
	return texture;
}


internal void
push_texture_upload_request(Textures_Upload_Buffer *upload_buffer, Texture *texture, Buffer tex_buf, b32 should_free)
{
	assert(upload_buffer);
	
	u64 claimed_head = platform->atomic_increment64((volatile i64 *)&upload_buffer->claimed_head);
	assert(claimed_head - upload_buffer->tail < TEXTURE_UPLOAD_BUFFER_CAPACITY);
	
	u64 entry_index = claimed_head % TEXTURE_UPLOAD_BUFFER_CAPACITY;
	Texture_Upload_Entry *entry = upload_buffer->entries + entry_index;
	entry->texture = texture;
	entry->tex_buf = tex_buf;
	entry->should_free = should_free;
	
	while (!platform->atomic_compare_and_exchange64((volatile i64 *)&upload_buffer->head, claimed_head, claimed_head + 1));
}

internal void
init_renderer(Render_Context *render_ctx, Memory_Arena *arena)
{
	assert(render_ctx);
	assert(arena);
	render_ctx->arena = arena;
	
	
	render_ctx->nil_available_index.next = &render_ctx->nil_available_index;
	render_ctx->first_available_index = &render_ctx->nil_available_index;
	render_ctx->free_available_indices = &render_ctx->nil_available_index;
	
	render_ctx->commands = arena_push_buffer(arena, megabytes(16));
	render_ctx->command_offset = 0;
	
	render_ctx->upload_buffer.head    = 0;
	render_ctx->upload_buffer.claimed_head = 0;
	render_ctx->upload_buffer.tail    = 0;
	render_ctx->upload_buffer.entries  = m_arena_push_array(arena, Texture_Upload_Entry, 
		TEXTURE_UPLOAD_BUFFER_CAPACITY);
	render_ctx->release_buffer.count    = 0;
	render_ctx->release_buffer.textures = m_arena_push_array(arena, Texture,
		TEXTURE_RELEASE_BUFFER_CAPACITY);
	
	render_ctx->white_color = 0xFFFFFFFF;
	render_ctx->white_texture = reserve_texture_handle(render_ctx, 1, 1, 0);
	push_texture_upload_request(&render_ctx->upload_buffer, &render_ctx->white_texture, make_buffer((u8*)&render_ctx->white_color, 4), 0);
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
render_group_set_cull_range(Render_Group *group, Range2_F32 cull_range)
{
	render_group_flush(group);
	group->config.cull_range = cull_range;
}

internal void
render_group_add_cull_range(Render_Group *group, Range2_F32 cull_range)
{
	render_group_flush(group);
	group->config.cull_range = range2f32_intersection(group->config.cull_range, cull_range);
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
push_image(Render_Group *group, V3_F32 pos, V2_F32 dim, Texture texture, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0f, V2_F32 uv_scale = v2(1, 1), V2_F32 uv_offset = v2(0, 0))
{
	Render_Context *render_ctx = group->render_ctx;
	
	if (!range2f32_check_intersect(group->config.cull_range, range2f32_center_dim(pos.xy, dim)))
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
	vertex[0].pos   = v3(pos.x - 0.5f * dim.width, pos.y + 0.5f * dim.height, pos.z);
	vertex[0].uv    = uv_offset;
	vertex[0].color = color;
	vertex[0].rect_center = pos.xy;
	vertex[0].rect_dim = dim;
	vertex[0].roundness = roundness;
	vertex[0].textured = textured;
	
	// NOTE(fakhri): bottom-left
	vertex[1].pos   = v3(pos.x - 0.5f * dim.width, pos.y - 0.5f * dim.height, pos.z);
	vertex[1].uv    = v2(uv_offset.x, uv_offset.y + uv_scale.height);
	vertex[1].color = color;
	vertex[1].rect_center = pos.xy;
	vertex[1].rect_dim = dim;
	vertex[1].roundness = roundness;
	vertex[1].textured = textured;
	
	// NOTE(fakhri): bottom-right
	vertex[2].pos   = v3(pos.x + 0.5f * dim.width, pos.y - 0.5f * dim.height, pos.z);
	vertex[2].uv    = v2(uv_offset.x + uv_scale.width, uv_offset.y + uv_scale.height);
	vertex[2].color = color;
	vertex[2].rect_center = pos.xy;
	vertex[2].rect_dim = dim;
	vertex[2].roundness = roundness;
	vertex[2].textured = textured;
	
	// NOTE(fakhri): top-right
	vertex[3].pos   = v3(pos.x + 0.5f * dim.width, pos.y + 0.5f * dim.height, pos.z);
	vertex[3].uv    = v2(uv_offset.x + uv_scale.width, uv_offset.y);
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
push_image(Render_Group *group, V2_F32 pos, V2_F32 dim, Texture texture, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0f, V2_F32 uv_scale = v2(1, 1), V2_F32 uv_offset = v2(0, 0))
{
	push_image(group, v3v(pos, 0), dim, texture, color, roundness, uv_scale, uv_offset);
}

internal void
push_image(Render_Group *group, V2_F32 pos, f32 z, V2_F32 dim, Texture texture, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0f, V2_F32 uv_scale = v2(1, 1), V2_F32 uv_offset = v2(0, 0))
{
	push_image(group, v3v(pos, z), dim, texture, color, roundness, uv_scale, uv_offset);
}

internal void
push_rect(Render_Group *group, V3_F32 pos, V2_F32 dim, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, pos, dim, group->render_ctx->white_texture, color, roundness);
}

internal void
push_rect(Render_Group *group, Range2_F32 rect, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, range2f32_center(rect), range2f32_dim(rect), group->render_ctx->white_texture, color, roundness);
}

internal void
push_rect(Render_Group *group, Range2_F32 rect, f32 z, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, range2f32_center(rect), z, range2f32_dim(rect), group->render_ctx->white_texture, color, roundness);
}

internal void
push_rect(Render_Group *group, V2_F32 pos, V2_F32 dim, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, v3v(pos, 0), dim, group->render_ctx->white_texture, color, roundness);
}

internal void
push_rect(Render_Group *group, V2_F32 pos, f32 z, V2_F32 dim, V4_F32 color = v4(1, 1, 1, 1), f32 roundness = 0.0)
{
	push_image(group, v3v(pos, z), dim, group->render_ctx->white_texture, color, roundness);
}


internal void
push_texture_release_request(Textures_Release_Buffer *release_buffer, Texture texture)
{
	if (texture.index)
	{
		assert(release_buffer);
		assert(release_buffer->count < TEXTURE_RELEASE_BUFFER_CAPACITY);
		release_buffer->textures[release_buffer->count++] = texture;
	}
}
