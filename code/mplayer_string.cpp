
struct String8
{
	u8 *str;
	u64 len;
};

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
