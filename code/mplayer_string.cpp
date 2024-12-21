
struct String8
{
	union
	{
		u8 *str;
		char *cstr;
	};
	u64 len;
};

struct String16
{
	u16 *str;
	u64 len;
};

struct Decoded_Codepoint
{
  u32 codepoint;
  u32 advance;
};

struct String8_Node
{
	String8_Node *next;
	String8 str;
};
struct String8_List
{
	String8_Node *first;
	String8_Node *last;
	u64 count;
	u64 total_len;
};
struct String_Join
{
  String8 pre;
  String8 sep;
  String8 post;
};



internal b32
char_is_alpha_upper(u8 c)
{
  return c >= 'A' && c <= 'Z';
}

internal b32
char_is_alpha_lower(u8 c)
{
  return c >= 'a' && c <= 'z';
}

internal b32
char_is_alpha(u8 c)
{
  return char_is_alpha_upper(c) || char_is_alpha_lower(c);
}

internal b32
char_is_digit(u8 c)
{
  return (c >= '0' && c <= '9');
}

internal b32
char_is_symbol(u8 c)
{
  return (c == '~' || c == '!'  || c == '$' || c == '%' || c == '^' ||
		c == '&' || c == '*'  || c == '-' || c == '=' || c == '+' ||
		c == '<' || c == '.'  || c == '>' || c == '/' || c == '?' ||
		c == '|' || c == '\\' || c == '{' || c == '}' || c == '(' ||
		c == ')' || c == '\\' || c == '[' || c == ']' || c == '#' ||
		c == ',' || c == ';'  || c == ':' || c == '@');
}

internal b32
char_is_space(u8 c)
{
  return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
}

internal u8
char_to_upper(u8 c)
{
  return (c >= 'a' && c <= 'z') ? ('A' + (c - 'a')) : c;
}

internal u8
char_to_lower(u8 c)
{
  return (c >= 'A' && c <= 'Z') ? ('a' + (c - 'A')) : c;
}

internal u8
char_to_forward_slash(u8 c)
{
  return (c == '\\' ? '/' : c);
}


#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

#define str8_lit(str) str8((u8*)(str), sizeof(str) - 1)
#define STR8_EXPAND(s) (int)(s).len, (s).str

internal String8
str8(u8 *str, u64 len)
{
	String8 result;
	result.str = str;
	result.len = len;
	return result;
}

internal String16
str16(u16 *str, u64 len)
{
	String16 result;
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

internal String8
str8_clone(Memory_Arena *arena, String8 str)
{
	String8 result;
	result.str = m_arena_push_array(arena, u8, str.len);
	result.len = str.len;
	memory_copy(result.str, str.str, str.len);
	return result;
}

global u8 utf8_class[32] =
{
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
};

#define bitmask1 0x01
#define bitmask2 0x03
#define bitmask3 0x07
#define bitmask4 0x0F
#define bitmask5 0x1F
#define bitmask6 0x3F
#define bitmask7 0x7F
#define bitmask8 0xFF
#define bitmask9  0x01FF
#define bitmask10 0x03FF

internal Decoded_Codepoint
decode_codepoint_from_utf8(u8 *str, u64 max)
{
  Decoded_Codepoint result = {~((u32)0), 1};
  u8 byte = str[0];
  u8 byte_class = utf8_class[byte >> 3];
  switch (byte_class)
  {
    case 1:
    {
      result.codepoint = byte;
    }break;
    
    case 2:
    {
      if (2 <= max)
      {
        u8 cont_byte = str[1];
        if (utf8_class[cont_byte >> 3] == 0)
        {
          result.codepoint = (byte & bitmask5) << 6;
          result.codepoint |=  (cont_byte & bitmask6);
          result.advance = 2;
        }
      }
    }break;
    
    case 3:
    {
      if (3 <= max)
      {
        u8 cont_byte[2] = {str[1], str[2]};
        if (utf8_class[cont_byte[0] >> 3] == 0 &&
					utf8_class[cont_byte[1] >> 3] == 0)
        {
          result.codepoint = (byte & bitmask4) << 12;
          result.codepoint |= ((cont_byte[0] & bitmask6) << 6);
          result.codepoint |=  (cont_byte[1] & bitmask6);
          result.advance = 3;
        }
      }
    }break;
    
    case 4:
    {
      if (4 <= max)
      {
        u8 cont_byte[3] = {str[1], str[2], str[3]};
        if (utf8_class[cont_byte[0] >> 3] == 0 &&
					utf8_class[cont_byte[1] >> 3] == 0 &&
					utf8_class[cont_byte[2] >> 3] == 0)
        {
          result.codepoint = (byte & bitmask3) << 18;
          result.codepoint |= ((cont_byte[0] & bitmask6) << 12);
          result.codepoint |= ((cont_byte[1] & bitmask6) <<  6);
          result.codepoint |=  (cont_byte[2] & bitmask6);
          result.advance = 4;
        }
      }
    }break;
  }
  
  return result;
}

internal Decoded_Codepoint
decode_codepoint_from_utf16(u16 *out, u64 max)
{
  Decoded_Codepoint result = {~((u32)0), 1};
  result.codepoint = out[0];
  result.advance = 1;
  if (1 < max && 0xD800 <= out[0] && out[0] < 0xDC00 && 0xDC00 <= out[1] && out[1] < 0xE000)
  {
    result.codepoint = ((out[0] - 0xD800) << 10) | (out[1] - 0xDC00);
    result.advance = 2;
  }
  return result;
}

internal u32             
utf8_from_codepoint(u8 *out, u32 codepoint)
{
	#define bit8 0x80
		u32 advance = 0;
  if (codepoint <= 0x7F)
  {
    out[0] = (u8)codepoint;
    advance = 1;
  }
  else if (codepoint <= 0x7FF)
  {
    out[0] = (bitmask2 << 6) | ((codepoint >> 6) & bitmask5);
    out[1] = bit8 | (codepoint & bitmask6);
    advance = 2;
  }
  else if (codepoint <= 0xFFFF)
  {
    out[0] = (bitmask3 << 5) | ((codepoint >> 12) & bitmask4);
    out[1] = bit8 | ((codepoint >> 6) & bitmask6);
    out[2] = bit8 | ( codepoint       & bitmask6);
    advance = 3;
  }
  else if (codepoint <= 0x10FFFF)
  {
    out[0] = (bitmask4 << 3) | ((codepoint >> 18) & bitmask3);
    out[1] = bit8 | ((codepoint >> 12) & bitmask6);
    out[2] = bit8 | ((codepoint >>  6) & bitmask6);
    out[3] = bit8 | ( codepoint        & bitmask6);
    advance = 4;
  }
  else
  {
    out[0] = '?';
    advance = 1;
  }
  return advance;
}
internal u32             
utf16_from_codepoint(u16 *out, u32 codepoint)
{
  u32 advance = 1;
  if (codepoint == ~((u32)0))
  {
    out[0] = (u16)'?';
  }
  else if (codepoint < 0x10000)
  {
    out[0] = (u16)codepoint;
  }
  else
  {
    u64 v = codepoint - 0x10000;
    out[0] = (u16)(0xD800 + (v >> 10));
    out[1] = (u16)(0xDC00 + (v & bitmask10));
    advance = 2;
  }
  return advance;
}

internal String16
str16_from_8(Memory_Arena *arena, String8 in)
{
  u64 cap = 2 * in.len;
  u16 *str = m_arena_push_array(arena, u16, cap + 1);
  u8 *ptr = in.str;
  u8 *opl = ptr + in.len;
  umem size = 0;
  Decoded_Codepoint consume;
  for (;ptr < opl;)
  {
    consume = decode_codepoint_from_utf8(ptr, opl - ptr);
    ptr += consume.advance;
    size += utf16_from_codepoint(str + size, consume.codepoint);
  }
  str[size] = 0;
	
  String16 result = {str, size};
  return result;
}

internal String8
str8_from_16(Memory_Arena *arena, String16 in)
{
  u64 cap = 3 * in.len;
  u8 *str = m_arena_push_array(arena, u8, cap + 1);
  u16 *ptr = in.str;
  u16 *opl = ptr + in.len;
  u64 size = 0;
  Decoded_Codepoint consume;
  for (;ptr < opl;)
  {
    consume = decode_codepoint_from_utf16(ptr, opl - ptr);
    ptr += consume.advance;
    size += utf8_from_codepoint(str + size, consume.codepoint);
  }
  str[size] = 0;
  return str8(str, size);
}


//- NOTE(fakhri): substrings

internal String8
substr8(String8 str, u64 start_index, u64 opl_index)
{
  if(opl_index > str.len)
  {
    opl_index = str.len;
  }
  if(start_index > str.len)
  {
		start_index = str.len;
  }
  if(start_index > opl_index)
  {
    SWAP(u64, start_index, opl_index);
  }
  str.len = opl_index - start_index;
  str.str += start_index;
  return str;
}

internal String8
str8_skip_first(String8 str, u64 min)
{
  return substr8(str, min, str.len);
}

internal String8
str8_chop_last(String8 str, u64 nmax)
{
  return substr8(str, 0, str.len-nmax);
}

internal String8
prefix8(String8 str, u64 size)
{
  return substr8(str, 0, size);
}

internal String8
suffix8(String8 str, u64 size)
{
  return substr8(str, str.len-size, str.len);
}

typedef u32 Match_Flags;
enum
{
  MatchFlag_CaseInsensitive  = (1<<0),
  MatchFlag_RightSideSloppy  = (1<<1),
  MatchFlag_SlashInsensitive = (1<<2),
  MatchFlag_FindLast         = (1<<3),
  MatchFlag_SkipFirst        = (1<<4),
};


//- NOTE(fakhri): Matching
internal b32
str8_match(String8 a, String8 b, Match_Flags flags)
{
  b32 result = 0;
  
  if ((flags & MatchFlag_RightSideSloppy) && b.len > a.len)
  {
    b.len = a.len;
  }
  
  if(a.len == b.len)
  {
    result = 1;
    for(u64 i = 0; i < a.len; i += 1)
    {
      b32 match = (a.str[i] == b.str[i]);
      if(flags & MatchFlag_CaseInsensitive)
      {
        match |= (char_to_lower(a.str[i]) == char_to_lower(b.str[i]));
      }
      if(flags & MatchFlag_SlashInsensitive)
      {
        match |= (char_to_forward_slash(a.str[i]) == char_to_forward_slash(b.str[i]));
      }
      if(match == 0)
      {
        result = 0;
        break;
      }
    }
  }
  return result;
}

internal b32
str8_is_subsequence(String8 a, String8 b, Match_Flags flags)
{
	b32 result = 0;
	
	if (a.len >= b.len)
	{
		u32 j = 0;
		for (u64 i = 0; i < a.len && j < b.len; i += 1)
		{
			b32 match = (a.str[i] == b.str[j]);
			
			if(flags & MatchFlag_CaseInsensitive)
      {
        match |= (char_to_lower(a.str[i]) == char_to_lower(b.str[j]));
      }
			if(flags & MatchFlag_SlashInsensitive)
      {
        match |= (char_to_forward_slash(a.str[i]) == char_to_forward_slash(b.str[j]));
      }
			
			if (match)
			{
				j += 1;
			}
		}
		
		if (j == b.len)
		{
			result = 1;
		}
	}
	
	return result;
}

internal u64
find_substr8(String8 haystack, String8 needle, u64 start_pt, Match_Flags flags)
{
  u64 found_idx = haystack.len;
  b32 is_first = true;
  for(u64 i = start_pt; i < haystack.len; i += 1)
  {
    if(i + needle.len <= haystack.len)
    {
      String8 substr = substr8(haystack, i, i+needle.len);
      if(str8_match(substr, needle, flags))
      {
        found_idx = i;
        if ((flags & MatchFlag_SkipFirst) && is_first)
        {
          is_first = false;
          continue;
        }
        if(!(flags & MatchFlag_FindLast))
        {
          break;
        }
      }
    }
  }
  return found_idx;
}

internal b32
str8_starts_with(String8 a, String8 prefix, Match_Flags flags)
{
  b32 result = str8_match(prefix, a, flags | MatchFlag_RightSideSloppy);
  return result;
}

internal b32
str8_ends_with(String8 a, String8 b, Match_Flags flags)
{
  b32 result = 1;
  if (a.len >= b.len)
  {
    for(u32 i = 0;
			i < b.len;
			i += 1)
    {
      b32 match = (a.str[a.len - 1 - i] == b.str[b.len - 1 - i]);
      if(flags & MatchFlag_CaseInsensitive)
      {
        match |= (char_to_lower(a.str[i]) == char_to_lower(b.str[i]));
      }
      if(flags & MatchFlag_SlashInsensitive)
      {
        match |= (char_to_forward_slash(a.str[i]) == char_to_forward_slash(b.str[i]));
      }
      if(!match)
      {
        result = 0;
        break;
      }
    }
  }
  return result;
}

internal u64
string_find_first_non_whitespace(String8 str){
  u64 i = 0;
  for (;i < str.len && char_is_space(str.str[i]); i += 1);
  return(i);
}


internal void
str8_list_push_node(String8_List *list, String8_Node *node)
{
	QueuePush(list->first, list->last, node);
	list->count += 1;
	list->total_len += node->str.len;
}


internal void
str8_list_push(Memory_Arena *arena, String8_List *list, String8 str)
{
  String8_Node *n = m_arena_push_struct_z(arena, String8_Node);
  n->str = str;
  str8_list_push_node(list, n);
}

internal void
str8_list_push_fv(Memory_Arena *arena, String8_List *list, const char *fmt, va_list args)
{
  String8_Node *n = m_arena_push_struct_z(arena, String8_Node);
  n->str = str8_fv(arena, fmt, args);
  str8_list_push_node(list, n);
}

internal void
str8_list_push_f(Memory_Arena *arena, String8_List *list, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  str8_list_push_fv(arena, list, fmt, args);
  va_end(args);
}


internal void
str8_list_concat(String8_List *list, String8_List *to_push)
{
  if(to_push->first)
  {
    list->count += to_push->count;
    list->total_len += to_push->total_len;
    if(list->last == 0)
    {
      *list = *to_push;
    }
    else
    {
      list->last->next = to_push->first;
      list->last = to_push->last;
    }
  }
  memory_zero(to_push, sizeof(*to_push));
}

internal String8_List
str8_split(Memory_Arena *arena, String8 string, int split_count, String8 *splits)
{
  String8_List list = {0};
  
  u64 split_start = 0;
  for(u64 i = 0; i < string.len; i += 1)
  {
    b32 was_split = 0;
    for(int split_idx = 0; split_idx < split_count; split_idx += 1)
    {
      b32 match = 0;
      if(i + splits[split_idx].len <= string.len)
      {
        match = 1;
        for(u64 split_i = 0; split_i < splits[split_idx].len && i + split_i < string.len; split_i += 1)
        {
          if(splits[split_idx].str[split_i] != string.str[i + split_i])
          {
            match = 0;
            break;
          }
        }
      }
      if(match)
      {
        String8 split_string = str8(string.str + split_start, i - split_start);
        str8_list_push(arena, &list, split_string);
        split_start = i + splits[split_idx].len;
        i += splits[split_idx].len - 1;
        was_split = 1;
        break;
      }
    }
    
    if(was_split == 0 && i == string.len - 1)
    {
      String8 split_string = str8(string.str + split_start, i+1 - split_start);
      str8_list_push(arena, &list, split_string);
      break;
    }
  }
  
  return list;
}

internal String8
str8_list_join(Memory_Arena *arena, String8_List list, String_Join *optional_params)
{
  // NOTE(fakhri): setup join parameters
  String_Join join = ZERO_STRUCT;
  if(optional_params != 0)
  {
    memory_copy(&join, optional_params, sizeof(join));
  }
  
  // NOTE(fakhri): calculate size & allocate
  u64 sep_count = 0;
  if(list.count > 1)
  {
    sep_count = list.count - 1;
  }
  String8 result = ZERO_STRUCT;
  result.len = (list.total_len + join.pre.len +
    sep_count*join.sep.len + join.post.len);
  result.str = m_arena_push_array(arena, u8, result.len);
  
  // NOTE(fakhri): fill
  u8 *ptr = result.str;
  memory_copy(ptr, join.pre.str, join.pre.len);
  ptr += join.pre.len;
  for(String8_Node *node = list.first; node; node = node->next)
  {
    memory_copy(ptr, node->str.str, node->str.len);
    ptr += node->str.len;
    if (node != list.last){
      memory_copy(ptr, join.sep.str, join.sep.len);
      ptr += join.sep.len;
    }
  }
  memory_copy(ptr, join.pre.str, join.pre.len);
  ptr += join.pre.len;
  
  return result;
}



internal String8
str8_chop_last_slash(String8 string)
{
	u64 slash_pos = find_substr8(string, str8_lit("/"), 0,
		MatchFlag_SlashInsensitive|
		MatchFlag_FindLast);
	if(slash_pos < string.len)
	{
		string.len = slash_pos+1;
	}
	return string;
}

struct String8_UTF8_Iterator
{
	u8 *str;
	u64 max;
	Decoded_Codepoint utf8;
	u64 offset;
};

internal b32
str8_utf8_it_valid(String8_UTF8_Iterator *it)
{
	b32 result = 0;
	if (it->offset < it->max)
	{
		result = 1;
		it->utf8 = decode_codepoint_from_utf8(it->str + it->offset, it->max - it->offset);
	}
	return result;
}

internal void
str8_utf8_advance(String8_UTF8_Iterator *it)
{
	it->offset += it->utf8.advance;
}

internal String8_UTF8_Iterator
str8_utf8_iterator(String8 string)
{
	String8_UTF8_Iterator it = ZERO_STRUCT;
	it.str = string.str;
	it.max = string.len;
	
	return it;
}
