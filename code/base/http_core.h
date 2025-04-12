/* date = April 12th 2025 6:54 pm */

#ifndef HTTP_CORE_H
#define HTTP_CORE_H

#define CRLF "\r\n"
enum Http_Version
{
	HTTP_VERSION_UNSUPPORTED,
	HTTP_1_0,
	HTTP_1_1,
};

enum Http_Method
{
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
};

struct Header_Field
{
	Header_Field *next_list;
	Header_Field *next_hash;
	u32 hash;
	String8 name;
	String8_List values;
};

#define FIELDS_TABLE_CAPACITY 4096
struct Http_Header_Fields
{
	Header_Field *fields_tables[FIELDS_TABLE_CAPACITY];
	
	// NOTE(fakhri): list to iterate over all the fields easily
	Header_Field *fields_first;
	Header_Field *fields_last;
};

struct Http_Request
{
	Http_Method method;
	String8 uri;
	Http_Header_Fields header_fields;
	String8 body;
};

struct Http_Response
{
	String8 status_line;
	Http_Version http_version;
	u32 status_code;
	
	Http_Header_Fields header_fields;
	
	String8_List body;
};

internal void http_header_add_field(Memory_Arena *arena, Http_Header_Fields *header_fields, String8 name, String8 value);
internal Header_Field *http_header_find_field(Http_Header_Fields *header_fields, String8 name);
internal b32 http_header_match_field_value_case_insensitive(Http_Header_Fields *header_fields, String8 field_name, String8 match_value);
internal b32 http_parse_header_fields(Memory_Arena *arena, String8_List field_lines, Http_Header_Fields *header_fields);
internal void debug_log_http_header_fields(Http_Header_Fields *header_fields);
internal const char *http_method_name(Http_Method method);

#endif //HTTP_CORE_H
