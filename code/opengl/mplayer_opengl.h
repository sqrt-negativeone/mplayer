/* date = March 9th 2025 1:05 pm */

#ifndef MPLAYER_OPENGL_H
#define MPLAYER_OPENGL_H

#include "third_party/glext.h"
#include "mplayer/mplayer_renderer.h"


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


struct OpenGL
{
	Render_Context render_ctx;
	Range2_I32 draw_region;
	V2_I32 window_dim;
	
	u32 *textures2d_array;
	Load_Entire_File *load_entire_file;
	
	GL_Textured_Rect_Shader rect_shader;
};

#endif //MPLAYER_OPENGL_H
