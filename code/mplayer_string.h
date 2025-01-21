/* date = January 19th 2025 4:21 pm */

#ifndef STRING_H
#define STRING_H

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

typedef u32 Match_Flags;
enum
{
  MatchFlag_CaseInsensitive  = (1<<0),
  MatchFlag_RightSideSloppy  = (1<<1),
  MatchFlag_SlashInsensitive = (1<<2),
  MatchFlag_FindLast         = (1<<3),
  MatchFlag_SkipFirst        = (1<<4),
};

struct String8_UTF8_Iterator
{
	u8 *str;
	u64 max;
	Decoded_Codepoint utf8;
	u64 offset;
};


#define str8_lit(str) str8((u8*)(str), sizeof(str) - 1)
#define STR8_EXPAND(s) (int)(s).len, (s).str

internal b32 char_is_alpha_upper(u8 c);
internal b32 char_is_alpha_lower(u8 c);
internal b32 char_is_alpha(u8 c);
internal b32 char_is_digit(u8 c);
internal b32 char_is_symbol(u8 c);
internal b32 char_is_space(u8 c);
internal u8 char_to_upper(u8 c);
internal u8 char_to_lower(u8 c);
internal u8 char_to_forward_slash(u8 c);

internal String8 str8(u8 *str, u64 len);
internal String16 str16(u16 *str, u64 len);
internal String8 str8_from_cstr(char *str);
internal String8 str8_fv(Memory_Arena *arena, const char *fmt, va_list args);
internal String8 str8_f(Memory_Arena *arena, const char *fmt, ...);
internal String8 str8_clone(Memory_Arena *arena, String8 str);

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

internal Decoded_Codepoint decode_codepoint_from_utf8(u8 *str, u64 max);
internal Decoded_Codepoint decode_codepoint_from_utf16(u16 *out, u64 max);
internal u32               utf8_from_codepoint(u8 *out, u32 codepoint);
internal String16          str16_from_8(Memory_Arena *arena, String8 in);
internal String8           str8_from_16(Memory_Arena *arena, String16 in);

//- NOTE(fakhri): substrings

internal String8 substr8(String8 str, u64 start_index, u64 opl_index);
internal String8 str8_skip_first(String8 str, u64 min);
internal String8 str8_chop_last(String8 str, u64 nmax);
internal String8 prefix8(String8 str, u64 size);
internal String8 suffix8(String8 str, u64 size);


//- NOTE(fakhri): Matching
internal b32 str8_match(String8 a, String8 b, Match_Flags flags);
internal b32 str8_is_subsequence(String8 a, String8 b, Match_Flags flags);
internal u64 find_substr8(String8 haystack, String8 needle, u64 start_pt, Match_Flags flags);
internal b32 str8_starts_with(String8 a, String8 prefix, Match_Flags flags);
internal b32 str8_ends_with(String8 a, String8 b, Match_Flags flags);
internal u64 string_find_first_non_whitespace(String8 str);
internal u64 string_find_first_whitespace(String8 str);
internal u64 string_find_first_characer(String8 str, u8 ch);
internal void str8_list_push_node(String8_List *list, String8_Node *node);
internal void str8_list_push(Memory_Arena *arena, String8_List *list, String8 str);
internal void str8_list_push_fv(Memory_Arena *arena, String8_List *list, const char *fmt, va_list args);
internal void str8_list_push_f(Memory_Arena *arena, String8_List *list, const char *fmt, ...);
internal void str8_list_concat(String8_List *list, String8_List *to_push);
internal String8_List str8_split(Memory_Arena *arena, String8 string, int split_count, String8 *splits);
internal String8 str8_list_join(Memory_Arena *arena, String8_List list, String_Join *optional_params);

internal String8 str8_chop_last_slash(String8 string);
internal String8 str8_chop_last_dot(String8 string);

internal b32 str8_utf8_it_valid(String8_UTF8_Iterator *it);
internal void str8_utf8_advance(String8_UTF8_Iterator *it);
internal String8_UTF8_Iterator str8_utf8_iterator(String8 string);

#endif //STRING_H
