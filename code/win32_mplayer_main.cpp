
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#undef min
#undef max

#include "mplayer_base.h"
#include "mplayer_math.h"

internal void *
sys_allocate_memory(u64 size)
{
	void *result = malloc(size);
	return result;
}

#include "mplayer_memory.cpp"
#include "mplayer_thread_context.cpp"
#include "mplayer_string.cpp"
#include "mplayer_buffer.h"

internal void
w32_read_whole_block(HANDLE file, void *data, u64 size)
{
	u8 *ptr = (u8*)data;
	u8 *opl = ptr + size;
	for (;;)
	{
		u64 unread = (u64)(opl - ptr);
		DWORD to_read = (DWORD)(MIN(unread, 0xFFFFFFFF));
		DWORD did_read = 0;
		if (!ReadFile(file, ptr, to_read, &did_read, 0))
		{
			break;
		}
		ptr += did_read;
		if (ptr >= opl)
		{
			break;
		}
	}
}


internal Buffer
w32_read_entire_file(String8 file_path, Memory_Arena *arena)
{
	Buffer result = ZERO_STRUCT;
	
	DWORD desired_access = GENERIC_READ;
	DWORD share_mode = 0;
	SECURITY_ATTRIBUTES security_attributes = {
		(DWORD)sizeof(SECURITY_ATTRIBUTES),
		0,
		0,
	};
	DWORD creation_disposition = OPEN_EXISTING;
	DWORD flags_and_attributes = 0;
	HANDLE template_file = 0;
	
	HANDLE file = CreateFileA((LPCSTR)file_path.str,
		desired_access,
		share_mode,
		&security_attributes,
		creation_disposition,
		flags_and_attributes,
		template_file);
	
	if(file != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER size_int;
		if(GetFileSizeEx(file, &size_int) && size_int.QuadPart > 0)
		{
			u64 size = size_int.QuadPart;
			void *data = m_arena_push_array(arena, u8, size);
			w32_read_whole_block(file, data, size);
			result.data = (u8*)data;
			result.size = size;
		}
		CloseHandle(file);
	}
	
	return result;
}

#include "mplayer_renderer.cpp"
#include "mplayer.cpp"

#include "win32_mplayer_renderer_gl.cpp"


#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"

struct Audio_Input
{
	MPlayer_Context *mplayer;
};

internal void
audio_device_data(ma_device *device, void *output_buf, const void *input_buf, u32 frame_count)
{
	init_thread_context();
	Audio_Input *input = (Audio_Input*)device->pUserData;
	mplayer_get_audio_samples(input->mplayer, output_buf, frame_count);
}

#define W32_WINDOW_NAME "mplayer"
#define W32_CLASS_NAME (W32_WINDOW_NAME "_CLASS")
#define W32_WINDOW_W 1280
#define W32_WINDOW_H 720

b32 global_request_quit = 0;
internal LRESULT
Wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CLOSE:
		{
			global_request_quit = 1;
		} break;
	}
	return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	init_thread_context();
	Memory_Arena *main_arena = m_arena_make(gigabytes(4));
	Memory_Arena *frame_arena = m_arena_make(gigabytes(4));
	
	WNDCLASSA wc = ZERO_STRUCT;
	{
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = Wndproc;
		wc.hInstance = hInstance;
		wc.lpszClassName = W32_CLASS_NAME;
		wc.hCursor = LoadCursor(0, IDC_ARROW);
	}
  
	if (RegisterClassA(&wc))
	{
		HWND window = CreateWindowA(W32_CLASS_NAME,
			W32_WINDOW_NAME,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			W32_WINDOW_W, W32_WINDOW_H,
			0, 0, hInstance, 0);
		if (window)
		{
			HDC wdc = GetDC(window);
			W32_GL_Renderer *w32_renderer = w32_gl_make_renderer(hInstance, wdc);
			
			MPlayer_Context *mplayer = m_arena_push_struct(main_arena, MPlayer_Context);
			{
			mplayer->main_arena  = main_arena;
			mplayer->frame_arena = frame_arena;
			mplayer->render_ctx  = (Render_Context*)w32_renderer;
				
				mplayer->read_entire_file = w32_read_entire_file;
				mplayer_initialize(mplayer);
			}
			
			Audio_Input audio_input = ZERO_STRUCT;
			audio_input.mplayer = mplayer;
			
			ma_device_config config = ma_device_config_init(ma_device_type_playback);
			config.playback.format   = ma_format_f32;  // Set to ma_format_unknown to use the device's native format.
			config.playback.channels = u32(mplayer->flac_stream.streaminfo.nb_channels);     // Set to 0 to use the device's native channel count.
			config.sampleRate        = mplayer->flac_stream.streaminfo.sample_rate; // Set to 0 to use the device's native sample rate.
			config.dataCallback      = audio_device_data; // Set to 0 to use the device's native sample rate.
			config.pUserData         = &audio_input; // Set to 0 to use the device's native sample rate.
			
			ma_device device;
			if (ma_device_init(0, &config, &device) != MA_SUCCESS)
			{
				printf("failed to initialize miniaudio device");
				return 1;
			}
			
			ma_device_start(&device);
			
			ShowWindow(window, nShowCmd);
			UpdateWindow(window);
			
			b32 running = true;
			for (;running;)
			{
				m_arena_free_all(frame_arena);
				b32 request_close = false;
				// NOTE(fakhri): process pending event
				{
					MSG msg;
					for (;PeekMessage(&msg, 0, 0, 0, PM_REMOVE);)
					{
						switch (msg.message)
						{
							case WM_CLOSE:
							{
								request_close = true;
							} break;
						}
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
				
				if (global_request_quit)
				{
					break;
				}
				
				V2_I32 window_dim;
				{
					RECT client_rect;
					GetClientRect(window, &client_rect);
					window_dim.x = client_rect.right - client_rect.left;
					window_dim.y = client_rect.bottom - client_rect.top;
				}
				
				V2_I32 draw_dim = window_dim;
				Range2_I32 draw_region = compute_draw_region_aspect_ratio_fit(draw_dim, window_dim);
				
				w32_gl_render_begin(w32_renderer, window_dim, draw_dim, draw_region);
				mplayer_update_and_render(mplayer);
				w32_gl_render_end(w32_renderer);
				
				running = !(request_close || global_request_quit);
			}
			
			ma_device_stop(&device);
		}
	}
	
	return 0;
}