
#include <gl/gl.h>
#include "third_party/wglext.h"
#include "third_party/glext.h"

#define GLProc(name, type) PFNGL##type##PROC name = 0;
#include "gl_functions.inc.h"

#define STBI_SSE2
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include "mplayer_renderer.h"
#include "mplayer_renderer.cpp"
#include "mplayer_opengl.cpp"

struct W32_GL_Renderer
{
	OpenGL opengl;
	HDC dc;
};

global PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
global PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
global PFNWGLMAKECONTEXTCURRENTARBPROC wglMakeContextCurrentARB;
global PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

internal void_function *
w32_get_gl_proc_address(const char *name)
{
  void_function *p = (void_function *)wglGetProcAddress(name);
  if(!p || p == (void_function*)0x1 || p == (void_function*)0x2 || p == (void_function*)0x3 || p == (void_function*)-1)
  {
    p = 0;
  }
  return p;
}

internal b32
w32_init_wgl(HINSTANCE hInstance, HDC dc)
{
	b32 result = false;
  
  //- NOTE(fakhri): make global invisible window
  HWND dummy_window = CreateWindowW(L"STATIC",
		L"",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		100, 100,
		0, 0,
		hInstance,
		0);
  assert(dummy_window);
  if (dummy_window)
  {
    HDC dummy_dc = GetDC(dummy_window);
    if (dummy_dc)
    {
      //- NOTE(fakhri): make dummy context
      HGLRC dummy_gl_conetxt;
      {
        PIXELFORMATDESCRIPTOR pfd = ZERO_STRUCT;
        {
          pfd.nSize           = sizeof(PIXELFORMATDESCRIPTOR);
          pfd.nVersion        = 1;
          pfd.dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
          pfd.iPixelType      = PFD_TYPE_RGBA;
          pfd.cColorBits      = 32;
          pfd.cDepthBits      = 24;
          pfd.cStencilBits    = 8;
          pfd.iLayerType      = PFD_MAIN_PLANE;
        };
        
        int dummy_pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
        if (dummy_pixel_format == 0) return result;
        
        SetPixelFormat(dummy_dc, dummy_pixel_format, &pfd);
        dummy_gl_conetxt = wglCreateContext(dummy_dc);
        
        wglMakeCurrent(dummy_dc, dummy_gl_conetxt);
				{
					wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC)    w32_get_gl_proc_address("wglChoosePixelFormatARB");
					wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) w32_get_gl_proc_address("wglCreateContextAttribsARB");
					wglMakeContextCurrentARB   = (PFNWGLMAKECONTEXTCURRENTARBPROC)   w32_get_gl_proc_address("wglMakeContextCurrentARB");
					wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC)         w32_get_gl_proc_address("wglSwapIntervalEXT");
				}
        wglMakeCurrent(0, 0);
        
        wglDeleteContext(dummy_gl_conetxt);
      }
      
      //- NOTE(fakhri): setup real pixel format
      int pixel_format;
      PIXELFORMATDESCRIPTOR pixel_format_descriptor = ZERO_STRUCT;
      pixel_format_descriptor.nSize = sizeof(PIXELFORMATDESCRIPTOR);
      
      {
        int pixel_format_attributes[] = {
          WGL_DRAW_TO_WINDOW_ARB, 1,
          WGL_SUPPORT_OPENGL_ARB, 1,
          WGL_DOUBLE_BUFFER_ARB,  1,
          WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
          WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
          WGL_COLOR_BITS_ARB, 24,
          WGL_DEPTH_BITS_ARB, 24,
          WGL_STENCIL_BITS_ARB, 8,
          WGL_SAMPLE_BUFFERS_ARB, 1,
          WGL_SAMPLES_ARB, 4,
          0,
        };
        
        u32 format_cnt;
        wglChoosePixelFormatARB(dc, &pixel_format_attributes[0], 0, 1, &pixel_format, &format_cnt);
        SetPixelFormat(dc, pixel_format, &pixel_format_descriptor);
      }
      
      HGLRC gl_ctx;
      //- NOTE(fakhri): initialize real context
      {
        int ctx_attribs[] = {
          WGL_CONTEXT_MAJOR_VERSION_ARB, GL_VERSION_MAJOR,
          WGL_CONTEXT_MINOR_VERSION_ARB, GL_VERSION_MINOR,
          WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
          WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
					#if DEBUG_BUILD
						| WGL_CONTEXT_DEBUG_BIT_ARB
					#endif
						,
          0,
        };
        
        gl_ctx = wglCreateContextAttribsARB(dc, 0, &ctx_attribs[0]);
        
        if (gl_ctx)
        {
          result = true;
          wglMakeCurrent(dc, gl_ctx);
          SetPixelFormat(dc, pixel_format, &pixel_format_descriptor);
          if (wglSwapIntervalEXT)
          {
            wglSwapIntervalEXT(1);
          }
        }
      }
      
      
      ReleaseDC(dummy_window, dummy_dc);
    }
    DestroyWindow(dummy_window);
  }
  
  return result;
}

internal void
w32_load_gl_procs()
{
	#define GLProc(name, type) name = (PFNGL##type##PROC)w32_get_gl_proc_address(#name); assert(name);
	#include "gl_functions.inc.h"
}

internal W32_GL_Renderer *
w32_gl_make_renderer(HINSTANCE hInstance, HDC dc)
{
	W32_GL_Renderer *result = 0;
	if (w32_init_wgl(hInstance, dc))
	{
		Memory_Arena *render_arena = (Memory_Arena *)_m_arena_bootstrap_push(sizeof(Memory_Arena), 0);
		w32_load_gl_procs();
		result = m_arena_push_struct(render_arena, W32_GL_Renderer);
		assert(result);
		result->dc = dc;
		
		// NOTE(fakhri): platform vtable
		{
			((OpenGL*)result)->load_entire_file = w32_load_entire_file;
		}
		
		init_opengl_renderer((OpenGL*)result, render_arena);
	}
	else
	{
		invalid_code_path("couldn't init modern opengl context");
	}
	
	return result;
}


internal void
w32_gl_render_begin(W32_GL_Renderer *wgl, V2_I32 window_dim, V2_I32 draw_dim, Range2_I32 draw_region)
{
	gl_begin_frame((OpenGL*)wgl, window_dim, draw_dim, draw_region);
}

internal void
w32_gl_render_end(W32_GL_Renderer *wgl)
{
	gl_end_frame((OpenGL*)wgl);
	SwapBuffers(wgl->dc);
}
