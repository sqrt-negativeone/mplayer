
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <windowsx.h>
#undef min
#undef max
#undef near
#undef far

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
#include "mplayer_base.cpp"
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
w32_load_entire_file(String8 file_path, Memory_Arena *arena)
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
typedef Buffer Load_Entire_File(String8 file_path, Memory_Arena *arena);

#include "mplayer_renderer.cpp"
#include "mplayer.cpp"

#include "win32_mplayer_renderer_gl.cpp"

#pragma warning(disable : 4061)
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"
#pragma warning(default : 4061)

struct Audio_Input
{
	Mplayer_Context *mplayer;
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


internal Mplayer_Input_Key_Kind
w32_resolve_vk_code(WPARAM wParam)
{
	local_persist Mplayer_Input_Key_Kind key_table[256] = {0};
	local_persist b32 key_table_initialized = 0;
	
	if(!key_table_initialized)
	{
		key_table_initialized = 1;
		
		for (u32 i = 'A', j = Key_A; i <= 'Z'; i += 1, j += 1)
		{
			key_table[i] = (Mplayer_Input_Key_Kind)j;
		}
		for (u32 i = '0', j = Key_0; i <= '9'; i += 1, j += 1)
		{
			key_table[i] = (Mplayer_Input_Key_Kind)j;
		}
		for (u32 i = VK_F1, j = Key_F1; i <= VK_F24; i += 1, j += 1)
		{
			key_table[i] = (Mplayer_Input_Key_Kind)j;
		}
		
		key_table[VK_OEM_1]      = Key_SemiColon;
		key_table[VK_OEM_2]      = Key_ForwardSlash;
		key_table[VK_OEM_3]      = Key_GraveAccent;
		key_table[VK_OEM_4]      = Key_LeftBracket;
		key_table[VK_OEM_6]      = Key_RightBracket;
		key_table[VK_OEM_7]      = Key_Quote;
		key_table[VK_OEM_PERIOD] = Key_Period;
		key_table[VK_OEM_COMMA]  = Key_Comma;
		key_table[VK_OEM_MINUS]  = Key_Minus;
		key_table[VK_OEM_PLUS]   = Key_Plus;
		
		key_table[VK_ESCAPE]     = Key_Esc;
		key_table[VK_BACK]       = Key_Backspace;
		key_table[VK_TAB]        = Key_Tab;
		key_table[VK_SPACE]      = Key_Space;
		key_table[VK_RETURN]     = Key_Enter;
		key_table[VK_CONTROL]    = Key_Ctrl;
		key_table[VK_SHIFT]      = Key_Shift;
		key_table[VK_MENU]       = Key_Alt;
		key_table[VK_UP]         = Key_Up;
		key_table[VK_LEFT]       = Key_Left;
		key_table[VK_DOWN]       = Key_Down;
		key_table[VK_RIGHT]      = Key_Right;
		key_table[VK_DELETE]     = Key_Delete;
		key_table[VK_PRIOR]      = Key_PageUp;
		key_table[VK_NEXT]       = Key_PageDown;
		key_table[VK_HOME]       = Key_Home;
		key_table[VK_END]        = Key_End;
		
	}
	
	Mplayer_Input_Key_Kind key = Key_Unknown;
	if(wParam < array_count(key_table))
	{
		key = key_table[wParam];
	}
	return key;
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
			
			Mplayer_Context *mplayer = m_arena_push_struct(main_arena, Mplayer_Context);
			{
				mplayer->main_arena  = main_arena;
				mplayer->frame_arena = frame_arena;
				mplayer->render_ctx  = (Render_Context*)w32_renderer;
				
				mplayer->load_entire_file = w32_load_entire_file;
				mplayer_initialize(mplayer);
			}
			
			
			// NOTE(fakhri): Find refresh rate
			f32 refresh_rate = 60.0f;
			{
				DEVMODEW device_mode;
				if (EnumDisplaySettingsW(0, ENUM_CURRENT_SETTINGS, &device_mode))
				{
					refresh_rate = f32(device_mode.dmDisplayFrequency);
				}
			}
			f32 target_fps = refresh_rate;
			
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
				
				mplayer->input.frame_dt = 1.0f / target_fps;
				for (u32 i = 0; i < array_count(mplayer->input.buttons); i += 1)
				{
					mplayer->input.buttons[i].was_down = mplayer->input.buttons[i].is_down;
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
				
				b32 request_close = false;
				// NOTE(fakhri): process pending event
				{
					mplayer->input.first_event = 0;
					mplayer->input.last_event = 0;
					Mplayer_Input_Event *event = 0;
					MSG msg;
					for (;PeekMessage(&msg, 0, 0, 0, PM_REMOVE);)
					{
						switch (msg.message)
						{
							case WM_CLOSE:
							{
								request_close = true;
							} break;
							
							case WM_SYSKEYDOWN: fallthrough;
							case WM_SYSKEYUP: fallthrough;
							case WM_KEYDOWN: fallthrough;
							case WM_KEYUP:
							{
								WPARAM vk_code = msg.wParam;
								
								b32 alt_key_down = (msg.lParam & (1 << 29)) != 0;
								b32 was_down = (msg.lParam & (1 << 30)) != 0;
								b32 is_down = (msg.lParam & (1 << 31)) == 0;
								
								event = m_arena_push_struct(frame_arena, Mplayer_Input_Event);
								
								event->kind = (is_down)? Event_Kind_Press : Event_Kind_Release;
								event->key = w32_resolve_vk_code(vk_code);
								
								mplayer->input.buttons[event->key].is_down = b32(is_down);
							} break;
							
							
							case WM_CHAR: fallthrough;
							case WM_SYSCHAR:
							{
								u32 char_input = u32(msg.wParam);
								
								if ((char_input >= 32 && char_input != 127) || char_input == '\t' || char_input == '\n')
								{
									event = m_arena_push_struct(frame_arena, Mplayer_Input_Event);
									
									event->kind = Event_Kind_Text;
									event->text_character = char_input;
								}
							} break;
							
							case WM_MBUTTONDOWN: fallthrough;
							case WM_MBUTTONUP:
							{
								b32 is_down = b32(msg.message == WM_MBUTTONDOWN);
								event = m_arena_push_struct(frame_arena, Mplayer_Input_Event);
								event->kind = is_down? Event_Kind_Press: Event_Kind_Release;
								event->key = Key_MiddleMouse;
								mplayer->input.buttons[Key_MiddleMouse].is_down = is_down;
							} break;
							
							case WM_LBUTTONDOWN: fallthrough;
							case WM_LBUTTONUP:
							{
								b32 is_down = b32(msg.message == WM_LBUTTONDOWN);
								event = m_arena_push_struct(frame_arena, Mplayer_Input_Event);
								event->kind = is_down? Event_Kind_Press: Event_Kind_Release;
								event->key = Key_LeftMouse;
								mplayer->input.buttons[Key_LeftMouse].is_down = is_down;
							} break;
							
							case WM_RBUTTONDOWN: fallthrough;
							case WM_RBUTTONUP:
							{
								b32 is_down = b32(msg.message == WM_RBUTTONDOWN);
								event = m_arena_push_struct(frame_arena, Mplayer_Input_Event);
								event->kind = is_down? Event_Kind_Press: Event_Kind_Release;
								event->key = Key_RightMouse;
								mplayer->input.buttons[Key_RightMouse].is_down = is_down;
							} break;
							
							case WM_MOUSEWHEEL:
							{
								i16 wheel_delta = i16(HIWORD(u32(msg.wParam)));
								event = m_arena_push_struct(frame_arena, Mplayer_Input_Event);
								event->kind = Event_Kind_Mouse_Wheel;
								event->scroll.x = 0;
								event->scroll.y = f32(wheel_delta) / WHEEL_DELTA;
							} break;
							
							case WM_MOUSEMOVE:
							{
								f32 mouse_x = f32(GET_X_LPARAM(msg.lParam)); 
								f32 mouse_y = f32(GET_Y_LPARAM(msg.lParam)); 
								mouse_y = f32(window_dim.y - 1) - mouse_y;
								
								V2_F32 mouse_clip_pos;
								mouse_clip_pos.x = map_into_range_no(f32(draw_region.min_x), mouse_x, f32(draw_region.max_x));
								mouse_clip_pos.y = map_into_range_no(f32(draw_region.min_y), mouse_y, f32(draw_region.max_y));
								
								mplayer->input.mouse_clip_pos = mouse_clip_pos;
							} break;
						}
						
						if (event != 0)
						{
							if (event->kind != Event_Kind_Text)
							{
								if ((u16(GetKeyState(VK_SHIFT)) & 0x8000) != 0)
								{
									set_flag(event->modifiers, Modifier_Shift);
								}
								
								if ((u16(GetKeyState(VK_CONTROL)) & 0x8000) != 0)
								{
									set_flag(event->modifiers, Modifier_Ctrl);
								}
								
								if ((u16(GetKeyState(VK_MENU)) & 0x8000) != 0)
								{
									set_flag(event->modifiers, Modifier_Alt);
								}
							}
							
							push_input_event(&mplayer->input, event);
						}
						
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
				
				if (global_request_quit)
				{
					break;
				}
				
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