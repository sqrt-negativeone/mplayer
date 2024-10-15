

struct String8
{
	union
	{
		u8 *str;
		char *cstr;
	};
	u64 len;
};


#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

#define str8_lit(str) str8((u8*)(str), sizeof(str) - 1)

internal String8
str8(u8 *str, u64 len)
{
	String8 result;
	result.str = str;
	result.len = len;
	return result;
}

internal String8
str8_from_cstr(char *str)
{
	String8 result = str8((u8*)str, strlen(str));
	return result;
}


internal String8
str8_fv(Memory_Arena *arena, const char *fmt, va_list args)
{
  String8 result = ZERO_STRUCT;
  va_list args2;
  va_copy(args2, args);
  u64 needed_bytes = stbsp_vsnprintf(0, 0, fmt, args) + 1;
  result.str = m_arena_push_array(arena, u8, needed_bytes);
  result.len = needed_bytes - 1;
  stbsp_vsnprintf((char*)result.str, (int)needed_bytes, fmt, args2);
  return result;
}

internal String8
str8_f(Memory_Arena *arena, const char *fmt, ...)
{
  String8 result = ZERO_STRUCT;
  va_list args;
  va_start(args, fmt);
  result = str8_fv(arena, fmt, args);
  va_end(args);
  return result;
}
