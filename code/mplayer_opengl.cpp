
#define GL_VERSION_MAJOR 3
#define GL_VERSION_MINOR 3

struct Rect_Shader_Uniforms
{
	i32 clip, rect_dim, rect_cent, roundness;
};

struct GL_Textured_Rect_Shader
{
	GLuint prog_id;
	GLuint vao;
	GLuint vbo;
	GLuint ebo;
	
	Rect_Shader_Uniforms uniforms;
};


#define SET_VERTEX_ATTRIBUTE(T, m, index) \
glEnableVertexAttribArray(index); \
glVertexAttribPointer(index, sizeof(member(T, m)) / sizeof(f32), GL_FLOAT, GL_FALSE, sizeof(T), (void*)member_offset(T, m));

#define SET_VERTEX_ATTRIBUTE_INSTANCED(T, m, index) \
SET_VERTEX_ATTRIBUTE(T, m, index); \
glVertexAttribDivisor(index, 1);

struct OpenGL
{
	Render_Context render_ctx;
	Range2_I32 draw_region;
	V2_I32 window_dim;
	
	u32 *textures2d_array;
	Load_Entire_File *load_entire_file;
	
	GL_Textured_Rect_Shader rect_shader;
};


internal u32
gl_compile_shader_program(OpenGL *opengl, const char *source_code)
{
	u32 id = 0;
	b32 ok = false;
  
	u32 vertex_shader_id   = glCreateShader(GL_VERTEX_SHADER);
	u32 fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
  
	u32 program = glCreateProgram();
  
	// NOTE(fakhri): compile vertex shader
	{
		const char *vertex_code[] = {"#version 330 core\n", "#define VERTEX_SHADER\n", source_code};
		glShaderSource(vertex_shader_id, array_count(vertex_code), vertex_code, 0);
		glCompileShader(vertex_shader_id);
		
		i32 status = 0;
		glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &status);
		if (status == 0)
		{
			GLchar info_log[512];
			glGetShaderInfoLog(vertex_shader_id, array_count(info_log), 0, info_log);
			LogError("Couldn't compile vertex shader program\n%s", source_code);
			LogError("shader error :\n%s", info_log);
			goto exit;
		}
	}
  
	// NOTE(fakhri): compile frag shader
	{
		const char *frag_code[] = {"#version 330 core\n", "#define FRAGMENT_SHADER\n", source_code};
		glShaderSource(fragment_shader_id, array_count(frag_code), frag_code, 0);
		glCompileShader(fragment_shader_id);
		
		i32 status = 0;
		glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &status);
		if (status == 0)
		{
			GLchar info_log[512];
			glGetShaderInfoLog(fragment_shader_id, array_count(info_log), 0, info_log);
			LogError("Couldn't compile fragment shader program\n%*s", source_code);
			LogError("shader error :\n%s", info_log);
			goto exit;
		}
	}
  
	// NOTE(fakhri): link shader program
	{
		glAttachShader(program, vertex_shader_id);
		glAttachShader(program, fragment_shader_id);
		
		glLinkProgram(program);
		
		i32 status = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == 1)
		{
			ok = true;
			id = program;
		}
		else
		{
			GLchar info_log[512];
			glGetProgramInfoLog(program, array_count(info_log), 0, info_log);
			LogError("Couldn't link shader program\n%*s", source_code);
			LogError("shader error :\n%s", info_log);
		}
	}
  
  
  
	exit:
	if (!ok)
	{
		glDetachShader(program, vertex_shader_id);
		glDetachShader(program, fragment_shader_id);
		glDeleteProgram(program);
		program = 0;
	}
  
  
	glDeleteShader(vertex_shader_id);
	glDeleteShader(fragment_shader_id);
  
	return id;
}

#define STATIC_VERTEX_BUFFERS 0

internal b32
gl_compile_textured_rect_shader(OpenGL *opengl, GL_Textured_Rect_Shader *rect_shader)
{
	Memory_Checkpoint scratch = begin_scratch(0, 0);
	// TODO(fakhri): bake the shader into the executable
	const char *rect_shader_source = (char *)opengl->load_entire_file(str8_lit("data/shaders/rectangle_shader_instanced.glsl"), scratch.arena).data;
	
	b32 result = true;
  
	rect_shader->prog_id = gl_compile_shader_program(opengl, rect_shader_source);
	if (!rect_shader->prog_id)
	{
		result = false;
		goto exit;
	}
  
	// TODO(fakhri): do we need to use the program here?
	glUseProgram(rect_shader->prog_id);
	
	glGenVertexArrays(1, &rect_shader->vao);
	glGenBuffers(1, &rect_shader->vbo);
	
	glBindVertexArray(rect_shader->vao);
  
	glBindBuffer(GL_ARRAY_BUFFER, rect_shader->vbo);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, pos,   0);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, uv,    1);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, color, 2);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, rect_center, 3);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, rect_dim,    4);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, roundness,   5);
	SET_VERTEX_ATTRIBUTE(Rect_Vertex_Data, textured,    6);
  
	glGenBuffers(1, &rect_shader->ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rect_shader->ebo);
	
	glBindVertexArray(0);
	
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	rect_shader->uniforms.clip      = glGetUniformLocation(rect_shader->prog_id, "clip");
	rect_shader->uniforms.rect_cent = glGetUniformLocation(rect_shader->prog_id, "rect_cent");
	rect_shader->uniforms.rect_dim  = glGetUniformLocation(rect_shader->prog_id, "rect_dim");
	rect_shader->uniforms.roundness = glGetUniformLocation(rect_shader->prog_id, "roundness");
	
	exit:;
	end_scratch(scratch);
	return result;
}

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
		if (has_flag(texture.flags, TEXTURE_FLAG_GRAY_BIT))
		{
			format = GL_RED;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
		}
		else if (has_flag(texture.flags, TEXTURE_FLAG_RGB_BIT))
		{
			format = GL_RGB;
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
		
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

WORK_SIG(upload_texture_to_gpu_work)
{
	assert(input);
	Upload_Texture_To_GPU_Data *upload_data = (Upload_Texture_To_GPU_Data *)input;
	
	Render_Context *render_ctx = upload_data->render_ctx;
	Texture *texture           = upload_data->texture;
	Buffer texture_data        = upload_data->texture_data;
	
	Textures_Upload_Buffer *upload_buffer = &render_ctx->upload_buffer;
	if (texture->state == Texture_State_Loading)
	{
		int req_channels = 4;
		int width, height, channels;
		u8 *pixels = stbi_load_from_memory(texture_data.data, int(texture_data.size), &width, &height, &channels, req_channels);
		assert(pixels);
		if (pixels)
		{
			texture->width  = u16(width);
			texture->height = u16(height);
			
			Buffer pixels_buf = make_buffer(pixels, width * height * req_channels); 
			push_texture_upload_request(upload_buffer, *texture, pixels_buf, 1);
			
			#if 0
				stbi_image_free(pixels);
			#endif
		}
		
		CompletePreviousWritesBeforeFutureWrites();
		texture->state = Texture_State_Loaded;
	}
	
	for (;;)
	{
		upload_data->next = render_ctx->free_texture_upload_data;
		
		if (platform->atomic_compare_and_exchange_pointer((volatile Address *)&render_ctx->free_texture_upload_data, upload_data->next, upload_data))
		{
			break;
		}
	}
}

internal void
init_opengl_renderer(OpenGL *opengl, Memory_Arena *arena)
{
	opengl->render_ctx.max_rects   = 5000;
	opengl->render_ctx.max_vertices_count = 4 * opengl->render_ctx.max_rects;
	opengl->render_ctx.max_indices_count = 6 * opengl->render_ctx.max_rects;
	
	opengl->render_ctx.vertex_array = m_arena_push_array(arena, Rect_Vertex_Data, opengl->render_ctx.max_vertices_count);
	opengl->render_ctx.index_array  = m_arena_push_array(arena, u16, opengl->render_ctx.max_indices_count);
	
	
	opengl->render_ctx.upload_texture_to_gpu_work = upload_texture_to_gpu_work;
	
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
	
	gl_compile_textured_rect_shader(opengl, &opengl->rect_shader);
}



internal void
gl_begin_frame(OpenGL *opengl, V2_I32 window_dim, V2_I32 draw_dim, Range2_I32 draw_region)
{
	opengl->render_ctx.vertices_count = 0;
	opengl->render_ctx.indices_count = 0;
	opengl->render_ctx.rects_count = 0;
	
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
	while (opengl->render_ctx.upload_buffer.tail < opengl->render_ctx.upload_buffer.head)
	{
		u64 entry_index = opengl->render_ctx.upload_buffer.tail % opengl->render_ctx.upload_buffer.capacity;
		Texture_Upload_Entry entry = opengl->render_ctx.upload_buffer.entries[entry_index];
		platform->atomic_increment64((volatile i64 *)&opengl->render_ctx.upload_buffer.tail);
		gl_upload_texture(opengl, entry.texture, entry.tex_buf);
		if (entry.should_free)
			free(entry.tex_buf.data);
	}
  
	Render_Context *render_ctx = (Render_Context *)opengl;
	
	// NOTE(fakhri): update buffers
	{
		glBindBuffer(GL_ARRAY_BUFFER, opengl->rect_shader.vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Rect_Vertex_Data) * render_ctx->vertices_count, render_ctx->vertex_array, GL_STREAM_DRAW);
		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl->rect_shader.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * render_ctx->indices_count, render_ctx->index_array, GL_STREAM_DRAW);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	
	for (u8 *cmd_ptr = render_ctx->commands.data, *cmds_ptr_end = render_ctx->commands.data + render_ctx->command_offset;
		cmd_ptr < cmds_ptr_end;)
	{
		Render_Entry_Header *header = (Render_Entry_Header *)cmd_ptr;
		cmd_ptr += sizeof(*header);
		
		#define RENDER_ENTRY_FROM_CMD_PTR(cmd_var_name, cmd_ptr, Type) \
		Type *cmd_var_name = (Type *)(cmd_ptr);                    \
		cmd_ptr += sizeof(Type);
		
		switch(header->kind)
		{
			case Render_Entry_Kind_Render_Entry_Clear_Color:
			{
				RENDER_ENTRY_FROM_CMD_PTR(cmd, cmd_ptr, Render_Entry_Clear_Color);
				
				glClearColor(cmd->color.r, cmd->color.g, cmd->color.b, cmd->color.a);
				glClear(GL_COLOR_BUFFER_BIT);
				
			} break;
			
			case Render_Entry_Kind_Render_Entry_Textured_Rects:
			{
				RENDER_ENTRY_FROM_CMD_PTR(cmd, cmd_ptr, Render_Entry_Textured_Rects);
				
				GL_Textured_Rect_Shader *rect_shader = &opengl->rect_shader;
				glUseProgram(rect_shader->prog_id);
				
				Render_Config config = cmd->config;
				
				Range2_F32 cull_range = config.cull_range;
				cull_range.minp = (config.proj.mat * vec4(cull_range.min_x, cull_range.min_y, 0, 1)).xy;
				cull_range.maxp = (config.proj.mat * vec4(cull_range.max_x, cull_range.max_y, 0, 1)).xy;
				
				i32 min_x = round_f32_i32(map_into_range(cull_range.min_x, -1, 1, 0, f32(opengl->window_dim.width)));
				i32 min_y = round_f32_i32(map_into_range(cull_range.min_y, -1, 1, 0, f32(opengl->window_dim.height)));
				
				i32 max_x = round_f32_i32(map_into_range(cull_range.max_x, -1, 1, 0, f32(opengl->window_dim.width)));
				i32 max_y = round_f32_i32(map_into_range(cull_range.max_y, -1, 1, 0, f32(opengl->window_dim.height)));
				
				glScissor(min_x, min_y,
					max_x - min_x,
					max_y - min_y);
				
				glUniformMatrix4fv(rect_shader->uniforms.clip,  1, GL_FALSE, (f32*)&config.proj.mat);
				
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, 
					gl_get_texture_handle(opengl, cmd->texture));
				
				glBindVertexArray(rect_shader->vao);
				
				
				#if 0
					u32 index_offset = cmd->index_offset;
				for (u32 i = 0; i < cmd->rects_count; i += 1)
				{
					Textured_Rect *rect = render_ctx->rects + cmd->rect_offset + i;
					
					glUniform2fv(rect_shader->uniforms.rect_cent,  1, (f32*)&rect->center);
					glUniform2fv(rect_shader->uniforms.rect_dim,  1, (f32*)&rect->dim);
					glUniform1f(rect_shader->uniforms.roundness, rect->roundness);
					
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void*)(index_offset * sizeof(u16)));
					index_offset += 6;
				}
				#else
					glDrawElements(GL_TRIANGLES, 6 * cmd->rects_count, GL_UNSIGNED_SHORT, (void*)(cmd->index_offset * sizeof(u16)));
				#endif
					
					glBindVertexArray(0);
				glBindTexture(GL_TEXTURE_2D, 0);
				glUseProgram(0);
			} break;
			
			invalid_default_case;
		}
		
		#undef RENDER_ENTRY_FROM_CMD_PTR
	}
}