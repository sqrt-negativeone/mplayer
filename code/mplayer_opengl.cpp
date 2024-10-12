
#define GL_VERSION_MAJOR 3
#define GL_VERSION_MINOR 3

struct OpenGL
{
	Render_Context render_ctx;
	Range2_I32 draw_region;
	V2_I32 window_dim;
	
	u32 *textures2d_array;
};


internal u32
gl_get_texture_handle(OpenGL *opengl, Texture texture)
{
	assert(texture.index < opengl->render_ctx.textures_count);
	u32 handle = opengl->textures2d_array[texture.index];
	return handle;
}


internal void
gl_upload_texture(OpenGL *opengl, Texture texture, Buffer buffer)
{
	u32 handle = gl_get_texture_handle(opengl, texture);
	if (handle != 0)
	{
		glBindTexture(GL_TEXTURE_2D, handle);
		
		u32 format = GL_RGBA;
		if (has_flag(texture.flags, Gray_Bit))
		{
			format = GL_RED;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
		}
		
		glTexImage2D(GL_TEXTURE_2D,
			0,
			i32(format), 
			i32(texture.width),
			i32(texture.height),
			0,
			u32(format),
			GL_UNSIGNED_BYTE,
			buffer.data);
	}
}


internal void
init_opengl_renderer(OpenGL *opengl, Memory_Arena *arena)
{
	opengl->render_ctx.textures_count = 0;
	opengl->render_ctx.textures_capacity = 128;
	opengl->textures2d_array = m_arena_push_array(arena, u32, opengl->render_ctx.textures_capacity);
	assert(opengl->textures2d_array);
	
	glGenTextures(u32(opengl->render_ctx.textures_capacity), opengl->textures2d_array);
	for (u32 i = 0; i < opengl->render_ctx.textures_capacity; i += 1)
	{
		u32 handle = opengl->textures2d_array[i];
		glBindTexture(GL_TEXTURE_2D, handle);
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	
init_renderer(&opengl->render_ctx, arena);
	
	u32 white_color = 0xFFFFFFFF;
	gl_upload_texture(opengl, opengl->render_ctx.white_texture, make_buffer((u8*)&white_color, sizeof(white_color)));
}


internal void
gl_begin_frame(OpenGL *opengl, V2_I32 window_dim, V2_I32 draw_dim, Range2_I32 draw_region)
{
	opengl->render_ctx.command_offset = 0;
	opengl->render_ctx.draw_dim       = vec2(draw_dim);
	opengl->window_dim  = window_dim;
	opengl->draw_region = draw_region;
}


internal void
gl_end_frame(OpenGL *opengl)
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_SCISSOR_TEST);
	
	glDepthFunc(GL_LEQUAL);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	
	glScissor(0, 0,
		opengl->window_dim.width,
		opengl->window_dim.height);
  
	glClearColor(0, 0, 0, 1);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
	V2_I32 draw_region_dim = range_dim(opengl->draw_region);
	glViewport(opengl->draw_region.min_x, 
		opengl->draw_region.min_y,
		draw_region_dim.width,
		draw_region_dim.height);
  
	glClearColor(1, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	
	
	// NOTE(fakhri): process upload requests
	if (opengl->render_ctx.upload_buffer.count)
	{
		for (u32 i = 0;
			i < opengl->render_ctx.upload_buffer.count;
			i += 1)
		{
			Texture_Upload_Entry *entry = opengl->render_ctx.upload_buffer.entries + i;
			gl_upload_texture(opengl, entry->texture, entry->tex_buf);
		}
		opengl->render_ctx.upload_buffer.count = 0;
	}
  
	Render_Context *render_ctx = (Render_Context *)opengl;
	for (u8 *cmd_ptr = render_ctx->commands.data, *cmds_ptr_end = render_ctx->commands.data + render_ctx->command_offset;
		cmd_ptr < cmds_ptr_end;)
	{
		Render_Command_Header *header = (Render_Command_Header *)cmd_ptr;
		cmd_ptr += sizeof(*header);
		
#define RENDER_CMD_FROM_CMD_PTR(cmd_var_name, cmd_ptr, Type) \
Type *cmd_var_name = (Type *)(cmd_ptr);                    \
cmd_ptr += sizeof(Type);
		
		
		switch(header->kind)
		{
			case Render_Command_Kind_Render_Command_Clear_Color:
			{
				RENDER_CMD_FROM_CMD_PTR(cmd, cmd_ptr, Render_Command_Clear_Color);
				
				glClearColor(cmd->color.r, cmd->color.g, cmd->color.b, cmd->color.a);
				glClear(GL_COLOR_BUFFER_BIT);
				
			} break;
			
			invalid_default_case;;
		}
		
#undef RENDER_CMD_FROM_CMD_PTR
	}
}