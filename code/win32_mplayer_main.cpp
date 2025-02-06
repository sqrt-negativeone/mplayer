
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include <timeapi.h>
#include <Objbase.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#define CompletePreviousWritesBeforeFutureWrites() MemoryBarrier(); __faststorefence()

#undef near
#undef far

#include "mplayer.h"


struct W32_Timer
{
	LARGE_INTEGER start_counter;
};


global Mplayer_OS_Vtable w32_vtable;

global HANDLE g_shit_event;
global HWND g_w32_window;
global Mplayer_Context *g_w32_mplayer;
global Mplayer_Input g_w32_input;
global Memory_Arena g_w32_frame_arena;
global W32_Timer g_w32_update_timer;
global W32_Timer g_w32_audio_timer;
global Cursor_Shape g_w32_cursor = Cursor_Arrow;
global DWORD g_w32_main_thread_id;
global HANDLE g_audio_mutex; 

internal void
w32_fix_path_slashes(String8 path)
{
	for(u32 i = 0; i < path.len; i += 1)
	{
		if (path.str[i] == '/') path.str[i] = '\\';
	}
}

internal void
w32_make_folder_if_missing(String8 dir)
{
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	String8 path = str8_clone(scratch.arena, dir);
	w32_fix_path_slashes(path);
	
	char *p = path.cstr;
	for(u32 i = 0; i < path.len; i += 1)
	{
		if (path.str[i] == '\\')
		{
			path.str[i] = 0;
			CreateDirectoryA(path.cstr, 0);
			path.str[i] = '\\';
		}
	}
	
	CreateDirectoryA(path.cstr, 0);
}

internal void
w32_set_cursor(Cursor_Shape cursor)
{
	g_w32_cursor = cursor;
}

internal i64
w32_atomic_increment64(volatile i64 *addend)
{
	i64 old_val = InterlockedIncrement64(addend) - 1;
	return old_val;
}

internal i64
w32_atomic_decrement64(volatile i64 *addend)
{
	i64 old_val = InterlockedDecrement64(addend) + 1;
	return old_val;
}

internal b32
w32_atomic_compare_and_exchange64(i64 volatile *dst, i64 expect, i64 exchange)
{
  b32 result = (InterlockedCompareExchange64(dst, exchange, expect) == expect);
  return result;
}

internal b32
w32_atomic_compare_and_exchange_pointer(Address volatile *dst, Address expect, Address exchange)
{
  b32 result = (InterlockedCompareExchangePointer(dst, exchange, expect) == expect);
  return result;
}


internal void *
w32_allocate_memory(u64 size)
{
	void *result = VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return result;
}

internal void
w32_deallocate_memory(void *memory)
{
	VirtualFree(memory, 0, MEM_RELEASE);
}

#define File_Open_ReadWrite (File_Open_Read | File_Open_Write)

internal File_Handle *
w32_open_file(String8 path, u32 flags)
{
	File_Handle *result = 0;
	
	DWORD desired_access = 0;
	if (has_flag(flags, File_Open_Read))
	{
		desired_access |= GENERIC_READ;
	}
	
	if (has_flag(flags, File_Open_Write))
	{
		desired_access |= GENERIC_WRITE;
	}
	
	DWORD share_mode = 0;
	if (!has_flag(flags, File_Open_Write))
	{
		share_mode |= FILE_SHARE_READ;
	}
	
	SECURITY_ATTRIBUTES security_attributes = {
		(DWORD)sizeof(SECURITY_ATTRIBUTES),
		0,
		0,
	};
	DWORD creation_disposition = OPEN_EXISTING;
	if (has_flag(flags,  File_Create_Always))
	{
		creation_disposition = CREATE_ALWAYS;
	}
	
	DWORD flags_and_attributes = 0;
	HANDLE template_file = 0;
	
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	String16 cpath16 = str16_from_8(scratch.arena, path);
	
	result = CreateFileW((LPCWSTR)cpath16.str,
		desired_access,
		share_mode,
		&security_attributes,
		creation_disposition,
		flags_and_attributes,
		template_file);
	return result;
}

internal void
w32_close_file(File_Handle *file)
{
	CloseHandle((HANDLE)file);
}

internal Buffer
w32_read_whole_block(File_Handle *file, void *data, u64 size)
{
	Buffer result = ZERO_STRUCT;
	
	HANDLE file_handle = (HANDLE)file;
	u8 *ptr = (u8*)data;
	u8 *opl = ptr + size;
	for (;;)
	{
		u64 unread = (u64)(opl - ptr);
		DWORD to_read = (DWORD)(MIN(unread, 0xFFFFFFFF));
		DWORD did_read = 0;
		if (!ReadFile(file_handle, ptr, to_read, &did_read, 0))
		{
			break;
		}
		if (!did_read)
		{
			break;
		}
		ptr += did_read;
		if (ptr >= opl)
		{
			break;
		}
	}
	
	result.data = (u8*)data;
	result.size = ptr - result.data;
	return result;
}

internal b32
w32_write_whole_block(File_Handle *file, void *buf, u64 size)
{
	b32 result = 1;
	HANDLE file_handle = (HANDLE)file;
	
	u8 *buf_ptr = (u8*)buf;
	u64 bytes_to_write = size;
	for (;bytes_to_write;)
	{
		DWORD bytes_written = 0;
		DWORD bytes_to_request = (DWORD)MIN(bytes_to_write, 0xFFFFFFFF);
		if (!WriteFile(file_handle, buf_ptr, bytes_to_request, &bytes_written, 0))
		{
			result = 0;
			break;
		}
		bytes_to_write -= bytes_written;
	}
	
	return result;
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
	
	Memory_Checkpoint_Scoped scratch(get_scratch(&arena, 1));
	String16 cpath16 = str16_from_8(scratch.arena, file_path);
	
	HANDLE file = w32_open_file(file_path, File_Open_Read);
	
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
		w32_close_file(file);
	}
	
	return result;
}

#include "win32_mplayer_renderer_gl.cpp"
global W32_GL_Renderer *g_w32_renderer;

#define W32_EXE_NAME "mplayer"
#define W32_WINDOW_NAME "mplayer"
#define W32_CLASS_NAME (W32_WINDOW_NAME "_CLASS")
#define W32_WINDOW_W 1280
#define W32_WINDOW_H 720

b32 global_request_quit = 0;
internal void w32_update();

internal LRESULT
Wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CLOSE:
		{
			SetEvent(g_shit_event);
			global_request_quit = 1;
			ExitProcess(0);
		} break;
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lparam;
			lpMMI->ptMinTrackSize.x = 650;
			lpMMI->ptMinTrackSize.y = 170;
			return 0;
		} break;
		case WM_SETCURSOR:
		{
			global HCURSOR cursors[Cursor_COUNT];
			local_persist b32 cursors_inited = 0;
			if (!cursors_inited)
			{
				cursors_inited = 1;
				
				for (u32 i = 0; i < Cursor_COUNT; i += 1)
				{
					Cursor_Shape shape = (Cursor_Shape)i;
					
					LPCSTR cur_name = 0;
					switch(shape)
					{
						case Cursor_Arrow:             cur_name = IDC_ARROW; break;
						case Cursor_Hand:              cur_name = IDC_HAND; break;
						case Cursor_Horizontal_Resize: cur_name = IDC_SIZEWE; break;
						case Cursor_Vertical_Resize:   cur_name = IDC_SIZENS; break;
						case Cursor_Wait:              cur_name = IDC_WAIT; break;
						case Cursor_TextSelect:        cur_name = IDC_IBEAM; break;
						case Cursor_Unavailable:       cur_name = IDC_NO; break;
						
						default:
						case Cursor_COUNT: invalid_code_path();
					}
					
					cursors[i] = LoadCursor(0, cur_name);
					
				}
			}
			
			
			RECT client_rect;
			GetClientRect(hwnd, &client_rect);
			
			POINT point;
			if (GetCursorPos(&point))
			{
				ScreenToClient(hwnd, &point);
			}
			if (is_in_range(range((f32)client_rect.left, (f32)client_rect.right, (f32)client_rect.top, (f32)client_rect.bottom),
					vec2((f32)point.x, (f32)point.y)))
			{
				SetCursor(cursors[g_w32_cursor]);
				return 0;
			}
		} break;
		
		case WM_SIZE:
		{
			SetEvent(g_shit_event);
		}
	}
	return DefWindowProcA(hwnd, msg, wparam, lparam);
}


internal Mplayer_Keyboard_Key_Kind
w32_resolve_vk_code(WPARAM wParam)
{
	local_persist Mplayer_Keyboard_Key_Kind key_table[256] = {0};
	local_persist b32 key_table_initialized = 0;
	
	if(!key_table_initialized)
	{
		key_table_initialized = 1;
		
		for (u32 i = 'A', j = Keyboard_A; i <= 'Z'; i += 1, j += 1)
		{
			key_table[i] = (Mplayer_Keyboard_Key_Kind)j;
		}
		for (u32 i = '0', j = Keyboard_0; i <= '9'; i += 1, j += 1)
		{
			key_table[i] = (Mplayer_Keyboard_Key_Kind)j;
		}
		for (u32 i = VK_F1, j = Keyboard_F1; i <= VK_F24; i += 1, j += 1)
		{
			key_table[i] = (Mplayer_Keyboard_Key_Kind)j;
		}
		
		key_table[VK_OEM_1]      = Keyboard_SemiColon;
		key_table[VK_OEM_2]      = Keyboard_ForwardSlash;
		key_table[VK_OEM_3]      = Keyboard_GraveAccent;
		key_table[VK_OEM_4]      = Keyboard_LeftBracket;
		key_table[VK_OEM_6]      = Keyboard_RightBracket;
		key_table[VK_OEM_7]      = Keyboard_Quote;
		key_table[VK_OEM_PERIOD] = Keyboard_Period;
		key_table[VK_OEM_COMMA]  = Keyboard_Comma;
		key_table[VK_OEM_MINUS]  = Keyboard_Minus;
		key_table[VK_OEM_PLUS]   = Keyboard_Plus;
		
		key_table[VK_ESCAPE]     = Keyboard_Esc;
		key_table[VK_BACK]       = Keyboard_Backspace;
		key_table[VK_TAB]        = Keyboard_Tab;
		key_table[VK_SPACE]      = Keyboard_Space;
		key_table[VK_RETURN]     = Keyboard_Enter;
		key_table[VK_CONTROL]    = Keyboard_Ctrl;
		key_table[VK_SHIFT]      = Keyboard_Shift;
		key_table[VK_MENU]       = Keyboard_Alt;
		key_table[VK_UP]         = Keyboard_Up;
		key_table[VK_LEFT]       = Keyboard_Left;
		key_table[VK_DOWN]       = Keyboard_Down;
		key_table[VK_RIGHT]      = Keyboard_Right;
		key_table[VK_DELETE]     = Keyboard_Delete;
		key_table[VK_PRIOR]      = Keyboard_PageUp;
		key_table[VK_NEXT]       = Keyboard_PageDown;
		key_table[VK_HOME]       = Keyboard_Home;
		key_table[VK_END]        = Keyboard_End;
		
	}
	
	Mplayer_Keyboard_Key_Kind key = Keyboard_Unknown;
	if(wParam < array_count(key_table))
	{
		key = key_table[wParam];
	}
	return key;
}

internal void
w32_output_err(const char *title, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	u32 required_characters = vsnprintf(0, 0, format, args)+1;
	va_end(args);
	
	char text[4096] = {0};
	if(required_characters > 4096)
	{
		required_characters = 4096;
	}
	
	va_start(args, format);
	vsnprintf(text, required_characters, format, args);
	va_end(args);
	
	text[required_characters-1] = 0;
	MessageBoxA(0, text, title, MB_OK);
}

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

#define EXIT_ON_ERROR(err, title, message, ...)  \
if ((err) != S_OK)                             \
{                                              \
w32_output_err(title, message, __VA_ARGS__); \
ExitProcess(1);                              \
} 

struct W32_Sound_Output
{
	b32 initialized;
	
	IMMDeviceEnumerator *enumerator;
	IMMDevice *device;
	IAudioClient *audio_client;
	IAudioRenderClient *render_client;
	u32 buffer_frame_count;
	REFERENCE_TIME buffer_duration;
	Sound_Config config;
};
global W32_Sound_Output g_w32_sound_output;

internal W32_Sound_Output
w32_init_wasapi(u32 sample_rate, u16 channels_count)
{
	W32_Sound_Output sound_output = ZERO_STRUCT;
	assert(channels_count <= 2);
	
	IMMDeviceEnumerator *enumerator = 0;
	IMMDevice *device = 0;
	IAudioClient *audio_client = 0;
	IAudioRenderClient *render_client = 0;
	u32 buffer_frame_count = 0;
	
	CoInitializeEx(0, COINIT_SPEED_OVER_MEMORY);
	
	HRESULT res = CoCreateInstance(CLSID_MMDeviceEnumerator, 0,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&enumerator);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed to create Device Enumartor");
	
	res = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed get default audio endpoint");
	
	
	res = device->Activate(IID_IAudioClient, CLSCTX_ALL,
		0, (void**)&audio_client);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed to activate device");
	
	WAVEFORMATEX *wfx = 0;
	res = audio_client->GetMixFormat(&wfx);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed to get mix format");
	
	WAVEFORMATEX new_wf = *wfx;
	{
		new_wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    new_wf.nChannels  = channels_count;
		new_wf.wBitsPerSample = 32;
		new_wf.nSamplesPerSec = sample_rate;
		new_wf.nBlockAlign = (WORD)((channels_count * new_wf.wBitsPerSample) / 8);
		new_wf.nAvgBytesPerSec = new_wf.nBlockAlign * sample_rate;
    new_wf.cbSize = 0;
	}
	
	REFERENCE_TIME requested_duration = 1 * REFTIMES_PER_SEC;
	res = audio_client->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		requested_duration,
		0,
		&new_wf,
		0);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed to Initialize audio");
	
	res = audio_client->GetBufferSize(&buffer_frame_count);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed to get buffer size");
	
	res = audio_client->GetService(
		IID_IAudioRenderClient,
		(void**)&render_client);
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed to Get render client");
	
	res = audio_client->Start();
	EXIT_ON_ERROR(res, "WASAPI ERROR", "Failed Start playing audio");
	
	sound_output.initialized = true;
	
	sound_output.enumerator            = enumerator;
	sound_output.device                = device;
	sound_output.audio_client          = audio_client;
	sound_output.render_client         = render_client;
	sound_output.buffer_frame_count    = buffer_frame_count;
	sound_output.buffer_duration       = (REFERENCE_TIME)((f64)REFTIMES_PER_SEC * buffer_frame_count / sample_rate);
	sound_output.config.sample_rate    = sample_rate;
	sound_output.config.channels_count = channels_count;
	
	return sound_output;
}

global LARGE_INTEGER w32_timer_freq;

internal void
w32_update_timer(W32_Timer *timer)
{
	QueryPerformanceCounter(&timer->start_counter);
}

internal f32
w32_get_seconds_elapsed(W32_Timer *timer)
{
	LARGE_INTEGER end_counter;
	QueryPerformanceCounter(&end_counter);
	
	u64 ticks = end_counter.QuadPart - timer->start_counter.QuadPart;
	f32 result = (f32)ticks / (f32)w32_timer_freq.QuadPart;
	return result;
}

struct W32_File_Iterator
{
	HANDLE state;
	b32 first;
	WIN32_FIND_DATAW find_data;
};

internal b32
w32_file_iter_begin(File_Iterator_Handle *it, String8 path)
{
	Memory_Checkpoint_Scoped scratch(get_scratch(0, 0));
	String8 cpath;
	if (path.str[path.len - 1] == '/' || 
		path.str[path.len - 1] == '\\' )
	{
		cpath = str8_f(scratch.arena, "%.*s*", STR8_EXPAND(path));
	}
	else
	{
		cpath = str8_f(scratch.arena, "%.*s/*", STR8_EXPAND(path));
	}
	
	String16 cpath16 = str16_from_8(scratch.arena, cpath);
	
  WIN32_FIND_DATAW find_data = ZERO_STRUCT;
  HANDLE state = FindFirstFileW((WCHAR*)cpath16.str, &find_data);
  
  //- fill results
  b32 result = !!state;
  if (result)
  {
    W32_File_Iterator *win32_it = (W32_File_Iterator*)it; 
    win32_it->state = state;
    win32_it->first = 1;
    memory_copy(&win32_it->find_data, &find_data, sizeof(find_data));
  }
	
	return result;
}


internal File_Info
w32_file_iter_next(File_Iterator_Handle *it, Memory_Arena *arena)
{
  //- get low-level file info for this step
  b32 good = 0;
  
  W32_File_Iterator *win32_it = (W32_File_Iterator*)it; 
  WIN32_FIND_DATAW *find_data = &win32_it->find_data;
  if (win32_it->first)
  {
    win32_it->first = 0;
    good = 1;
  }
  else
  {
    good = FindNextFileW(win32_it->state, find_data);
  }
  
  //- convert to File_Info
  File_Info result = {0};
  if (good)
  {
    set_flag(result.flags, FileFlag_Valid);
    
    if (find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
      set_flag(result.flags, FileFlag_Directory);
    }
    u16 *filename_base = (u16*)find_data->cFileName;
    u16 *ptr = filename_base;
    for (;*ptr != 0; ptr += 1);
    String16 filename16 = {0};
    filename16.str = filename_base;
    filename16.len = (u64)(ptr - filename_base);
    result.name = str8_from_16(arena, filename16);
    result.size = ((((u64)find_data->nFileSizeHigh) << 32) |
			((u64)find_data->nFileSizeLow));
  }
  return result;
}


internal void
w32_file_iter_end(File_Iterator_Handle *it)
{
  W32_File_Iterator *win32_it = (W32_File_Iterator*)it; 
  FindClose(win32_it->state);
}


internal u32
w32_get_directory_files_count(String16 search_path16)
{
	u32 count = 0;
	WIN32_FIND_DATAW find_data = ZERO_STRUCT;
  HANDLE hFind = FindFirstFileW((WCHAR*)search_path16.str, &find_data);
	if (INVALID_HANDLE_VALUE != hFind)
	{
		do { count += 1;} while (FindNextFileW(hFind, &find_data) != 0);
	}
	FindClose(hFind);
	return count;
}

internal Directory
w32_read_directory(Memory_Arena *arena, String8 path)
{
	Directory result = ZERO_STRUCT;
	Memory_Checkpoint_Scoped scratch(get_scratch(&arena, 1));
	
	String8 search_path;
	if (path.str[path.len - 1] == '/' || 
		path.str[path.len - 1] == '\\' )
	{
		search_path = str8_f(scratch.arena, "%.*s*", STR8_EXPAND(path));
	}
	else
	{
		search_path = str8_f(scratch.arena, "%.*s/*", STR8_EXPAND(path));
	}
	String16 search_path16 = str16_from_8(scratch.arena, search_path);
	
	result.count = w32_get_directory_files_count(search_path16);
	
	// NOTE(fakhri): fill the content
	if (result.count)
	{
		result.files = m_arena_push_array_z(arena, File_Info, result.count);
		
		WIN32_FIND_DATAW find_data = ZERO_STRUCT;
		HANDLE hFind = FindFirstFileW((WCHAR*)search_path16.str, &find_data);
		assert(INVALID_HANDLE_VALUE != hFind);
		
		for (u32 i = 0; i < result.count; i += 1)
		{
			File_Info *info = result.files + i;
			
			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				set_flag(info->flags, FileFlag_Directory);
			}
			
			u16 *filename_base = (u16*)find_data.cFileName;
			u16 *ptr = filename_base;
			for (;*ptr; ptr += 1);
			String16 filename16 = str16(filename_base, (u64)(ptr - filename_base));
			info->name = str8_from_16(arena, filename16);
			info->size = ((((u64)find_data.nFileSizeHigh) << 32) |
				((u64)find_data.nFileSizeLow));
			// TODO(fakhri): set the extension
			info->path = str8_f(arena, "%.*s%.*s", search_path.len - 1, search_path.str, STR8_EXPAND(info->name));
			if (!FindNextFileW(hFind, &find_data))
			{
				assert(i == result.count - 1);
				break;
			}
		}
		
		FindClose(hFind);
	}
	
	return result;
}

global SRWLOCK g_w32_cwd_lock;

internal String8
w32_get_current_directory(Memory_Arena *arena)
{
	Memory_Checkpoint_Scoped scratch(get_scratch(&arena, 1));
	
	String8 cwd = ZERO_STRUCT;
	AcquireSRWLockShared(&g_w32_cwd_lock);
	
	u64 size16 = GetCurrentDirectoryW(0, 0);
	String16 path16;
	path16.len = size16-1;
	path16.str = m_arena_push_array(scratch.arena, u16, size16);
	
	if (GetCurrentDirectoryW((DWORD)size16, (LPWSTR)path16.str))
	{
		cwd = str8_from_16(scratch.arena, path16);
		
		if (cwd.str[cwd.len - 1] == '/' ||
			cwd.str[cwd.len - 1] == '\\')
		{
			cwd = str8_clone(arena, cwd);
		}
		else
		{
			cwd = str8_f(arena, "%.*s\\", STR8_EXPAND(cwd));
		}
	}
	
	ReleaseSRWLockShared(&g_w32_cwd_lock);
	return cwd;
}

// NOTE(fakhri): single producer multiple consumers
struct Work_Queue
{
	Work_Entry entries[1024];
	volatile u64 head; // push to head
	volatile u64 tail; // pop from tail
	
	HANDLE semaphore;
};

global Work_Queue g_w32_queue;

// NOTE(fakhri): single producer
internal b32
w32_push_work_sp(Work_Proc *work, void *input)
{
	b32 result = 0;
	
	if (g_w32_queue.head - g_w32_queue.tail < array_count(g_w32_queue.entries))
	{
		result = true;
		
		u64 entry_index = g_w32_queue.head % array_count(g_w32_queue.entries);
		g_w32_queue.entries[entry_index].work = work;
		g_w32_queue.entries[entry_index].input = input;
		
		CompletePreviousWritesBeforeFutureWrites();
		g_w32_queue.head += 1;
		
		ReleaseSemaphore(g_w32_queue.semaphore, 1, 0);
	}
	return result;
}

struct Next_Work
{
	Work_Entry entry;
	b32 valid;
};

internal Next_Work
w32_get_next_work()
{
	Next_Work result = ZERO_STRUCT;
	for (;;)
	{
		u64 read_tail = g_w32_queue.tail;
		if (g_w32_queue.tail != g_w32_queue.head)
		{
			u64 entry_index = read_tail % array_count(g_w32_queue.entries);
			Work_Entry entry = g_w32_queue.entries[entry_index];
			
			if (w32_atomic_compare_and_exchange64((volatile LONG64 *)&g_w32_queue.tail, read_tail, read_tail + 1))
			{
				result.valid = true;
				result.entry = entry;
				break;
			}
		}
		else
		{
			// NOTE(fakhri): empty
			break;
		}
	}
	return result;
}

internal b32
w32_do_next_work()
{
	b32 result = 0;
	Next_Work next_work = w32_get_next_work();
	if (next_work.valid)
	{
		result = true;
		assert(next_work.entry.work);
		next_work.entry.work(next_work.entry.input);
	}
	return result;
}

struct Worker_Thread_Context
{
	u32 id;
};

DWORD WINAPI 
w32_worker_thread_main(void *parameter)
{
	init_thread_context();
	Worker_Thread_Context *work_ctx = (Worker_Thread_Context*)parameter;
	
	for (;;)
	{
		if (!w32_do_next_work())
		{
			WaitForSingleObject(g_w32_queue.semaphore, INFINITE);
		}
	}
}

struct W32_Event_Queue
{
	Mplayer_Input_Event events[1024];
	
	volatile u64 head;
	volatile u64 tail;
};

struct Mplayer_Code
{
	HMODULE dll;
	FILETIME last_write_time;
	
	Mplayer_Exported_Vtable vtable;
};

global Mplayer_Code w32_app_code;

global char w32_exe_path[256];
global char w32_exe_dir[256];
global char w32_app_dll_path[256];
global char w32_temp_app_dll_path[256];

internal FILETIME
w32_get_last_write_time(char *filename)
{
	FILETIME last_write_time = {0};
	WIN32_FIND_DATA find_data;
	HANDLE find_handle = FindFirstFileA(filename, &find_data);
	if(find_handle != INVALID_HANDLE_VALUE)
	{
		FindClose(find_handle);
		last_write_time = find_data.ftLastWriteTime;
	}
	return last_write_time;
}

internal void
w32_set_code_stub(Mplayer_Code *code)
{
	code->vtable = g_vtable_stub;
}

internal b32
w32_load_app_code(Mplayer_Code *code)
{
	b32 result = 0;
	if (CopyFile(w32_app_dll_path, w32_temp_app_dll_path, FALSE))
	{
		code->dll = LoadLibraryA(w32_temp_app_dll_path);
		if (code->dll)
		{
			code->last_write_time = w32_get_last_write_time(w32_temp_app_dll_path);
			
			Mplayer_Get_Vtable *mplayer_get_vtable = (Mplayer_Get_Vtable *)GetProcAddress(code->dll, "mplayer_get_vtable");
			
			if (mplayer_get_vtable)
			{
				result = 1;
				code->vtable = mplayer_get_vtable();
			}
			else
			{
				w32_set_code_stub(code);
			}
		}
	}
	return result;
}

internal void
w32_code_unload(Mplayer_Code *code)
{
	if (code->dll)
	{
		FreeLibrary(code->dll);
	}
	code->dll = 0;
	w32_set_code_stub(code);
}

internal void
w32_update_code(Mplayer_Context *mplayer, Mplayer_Input *input, Mplayer_Code *code)
{
	FILETIME last_write_time = w32_get_last_write_time(w32_app_dll_path);
	if (CompareFileTime(&last_write_time, &code->last_write_time))
	{
		// TODO(fakhri): wait for all worker threads to finish their work before active reloading
		w32_code_unload(code);
		w32_load_app_code(code);
		code->vtable.mplayer_hotload(mplayer, input, &w32_vtable);
	}
}


internal void
w32_process_pending_messages(Memory_Arena *frame_arena, Mplayer_Input *input,  V2_I32 window_dim, Range2_I32 draw_region, HWND window)
{
	MSG msg;
	for (;PeekMessage(&msg, 0, 0, 0, PM_REMOVE);)
	{
		Mplayer_Input_Event *event = 0;
		switch (msg.message)
		{
			case WM_DESTROY: 
			case WM_CLOSE:
			{
				global_request_quit = true;
			} break;
			
			
			case WM_SYSKEYDOWN: // fallthrough;
			case WM_SYSKEYUP: // fallthrough;
			case WM_KEYDOWN: // fallthrough;
			case WM_KEYUP:
			{
				WPARAM vk_code = msg.wParam;
				
				b32 alt_key_down = (msg.lParam & (1 << 29)) != 0;
				b32 was_down = (msg.lParam & (1 << 30)) != 0;
				b32 is_down = (msg.lParam & (1 << 31)) == 0;
				
				event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
				
				event->kind = Event_Kind_Keyboard_Key;
				event->key = w32_resolve_vk_code(vk_code);
				event->is_down = b32(is_down);
				
				input->keyboard_buttons[event->key].is_down = event->is_down;
			} break;
			
			
			case WM_CHAR: // fallthrough;
			case WM_SYSCHAR:
			{
				u32 char_input = u32(msg.wParam);
				
				if ((char_input >= 32 && char_input != 127) || char_input == '\t' || char_input == '\n')
				{
					event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
					
					event->kind = Event_Kind_Text;
					event->text_character = char_input;
				}
			} break;
			
			case WM_MBUTTONDOWN: // fallthrough;
			case WM_MBUTTONUP:
			{
				b32 is_down = b32(msg.message == WM_MBUTTONDOWN);
				event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
				event->kind = Event_Kind_Mouse_Key;
				event->key = Mouse_Middle;
				event->is_down = is_down;
				
				input->mouse_buttons[event->key].is_down = event->is_down;
			} break;
			
			case WM_LBUTTONDOWN: // fallthrough;
			case WM_LBUTTONUP:
			{
				b32 is_down = b32(msg.message == WM_LBUTTONDOWN);
				event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
				event->kind = Event_Kind_Mouse_Key;
				event->key = Mouse_Left;
				event->is_down = is_down;
				
				input->mouse_buttons[event->key].is_down = event->is_down;
			} break;
			
			case WM_RBUTTONDOWN: // fallthrough;
			case WM_RBUTTONUP:
			{
				b32 is_down = b32(msg.message == WM_RBUTTONDOWN);
				event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
				event->kind = Event_Kind_Mouse_Key;
				event->key = Mouse_Right;
				event->is_down = is_down;
				
				input->mouse_buttons[event->key].is_down = event->is_down;
			} break;
			
			case WM_MOUSEWHEEL:
			{
				i16 wheel_delta = i16(HIWORD(u32(msg.wParam)));
				event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
				event->kind = Event_Kind_Mouse_Wheel;
				event->scroll.x = 0;
				event->scroll.y = f32(wheel_delta) / WHEEL_DELTA;
			} break;
			
			case WM_MOUSEMOVE:
			{
				f32 mouse_x = f32(GET_X_LPARAM(msg.lParam)); 
				f32 mouse_y = f32(GET_Y_LPARAM(msg.lParam)); 
				mouse_y = f32(window_dim.y - 1) - mouse_y;
				
				input->mouse_clip_pos.x = map_into_range_no(f32(draw_region.min_x), mouse_x, f32(draw_region.max_x));
				input->mouse_clip_pos.y = map_into_range_no(f32(draw_region.min_y), mouse_y, f32(draw_region.max_y));
				
				event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
				event->kind = Event_Kind_Mouse_Move;
				event->pos.x = mouse_x;
				event->pos.y = mouse_y;
			} break;
			
			case WM_XBUTTONDOWN: // fallthrough;
			case WM_XBUTTONUP:
			{
				b32 is_down = b32(msg.message == WM_XBUTTONDOWN);
				switch(HIWORD(msg.wParam))
				{
					case XBUTTON1:
					{
						event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
						event->key = Mouse_M4;
					} break;
					case XBUTTON2:
					{
						event = m_arena_push_struct_z(frame_arena, Mplayer_Input_Event);
						event->key = Mouse_M5;
					} break;
				}
				
				if (event)
				{
					event->kind = Event_Kind_Mouse_Key;
					event->is_down = is_down;
					input->mouse_buttons[event->key].is_down = is_down;
				}
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
			
			DLLPushBack(input->first_event, input->last_event, event);
		}
		
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

internal void
w32_update_audio()
{
	u32 max_ms_lag = 100;
	u32 max_lag_sample_count = max_ms_lag * g_w32_sound_output.config.sample_rate / 1000;
	
	f32 audio_dt = w32_get_seconds_elapsed(&g_w32_audio_timer);
	w32_update_timer(&g_w32_audio_timer);
	
	f32 request_duration = 1.5f * audio_dt;
	u32 needed_audio_samples = (u32)round_f32_i32(request_duration * g_w32_sound_output.config.sample_rate);
	
	// See how much buffer space is available.
	u32 pending_frame_count = 0;
	g_w32_sound_output.audio_client->GetCurrentPadding(&pending_frame_count);
	if (pending_frame_count < max_lag_sample_count)
	{
		u32 available_frames_count = g_w32_sound_output.buffer_frame_count - pending_frame_count;
		u32 frames_to_write = MIN(needed_audio_samples, available_frames_count);
		
		if (frames_to_write)
		{
			u32 flags = 0;
			u8 *data;
			g_w32_sound_output.render_client->GetBuffer(frames_to_write, &data);
			
			memory_zero(data, frames_to_write * g_w32_sound_output.config.channels_count * sizeof(f32));
			w32_app_code.vtable.mplayer_get_audio_samples(g_w32_sound_output.config, data, frames_to_write);
			
			g_w32_sound_output.render_client->ReleaseBuffer(frames_to_write, flags);
		}
	}
}

internal DWORD WINAPI
w32_audio_thread(void *unused)
{
	w32_update_timer(&g_w32_audio_timer);
	for(;;)
	{
		Sleep(50);
		DWORD wait_result = WaitForSingleObject(g_audio_mutex, INFINITE);
		if (wait_result == WAIT_OBJECT_0)
		{
			w32_update_audio();
			ReleaseMutex(g_audio_mutex);
		}
	}
}

internal void
w32_update()
{
	m_arena_free_all(&g_w32_frame_arena);
	
	g_w32_input.frame_dt = w32_get_seconds_elapsed(&g_w32_update_timer);
	g_w32_input.time += g_w32_input.frame_dt;
	
	// NOTE(fakhri): wait until we get user message or timer run-out 
	if (g_w32_input.next_animation_timer_request > 0)
	{
		f32 wait_request_time = g_w32_input.next_animation_timer_request;
		DWORD wait_requst_time_ms = (DWORD)(wait_request_time * 1000);
		MsgWaitForMultipleObjects(1, &g_shit_event, FALSE, wait_requst_time_ms, QS_ALLINPUT|QS_ALLPOSTMESSAGE|QS_POSTMESSAGE);
	}
	else if (g_w32_input.next_animation_timer_request < 0)
	{
		MsgWaitForMultipleObjects(1, &g_shit_event, FALSE, INFINITE, QS_ALLINPUT|QS_ALLPOSTMESSAGE|QS_POSTMESSAGE);
	}
	
	w32_update_timer(&g_w32_update_timer);
	
	w32_update_code(g_w32_mplayer, &g_w32_input, &w32_app_code);
	
	for (u32 i = 0; i < array_count(g_w32_input.keyboard_buttons); i += 1)
	{
		g_w32_input.keyboard_buttons[i].was_down = g_w32_input.keyboard_buttons[i].is_down;
	}
	for (u32 i = 0; i < array_count(g_w32_input.mouse_buttons); i += 1)
	{
		g_w32_input.mouse_buttons[i].was_down = g_w32_input.mouse_buttons[i].is_down;
		
	}
	
	V2_I32 window_dim;
	{
		RECT client_rect;
		GetClientRect(g_w32_window, &client_rect);
		window_dim.x = client_rect.right - client_rect.left;
		window_dim.y = client_rect.bottom - client_rect.top;
	}
	
	V2_I32 draw_dim = window_dim;
	Range2_I32 draw_region = compute_draw_region_aspect_ratio_fit(draw_dim, window_dim);
	
	g_w32_input.first_event = 0;
	g_w32_input.last_event = 0;
	
	w32_process_pending_messages(&g_w32_frame_arena, &g_w32_input, window_dim, draw_region, g_w32_window);
	
	w32_gl_render_begin(g_w32_renderer, window_dim, draw_dim, draw_region);
	
	for (;;)
	{
		DWORD wait_result = WaitForSingleObject(g_audio_mutex, INFINITE);
		if (wait_result == WAIT_OBJECT_0)
		{
			w32_app_code.vtable.mplayer_update_and_render();
			ReleaseMutex(g_audio_mutex);
			break;
		}
	}
	
	w32_gl_render_end(g_w32_renderer);
}

internal DWORD WINAPI 
w32_main_thread(void *unused)
{
	HINSTANCE hInstance = GetModuleHandleA(0);
	init_thread_context();
	g_w32_window = FindWindowA(W32_CLASS_NAME, W32_WINDOW_NAME);
	
	if (g_w32_window)
	{
		HDC wdc = GetDC(g_w32_window);
		
		g_w32_renderer = w32_gl_make_renderer(hInstance, wdc);
		
		assert(w32_load_app_code(&w32_app_code));
		g_w32_mplayer = w32_app_code.vtable.mplayer_initialize(&g_w32_frame_arena, (Render_Context *)g_w32_renderer, &g_w32_input, &w32_vtable);
		
		u32 sample_rate    = 96000;
		u16 channels_count = 2;
		g_w32_sound_output = w32_init_wasapi(sample_rate, channels_count);
		assert(g_w32_sound_output.initialized);
		
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
		
		
		w32_update_timer(&g_w32_update_timer);
		w32_update_timer(&g_w32_audio_timer);
		
		ShowWindow(g_w32_window, SW_SHOWNORMAL);
		UpdateWindow(g_w32_window);
		
		g_w32_input.target_frame_dt = 1.0f / refresh_rate;
		
		g_audio_mutex = CreateMutex(0, 0, 0);
		
		HANDLE audio_thread_handle = CreateThread(0, 0, w32_audio_thread, 0, 0, 0);
		CloseHandle(audio_thread_handle);
		
		b32 running = true;
		for (;running;)
		{
			w32_update();
			running = !(global_request_quit);
		}
	}
	
	ExitProcess(0);
}


internal void
w32_init_os_vtable()
{
	w32_vtable = {
		.fix_path_slashes                    = w32_fix_path_slashes,
		.make_folder_if_missing              = w32_make_folder_if_missing,
		.set_cursor                          = w32_set_cursor,
		.read_directory                      = w32_read_directory,
		.get_current_directory               = w32_get_current_directory,
		.load_entire_file                    = w32_load_entire_file,
		.open_file                           = w32_open_file,
		.close_file                          = w32_close_file,
		.read_block                          = w32_read_whole_block,
		.write_block                         = w32_write_whole_block,
		.push_work                           = w32_push_work_sp,
		.do_next_work                        = w32_do_next_work,
		.alloc                               = w32_allocate_memory,
		.dealloc                             = w32_deallocate_memory,
		.atomic_increment64                  = w32_atomic_increment64,
		.atomic_decrement64                  = w32_atomic_decrement64,
		.atomic_compare_and_exchange64       = w32_atomic_compare_and_exchange64,
		.atomic_compare_and_exchange_pointer = w32_atomic_compare_and_exchange_pointer,
	};
}

global Worker_Thread_Context g_worker_threads[64];

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	w32_init_os_vtable();
	platform = &w32_vtable;
	
	init_thread_context();
	timeBeginPeriod(1);
	QueryPerformanceFrequency(&w32_timer_freq);
	
	SetProcessDPIAware();
	
	// NOTE(fakhri): Calculate executable name and path to DLL
	{
		DWORD size_of_executable_path = GetModuleFileNameA(0, w32_exe_path, sizeof(w32_exe_path));
		
		// NOTE(fakhri): Calculate executable directory
		{
			memory_copy(w32_exe_dir, w32_exe_path, size_of_executable_path);
			char *one_past_last_slash = w32_exe_dir;
			for(i32 i = 0; w32_exe_dir[i]; ++i)
			{
				if(w32_exe_dir[i] == '\\')
				{
					one_past_last_slash = w32_exe_dir + i + 1;
				}
			}
			*one_past_last_slash = 0;
		}
		
		// NOTE(fakhri): Create DLL filenames
		{
			wsprintf(w32_app_dll_path, "%s%s.dll", w32_exe_dir, W32_EXE_NAME);
			wsprintf(w32_temp_app_dll_path, "%stemp_%s.dll", w32_exe_dir, W32_EXE_NAME);
		}
	}
	
	
	DWORD cpu_count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	u32 workers_count = cpu_count - 2;
	g_w32_queue.semaphore = CreateSemaphoreA(0, 0, workers_count, 0);
	
	// NOTE(fakhri): worker threads
	for (u32 thread_index = 0; thread_index < workers_count; thread_index += 1)
	{
		Worker_Thread_Context *worker_thread_ctx = g_worker_threads + thread_index;
		worker_thread_ctx->id = thread_index + 1;
		
		HANDLE thread_handle = CreateThread(0, 0, w32_worker_thread_main, (void*)worker_thread_ctx, 0, 0);
		CloseHandle(thread_handle);
	}
	
	g_shit_event = CreateEvent(0, 0, 0, 0);
	
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
		g_w32_window = CreateWindowA(W32_CLASS_NAME,
			W32_WINDOW_NAME,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			W32_WINDOW_W, W32_WINDOW_H,
			0, 0, hInstance, 0);
		if (g_w32_window)
		{
			
			HANDLE thread_handle = CreateThread(0,0,w32_main_thread,0,0,&g_w32_main_thread_id);
			CloseHandle(thread_handle);
			
			for (;;)
			{
				b32 request_close = false;
				MSG msg;
				GetMessageW(&msg, 0, 0, 0);
				TranslateMessage(&msg);
				PostThreadMessageW(g_w32_main_thread_id, msg.message, msg.wParam, msg.lParam);
				
				switch(msg.message)
				{
					case WM_QUIT:       // fallthrough;
					case WM_CLOSE:       // fallthrough;
					case WM_SYSKEYDOWN:  // fallthrough;
					case WM_SYSKEYUP:    // fallthrough;
					case WM_KEYDOWN:     // fallthrough;
					case WM_KEYUP:       // fallthrough;
					case WM_CHAR:        // fallthrough;
					case WM_SYSCHAR:     // fallthrough;
					case WM_MBUTTONDOWN: // fallthrough;
					case WM_MBUTTONUP:   // fallthrough;
					case WM_LBUTTONDOWN: // fallthrough;
					case WM_LBUTTONUP:   // fallthrough;
					case WM_RBUTTONDOWN: // fallthrough;
					case WM_RBUTTONUP:   // fallthrough;
					case WM_MOUSEWHEEL:  // fallthrough;
					case WM_MOUSEMOVE:   // fallthrough;
					case WM_XBUTTONDOWN: // fallthrough;
					case WM_XBUTTONUP:
					{
						PostThreadMessageW(g_w32_main_thread_id, msg.message, msg.wParam, msg.lParam);
					} break;
					
					default:
					{
						DispatchMessage(&msg);
					} break;
				}
			}
		}
	}
	
	return 0;
}