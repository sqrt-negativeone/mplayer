
internal void
http_header_add_field(Memory_Arena *arena, Http_Header_Fields *header_fields, String8 name, String8 value)
{
	u32 hash = str8_hash32_case_insensitive(name);
	u32 index = hash & (FIELDS_TABLE_CAPACITY - 1);
	
	b32 found = 0;
	Header_Field *slot = header_fields->fields_tables[index];
	for (;slot;
			 slot = slot->next_hash)
	{
		if (slot->hash == hash &&
				str8_match(slot->name, name, MatchFlag_CaseInsensitive))
		{
			found = 1;
			break;
		}
	}
	
	if (!slot)
	{
		slot = m_arena_push_struct_z(arena, Header_Field);
		slot->next_hash = header_fields->fields_tables[index];
		header_fields->fields_tables[index] = slot;
		
		if (!header_fields->fields_last)
		{
			header_fields->fields_last = header_fields->fields_first = slot;
		}
		else
		{
			header_fields->fields_last->next_list = slot;
			header_fields->fields_last = slot;
		}
	}
	
	assert(slot);
	slot->hash = hash;
	slot->name = name;
	str8_list_push(arena, &slot->values, value);
}

internal Header_Field *
http_header_find_field(Http_Header_Fields *header_fields, String8 name)
{
	u32 hash = str8_hash32_case_insensitive(name);
	u32 index = hash & (FIELDS_TABLE_CAPACITY - 1);
	
	Header_Field *slot = header_fields->fields_tables[index];
	for (;slot;
			 slot = slot->next_hash)
	{
		if (slot->hash == hash &&
				str8_match(slot->name, name, MatchFlag_CaseInsensitive))
		{
			break;
		}
	}
	
	return slot;
}

internal b32
http_header_match_field_value_case_insensitive(Http_Header_Fields *header_fields, String8 field_name, String8 match_value)
{
	b32 result = 0;
	Header_Field *field = http_header_find_field(header_fields, field_name);
	if (field)
	{
		assert(field->values.first);
		String8 field_value = field->values.first->str;
		if (str8_find(field_value, match_value, 0, MatchFlag_CaseInsensitive) < field_value.len)
		{
			result = 1;
		}
	}
	return result;
}

internal b32
char_is_ows(u8 c)
{
	b32 result = (c == ' ') || (c == '\t');
	return result;
}

internal String8
str8_skip_ows(String8 str)
{
	u32 ows_count = 0;
	while (ows_count < str.len&& 
				 char_is_ows(str.str[ows_count]))
	{
		ows_count += 1;
	}
	
	String8 result = str8_skip_first(str, ows_count);
	return result;
}

internal b32
http_parse_header_fields(Memory_Arena *arena, String8_List field_lines, Http_Header_Fields *header_fields)
{
	b32 success = 1;
	for (String8_Node *node = field_lines.first;
			 node;
			 node = node->next)
	{
		String8 field_line = node->str;
		
		if (!char_is_ows(field_line.str[0]))
		{
			String8 field_name = str8_chop_first_occurence(field_line, str8_lit(":"));
			if (char_is_ows(field_name.str[field_line.len-1]))
			{
				// NOTE(fakhri): no optional whitespaces are allowed between field name and :
				success = 0;
				break;
			}
			
			field_line = str8_skip_first(field_line, field_name.len + 1);
			field_line = str8_skip_ows(field_line);
			
			String8 field_value = str8_chop_first_occurence(field_line, str8_lit(CRLF));
			
			// TODO(fakhri): push field value to the header without parsing it for now
			http_header_add_field(arena, header_fields, field_name, field_value);
		}
		else
		{
			// TODO(fakhri): field names should not start with optional whitespaces. ignore the line?
			success = 0;
			break;
		}
	}
	
	return success;
}

internal void
debug_log_http_header_fields(Http_Header_Fields *header_fields)
{
	Memory_Checkpoint scratch = begin_scratch(0, 0);
	
	String8_List stream = ZERO_STRUCT;
	for (Header_Field *field = header_fields->fields_first;
			 field;
			 field = field->next_list)
	{
		String_Join join_params = ZERO_STRUCT;
		join_params.sep = str8_lit (", ");
		String8 field_value = str8_list_join(scratch.arena, field->values, &join_params);
		
		str8_list_push_f(scratch.arena, &stream, "%.*s: %.*s" CRLF, STR8_EXPAND(field->name), STR8_EXPAND(field_value));
	}
	String8 log = str8_list_join(scratch.arena, stream, 0);
	Log("header fields:\n%.*s", STR8_EXPAND(log));
	end_scratch(scratch);
}

internal const char *
http_method_name(Http_Method method)
{
	const char *result = 0;
	switch (method)
	{
		case HTTP_METHOD_GET:  result = "GET";  break;
		case HTTP_METHOD_POST: result = "POST"; break;
	}
	return result;
}
