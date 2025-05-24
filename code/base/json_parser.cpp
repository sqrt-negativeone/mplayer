// TODO(fakhri): lower memory usage
// TODO(fakhri): optimizations
// TODO(fakhri): error reporting instead of hard crashing?

enum Json_Parse_State
{
  STATE_PARSE_VALUE,
  STATE_PARSE_VALUE_FINISH_OBJ,
  STATE_PARSE_VALUE_FINISH_ARR,
  
  STATE_PARSE_ARR,
  STATE_PARSE_ARR_FINISH,
  
  STATE_PARSE_OBJ,
  STATE_PARSE_OBJ_FINISH,
  
  STATE_PARSE_PAIRS,
};

enum Json_Value_Kind
{
  JSON_VALUE_NULL,
  JSON_VALUE_STRING,
  JSON_VALUE_OBJECT,
  JSON_VALUE_ARRAY,
  JSON_VALUE_NUMBER_UINT,
  JSON_VALUE_NUMBER_INT,
  JSON_VALUE_NUMBER_FLOAT,
  JSON_VALUE_BOOL,
  
	JSON_VALUE_TYPE_COUNT,
};

static_assert(JSON_VALUE_TYPE_COUNT <= 8);

#define JSON_VALUE_KIND(value) (Json_Value_Kind)((value).kind_with_chunk_offset >> 13)
union Json_ID
{
  u32 compact;
  struct
  {
    u16 chunk_index;
    u16 chunk_offset;
  };
};
static_assert(sizeof(Json_ID) == sizeof(u32));

union Json_Value
{
	u32 compact;
	struct
	{
		union
		{
			u16 chunk_index;
			u16 bool_value;
		};
		// NOTE(fakhri): packed kind and chunk_offset, kind: 3bits, chunk_offset:13bits
		u16 kind_with_chunk_offset;
	};
	Json_ID pair_id;
};

internal b32
is_valid(Json_Value value)
{
	b32 result = (value.compact != 0);
	return result;
}

internal b32
is_valid(Json_ID id)
{
	b32 result = (id.compact != 0);
	return result;
}

internal Json_Value
make_json_value(Json_Value_Kind kind, Json_ID id)
{
  Json_Value json_value;
  json_value.chunk_index = id.chunk_index;
  json_value.kind_with_chunk_offset = u16((kind << 13) | (id.chunk_offset & 0x1FFF));
  return json_value;
}

internal Json_Value
make_json_bool(b16 b)
{
  Json_Value json_value;
  json_value.bool_value = b;
  json_value.kind_with_chunk_offset = JSON_VALUE_BOOL << 13;
  return json_value;
}

internal Json_ID
json_id_from_value(Json_Value json_value)
{
  Json_ID json_id;
  json_id.chunk_index = json_value.chunk_index;
  json_id.chunk_offset = (json_value.kind_with_chunk_offset & 0x1FFF);
  return json_id;
}

union Json_Number
{
	u64 val_int;
	f64 val_float;
};

struct Json_String
{
  u32 offset; 
  u32 size;
};

struct Json_Pair
{
  Json_ID  string_id;
  Json_Value value;
};

// TODO(fakhri): use sparse storage for container parts? this way we only need 4bytes to reference
// the next pointer and can store an extra value
// this will also make the container smaller by 50%

// NOTE(fakhri): the way we store json values make it
// so that a json object is the same as a json array
#define CONTAINER_PART_ELEMENTS_COUNT 13
struct Json_Container_Part
{
	Json_Value values[CONTAINER_PART_ELEMENTS_COUNT];   // for arrays
  u32 count;
  Json_Container_Part *next;
};
static_assert(sizeof(Json_Container_Part) == 64);

struct Json_Container
{
  Json_Container_Part *first;
  Json_Container_Part *last;
  u32 total_values_count;
};
static_assert(sizeof(Json_Container) == 24);

#define SPARSE_CHUNK_ARRAY_MAX_COUNT (1 << 13)
struct _Json_Sparse_Storage_Chunk
{
  u32 count;
};

struct Json_String_Sparse_Storage_Chunk
{
  u32 strings_count;
  Json_String strings[SPARSE_CHUNK_ARRAY_MAX_COUNT];
};
struct Json_Number_Sparse_Storage_Chunk
{
  u32 numbers_count;
  Json_Number numbers[SPARSE_CHUNK_ARRAY_MAX_COUNT];
};
struct Json_Pair_Sparse_Storage_Chunk
{
  u32 pairs_count;
  Json_Pair pairs[SPARSE_CHUNK_ARRAY_MAX_COUNT];
};
struct Json_Container_Sparse_Storage_Chunk
{
  u32 containers_count;
  Json_Container containers[SPARSE_CHUNK_ARRAY_MAX_COUNT];
};

struct Json_Sparse_Storage
{
  void *chunks[65536]; // 512KB
  u16 last_chunk_index;
};

struct Json_Storage
{
  Json_Sparse_Storage strings_storage;
  Json_Sparse_Storage numbers_storage;
  Json_Sparse_Storage pairs_storage;
  Json_Sparse_Storage containers_storage;
  
  Memory_Arena *arena;
};


#define JSON_TMP_BUFFER_SIZE MB(2)

struct Json_Parse_Context
{
  Buffer json_buffer;
  u64 at;
  
  Json_Parse_State states_stack[1024];
  i32 state_stack_top;
  
  Json_ID string_stack[1024];
  i32 string_stack_top;
  
  Json_Value values_stack[1024];
  i32 value_stack_top;
  
  Json_ID  obj_stack[1024];
  i32 obj_stack_top;
  
  Json_ID  arr_stack[1024];
  i32 arr_stack_top;
  
  Json_Storage storage;
};
global thread_local Json_Parse_Context g_parse_ctx;

#define _JSON_PARSER_STACK_CURR(parse_ctx, stack, stack_top) (parse_ctx)->stack[(parse_ctx)->stack_top]
#define _JSON_PARSER_STACK_POP(parse_ctx, stack, stack_top) ((parse_ctx)->stack[(parse_ctx)->stack_top--])
#define _JSON_PARSER_STACK_CHANGE(parse_ctx, new_item, stack, stack_top) (parse_ctx)->stack[(parse_ctx)->stack_top] = (new_item)
#define _JSON_PARSER_STACK_PUSH(parse_ctx, new_item, stack, stack_top) ((parse_ctx)->stack_top++, _JSON_PARSER_STACK_CHANGE(parse_ctx, new_item, stack, stack_top))

#define JSON_PARSER_CURR_STATE(parse_ctx) _JSON_PARSER_STACK_CURR(parse_ctx, states_stack, state_stack_top)
#define JSON_PARSER_POP_STATE(parse_ctx) _JSON_PARSER_STACK_POP(parse_ctx, states_stack, state_stack_top)
#define JSON_PARSER_CHANGE_STATE(parse_ctx, new_state) _JSON_PARSER_STACK_CHANGE(parse_ctx, new_state, states_stack, state_stack_top)
#define JSON_PARSER_PUSH_STATE(parse_ctx, new_state) _JSON_PARSER_STACK_PUSH(parse_ctx, new_state, states_stack, state_stack_top)

#define JSON_PARSER_CURR_STRING(parse_ctx) _JSON_PARSER_STACK_CURR(parse_ctx, string_stack, string_stack_top)
#define JSON_PARSER_POP_STRING(parse_ctx) _JSON_PARSER_STACK_POP(parse_ctx, string_stack, string_stack_top)
#define JSON_PARSER_CHANGE_STRING(parse_ctx, new_string) _JSON_PARSER_STACK_CHANGE(parse_ctx, new_string, string_stack, string_stack_top)
#define JSON_PARSER_PUSH_STRING(parse_ctx, new_string) _JSON_PARSER_STACK_PUSH(parse_ctx, new_string, string_stack, string_stack_top)

#define JSON_PARSER_CURR_OBJECT(parse_ctx) _JSON_PARSER_STACK_CURR(parse_ctx, obj_stack, obj_stack_top)
#define JSON_PARSER_POP_OBJECT(parse_ctx) _JSON_PARSER_STACK_POP(parse_ctx, obj_stack, obj_stack_top)
#define JSON_PARSER_CHANGE_OBJECT(parse_ctx, new_obj) _JSON_PARSER_STACK_CHANGE(parse_ctx, new_obj, obj_stack, obj_stack_top)
#define JSON_PARSER_PUSH_OBJECT(parse_ctx, new_obj) _JSON_PARSER_STACK_PUSH(parse_ctx, new_obj, obj_stack, obj_stack_top)

#define JSON_PARSER_CURR_ARRAY(parse_ctx) _JSON_PARSER_STACK_CURR(parse_ctx, arr_stack, arr_stack_top)
#define JSON_PARSER_POP_ARRAY(parse_ctx) _JSON_PARSER_STACK_POP(parse_ctx, arr_stack, arr_stack_top)
#define JSON_PARSER_CHANGE_ARRAY(parse_ctx, new_arr) _JSON_PARSER_STACK_CHANGE(parse_ctx, new_arr, arr_stack, arr_stack_top)
#define JSON_PARSER_PUSH_ARRAY(parse_ctx, new_arr) _JSON_PARSER_STACK_PUSH(parse_ctx, new_arr, arr_stack, arr_stack_top)

#define JSON_PARSER_CURR_VALUE(parse_ctx) _JSON_PARSER_STACK_CURR(parse_ctx, values_stack, value_stack_top)
#define JSON_PARSER_POP_VALUE(parse_ctx) _JSON_PARSER_STACK_POP(parse_ctx, values_stack, value_stack_top)
#define JSON_PARSER_CHANGE_VALUE(parse_ctx, new_value) _JSON_PARSER_STACK_CHANGE(parse_ctx, new_value, values_stack, value_stack_top)
#define JSON_PARSER_PUSH_VALUE(parse_ctx, new_value) _JSON_PARSER_STACK_PUSH(parse_ctx, new_value, values_stack, value_stack_top)


#if 1
# define JSON_PARSER_ADVANCE(parse_ctx, advance) (parse_ctx)->at += (advance)
# define JSON_PARSER_EOF(parse_ctx) ((parse_ctx)->at >= (parse_ctx)->json_buffer.size)
# define JSON_PARSER_READ(parse_ctx) (parse_ctx)->json_buffer.data[(parse_ctx)->at]
#else
# define JSON_PARSER_ADVANCE(parse_ctx, advance) json_parser_advance(advance)
# define JSON_PARSER_EOF(parse_ctx) json_parser_eof()
# define JSON_PARSER_READ(parse_ctx) json_parser_read()

internal void
json_parser_advance(u64 advance)
{
	file_stream_advance(&g_parse_ctx.file_stream, advance);
}

internal b32
json_parser_eof()
{
	b32 result = file_stream_eof(&g_parse_ctx.file_stream);
	return result;
}

internal u8
json_parser_read()
{
	u8 result = g_parse_ctx.file_stream.tmp_buffer.data[g_parse_ctx.file_stream.buf_offset];
	return result;
}

#endif

#define JSON_PARSER_CONSUME_WHITESPACES(parse_ctx) for (;json_is_whitespace(JSON_PARSER_READ(parse_ctx)); JSON_PARSER_ADVANCE(parse_ctx, 1));

#define make_json_sparse_storage_chunk(arena, T) (T*)_make_json_sparse_storage_chunk(arena, sizeof(T))
internal _Json_Sparse_Storage_Chunk *
_make_json_sparse_storage_chunk(Memory_Arena *arena, u32 chunk_size)
{
	_Json_Sparse_Storage_Chunk *chunk = (_Json_Sparse_Storage_Chunk *)m_arena_push(arena, chunk_size);
	chunk->count = 0;
	return chunk;
}

internal Json_ID
_json_sparse_storage_allocate(Memory_Arena *arena, Json_Sparse_Storage *sparse_storage, u32 chunk_size)
{
  assert(sparse_storage->last_chunk_index < array_count(sparse_storage->chunks));
	
	if (!sparse_storage->chunks[sparse_storage->last_chunk_index])
	{
		sparse_storage->chunks[sparse_storage->last_chunk_index] = _make_json_sparse_storage_chunk(arena, chunk_size);
	}
	
  // NOTE(fakhri): we assume strings_storage->chunks_count always
  _Json_Sparse_Storage_Chunk *chunk = (_Json_Sparse_Storage_Chunk *)sparse_storage->chunks[sparse_storage->last_chunk_index];
  assert(chunk);
  if (chunk->count == SPARSE_CHUNK_ARRAY_MAX_COUNT)
  {
		chunk = _make_json_sparse_storage_chunk(arena, chunk_size);
    
    sparse_storage->last_chunk_index++;
    assert(sparse_storage->last_chunk_index < array_count(sparse_storage->chunks));
    sparse_storage->chunks[sparse_storage->last_chunk_index] = chunk;
  }
  
  Json_ID string_id;
  string_id.chunk_index  = sparse_storage->last_chunk_index;
  string_id.chunk_offset = (u16)(chunk->count++);
  return string_id;
}

internal void *
json_get_sparse_chunk(Json_Sparse_Storage *sparse_storage, Json_ID id)
{
  assert(id.chunk_index < array_count(sparse_storage->chunks));
  void *chunk = sparse_storage->chunks[id.chunk_index];
  assert(id.chunk_offset < SPARSE_CHUNK_ARRAY_MAX_COUNT);
  return chunk;
}

internal Json_String *
json_string_ref_from_id(Json_ID string_id)
{
	Json_Sparse_Storage *strings_storage = &g_parse_ctx.storage.strings_storage;
  Json_String_Sparse_Storage_Chunk *chunk = (Json_String_Sparse_Storage_Chunk *)json_get_sparse_chunk(strings_storage, string_id);
  Json_String *result = chunk->strings + string_id.chunk_offset;
  return result;
}

internal Json_Number *
json_number_ref_from_id(Json_ID number_id)
{
	Json_Sparse_Storage *numbers_storage = &g_parse_ctx.storage.numbers_storage;
  Json_Number_Sparse_Storage_Chunk *chunk = (Json_Number_Sparse_Storage_Chunk *)json_get_sparse_chunk(numbers_storage, number_id);
  Json_Number *result = chunk->numbers + number_id.chunk_offset;
  return result;
}

internal Json_Pair *
json_pair_ref_from_id(Json_ID pair_id)
{
	Json_Sparse_Storage *pairs_storage = &g_parse_ctx.storage.pairs_storage;
  Json_Pair_Sparse_Storage_Chunk *chunk = (Json_Pair_Sparse_Storage_Chunk *)json_get_sparse_chunk(pairs_storage, pair_id);
  Json_Pair *result = chunk->pairs + pair_id.chunk_offset;
  return result;
}

internal Json_String
json_string_from_val(Json_Value string_val)
{
	Json_ID id = json_id_from_value(string_val);
	Json_Sparse_Storage *strings_storage = &g_parse_ctx.storage.strings_storage;
  Json_String_Sparse_Storage_Chunk *chunk = (Json_String_Sparse_Storage_Chunk *)json_get_sparse_chunk(strings_storage, id);
  Json_String result = chunk->strings[id.chunk_offset];
  return result;
}

internal Json_Number
json_number_from_val(Json_Value number_val)
{
	Json_ID id = json_id_from_value(number_val);
	Json_Sparse_Storage *numbers_storage = &g_parse_ctx.storage.numbers_storage;
  Json_Number_Sparse_Storage_Chunk *chunk = (Json_Number_Sparse_Storage_Chunk *)json_get_sparse_chunk(numbers_storage, id);
  Json_Number result = chunk->numbers[id.chunk_offset];
  return result;
}

internal Json_Pair
json_pair_from_val(Json_Value pair_val)
{
	Json_ID id = json_id_from_value(pair_val);
	Json_Sparse_Storage *pairs_storage = &g_parse_ctx.storage.pairs_storage;
  Json_Pair_Sparse_Storage_Chunk *chunk = (Json_Pair_Sparse_Storage_Chunk *)json_get_sparse_chunk(pairs_storage, id);
  Json_Pair result = chunk->pairs[id.chunk_offset];
  return result;
}

internal Json_ID
json_storage_allocate_string(Memory_Arena *arena, Json_Sparse_Storage *sparse_storage)
{
  Json_ID string_id = _json_sparse_storage_allocate(arena, sparse_storage, sizeof(Json_String_Sparse_Storage_Chunk));
  return string_id;
}

internal Json_ID
json_storage_allocate_number(Memory_Arena *arena, Json_Sparse_Storage *sparse_storage)
{
  Json_ID number_id = _json_sparse_storage_allocate(arena, sparse_storage, sizeof(Json_Number_Sparse_Storage_Chunk));
  return number_id;
}

internal Json_Value
json_storage_allocate_pair(Memory_Arena *arena, Json_Sparse_Storage *sparse_storage, Json_Pair pair)
{
  Json_ID pair_id = _json_sparse_storage_allocate(arena, sparse_storage, sizeof(Json_Pair_Sparse_Storage_Chunk));
  *json_pair_ref_from_id(pair_id) = pair;
	Json_Value result;
	result.pair_id = pair_id;
  return result;
}


internal Json_Container *
json_get_container_from_storage(Json_ID container_id)
{
  Json_Sparse_Storage *containers_storage = &g_parse_ctx.storage.containers_storage;
	Json_Container_Sparse_Storage_Chunk *chunk = (Json_Container_Sparse_Storage_Chunk *)json_get_sparse_chunk(containers_storage, container_id);
  Json_Container *result = chunk->containers + container_id.chunk_offset;
  return result;
}

internal Json_ID
json_storage_allocate_container(Memory_Arena *arena)
{
	Json_Sparse_Storage *containers_storage = &g_parse_ctx.storage.containers_storage;
	Json_ID container_id = _json_sparse_storage_allocate(arena, containers_storage, sizeof(Json_Container_Sparse_Storage_Chunk));
  Json_Container *container = json_get_container_from_storage(container_id);
  
  container->first = m_arena_push_struct(arena, Json_Container_Part);
  container->last = container->first;
  
  return container_id;
}

internal void
json_container_push_value(Json_Storage *storage, Json_ID container_id, Json_Value value)
{
  Json_Container *container = json_get_container_from_storage(container_id);
  Json_Container_Part *chunk = container->last;
  if (chunk->count >= array_count(chunk->values))
  {
    chunk = m_arena_push_struct(storage->arena, Json_Container_Part);
    assert(chunk);
    container->last->next = chunk;
    container->last = chunk;
  }
  
  assert(chunk);
  chunk->values[chunk->count++] = value;
  container->total_values_count += 1;
}

internal Json_ID
json_parse_string(Json_Parse_Context *parse_ctx)
{
  Json_ID string_id = json_storage_allocate_string(parse_ctx->storage.arena, &parse_ctx->storage.strings_storage);
  Json_String *string = json_string_ref_from_id(string_id);
  
  assert(JSON_PARSER_READ(parse_ctx) == '"');
  string->offset = (u32)(++parse_ctx->at);
  
  for(;!JSON_PARSER_EOF(parse_ctx);)
  {
    if (JSON_PARSER_READ(parse_ctx) == '"') break;
    
    u32 advance = 1;
    if (JSON_PARSER_READ(parse_ctx) == '\\')
    {
      advance = 2; 
      if (JSON_PARSER_READ(parse_ctx) == 'u') advance = 6;
    }
    
    JSON_PARSER_ADVANCE(parse_ctx, advance);
  }
  
  assert(JSON_PARSER_READ(parse_ctx) == '"');
  string->size = (u32)(parse_ctx->at - string->offset);
  JSON_PARSER_ADVANCE(parse_ctx, 1);
  return string_id;
}

internal Json_Value
json_parse_number(Json_Parse_Context *parse_ctx)
{
  Json_ID number_id = json_storage_allocate_number(parse_ctx->storage.arena, &parse_ctx->storage.numbers_storage);
  Json_Number *number = json_number_ref_from_id(number_id);
  
  b32 negative = 0;
  u8 digit = JSON_PARSER_READ(parse_ctx);
  if (digit == '-')
  {
    negative = 1;
    JSON_PARSER_ADVANCE(parse_ctx, 1);
    digit = JSON_PARSER_READ(parse_ctx);
  }
  
  b32 number_is_float = 0;
  b32 negative_exponenet = 0;
  
  u64 integral = 0;
  f64 fraction = 0;
  u64 exponent = 0;
  
  assert(char_is_digit(digit));
  
  if (digit != '0')
  {
    integral = digit - '0';
    JSON_PARSER_ADVANCE(parse_ctx, 1);
    
    for (;!JSON_PARSER_EOF(parse_ctx);JSON_PARSER_ADVANCE(parse_ctx, 1))
    {
      digit = JSON_PARSER_READ(parse_ctx);
      if (!char_is_digit(digit)) break;
      integral = integral * 10 + (digit - '0');
    }
  }
  else
  {
    JSON_PARSER_ADVANCE(parse_ctx, 1);
  }
  
  if (!JSON_PARSER_EOF(parse_ctx))
  {
    digit = JSON_PARSER_READ(parse_ctx);
    if (digit == '.')
    {
      number_is_float = 1;
      JSON_PARSER_ADVANCE(parse_ctx, 1);
      f64 one_over_10 = 1.0 / 10.0;
      f64 pow_neg_ten = one_over_10;
      for (;!JSON_PARSER_EOF(parse_ctx); JSON_PARSER_ADVANCE(parse_ctx, 1))
      {
        digit = JSON_PARSER_READ(parse_ctx);
        if (!char_is_digit(digit)) break;
        fraction = fraction + ((f64)(digit - '0') * pow_neg_ten);
        pow_neg_ten *= one_over_10;
      }
    }
  }
  
  if (!JSON_PARSER_EOF(parse_ctx))
  {
    digit = JSON_PARSER_READ(parse_ctx);
    if (digit == 'e' || digit == 'E')
    {
      number_is_float = 1;
      JSON_PARSER_ADVANCE(parse_ctx, 1);
      assert(!JSON_PARSER_EOF(parse_ctx));
      digit = JSON_PARSER_READ(parse_ctx);
      
      if (digit == '-' || digit == '+')
      {
        negative_exponenet = (digit == '-');
        JSON_PARSER_ADVANCE(parse_ctx, 1);
        assert(!JSON_PARSER_EOF(parse_ctx));
        digit = JSON_PARSER_READ(parse_ctx);
      }
      
      assert(char_is_digit(digit));
      exponent = digit - '0';
      JSON_PARSER_ADVANCE(parse_ctx, 1);
      
      for (;!JSON_PARSER_EOF(parse_ctx); JSON_PARSER_ADVANCE(parse_ctx, 1))
      {
        digit = JSON_PARSER_READ(parse_ctx);
        if (!char_is_digit(digit)) break;
        exponent = exponent * 10 + (digit - '0');
      }
    }
  }
  
	Json_Value_Kind kind = (number_is_float? JSON_VALUE_NUMBER_FLOAT:
													(negative? JSON_VALUE_NUMBER_INT:JSON_VALUE_NUMBER_UINT));
  if (!number_is_float)
  {
    number->val_int = negative? (i64)((-1) * (i64)integral):integral;
  }
  else
  {
    number->val_float = (f64)integral + fraction;
    f64 c = log(10);
    number->val_float *= exp((negative_exponenet? -1: 1) * exponent * c);
    number->val_float *= negative? -1:1;
  }
	
	Json_Value number_val = make_json_value(kind, number_id);
  return number_val;
}

internal b32
json_is_whitespace(u8 ch)
{
  b32 result = ((ch == ' ')  ||
								(ch == '\n') ||
								(ch == '\r') ||
								(ch == '\t'));
  return result;
}

internal void
json_clear_sparse_storage(Json_Sparse_Storage *sparse_storage)
{
	sparse_storage->chunks[0] = 0;
	sparse_storage->last_chunk_index = 0;
}

internal void
json_init_storage(Memory_Arena *arena)
{
	Json_Storage *storage = &g_parse_ctx.storage;
  storage->arena = arena;
	json_clear_sparse_storage(&storage->strings_storage);
	json_clear_sparse_storage(&storage->numbers_storage);
	json_clear_sparse_storage(&storage->pairs_storage);
	json_clear_sparse_storage(&storage->containers_storage);
}

internal b32
json_unfinished_state()
{
	b32 result = 0;
	if (g_parse_ctx.state_stack_top >= 0)
	{
		Json_Parse_State state = JSON_PARSER_CURR_STATE(&g_parse_ctx);
		result = ((state == STATE_PARSE_VALUE_FINISH_OBJ) || 
							(state == STATE_PARSE_VALUE_FINISH_ARR) ||
							(state == STATE_PARSE_ARR_FINISH) ||
							(state == STATE_PARSE_OBJ_FINISH));
	}
	return result;
}


internal Json_Value
json_parse(Memory_Arena *arena, Buffer input)
{
	g_parse_ctx.state_stack_top = 0;
  g_parse_ctx.string_stack_top = 0;
  g_parse_ctx.value_stack_top = 0;
  g_parse_ctx.obj_stack_top = 0;
  g_parse_ctx.arr_stack_top = 0;
	
	json_init_storage(arena);
  JSON_PARSER_CHANGE_STATE(&g_parse_ctx, STATE_PARSE_VALUE);
	
	
#if 1
	g_parse_ctx.json_buffer = input;
#else
	g_parse_ctx.file_stream = open_file_stream(arena, input);
#endif
  g_parse_ctx.at = 0;
  
  for (;!JSON_PARSER_EOF(&g_parse_ctx) || json_unfinished_state();)
  {
    switch (JSON_PARSER_CURR_STATE(&g_parse_ctx))
    {
      case STATE_PARSE_VALUE:
      {
        JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
        switch(JSON_PARSER_READ(&g_parse_ctx))
        {
          case '{':
          {
            JSON_PARSER_CHANGE_STATE(&g_parse_ctx, STATE_PARSE_VALUE_FINISH_OBJ);
            JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_OBJ);
          } break;
          case '[':
          {
            JSON_PARSER_CHANGE_STATE(&g_parse_ctx, STATE_PARSE_VALUE_FINISH_ARR);
            JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_ARR);
          } break;
          case 'f':
          case 't':
          {
            b16 bool_value = (JSON_PARSER_READ(&g_parse_ctx) == 't');
            Json_Value json_val = make_json_bool(bool_value);
            
            // NOTE(fakhri): false/true
            static u32 advance[2] = {sizeof("false") - 1, sizeof("true") - 1};
            JSON_PARSER_ADVANCE(&g_parse_ctx, advance[bool_value]);
            JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
            JSON_PARSER_PUSH_VALUE(&g_parse_ctx, json_val);
            JSON_PARSER_POP_STATE(&g_parse_ctx);
          } break;
          case 'n':
          {
            Json_Value json_val = {JSON_VALUE_NULL};
            JSON_PARSER_ADVANCE(&g_parse_ctx, sizeof("null") - 1);
            JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
            JSON_PARSER_PUSH_VALUE(&g_parse_ctx, json_val);
            JSON_PARSER_POP_STATE(&g_parse_ctx);
          } break;
          case '"':
          {
            Json_ID string_id = json_parse_string(&g_parse_ctx);
            Json_Value json_val = make_json_value(JSON_VALUE_STRING, string_id);
            JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
            JSON_PARSER_PUSH_VALUE(&g_parse_ctx, json_val);
            JSON_PARSER_POP_STATE(&g_parse_ctx);
          } break;
          default:
          {
            // NOTE(fakhri): number
            Json_Value number_val = json_parse_number(&g_parse_ctx);
            JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
            JSON_PARSER_PUSH_VALUE(&g_parse_ctx, number_val);
            JSON_PARSER_POP_STATE(&g_parse_ctx);
          };
        }
      } break;
      
      case STATE_PARSE_VALUE_FINISH_OBJ:
      {
        Json_ID obj_id = JSON_PARSER_POP_OBJECT(&g_parse_ctx);
        Json_Value json_val = make_json_value(JSON_VALUE_OBJECT, obj_id);
        
        JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
        JSON_PARSER_PUSH_VALUE(&g_parse_ctx, json_val);
        JSON_PARSER_POP_STATE(&g_parse_ctx);
      } break;
      
      case STATE_PARSE_VALUE_FINISH_ARR:
      {
        Json_ID arr_id = JSON_PARSER_POP_ARRAY(&g_parse_ctx);
        Json_Value json_val = make_json_value(JSON_VALUE_ARRAY, arr_id);
        
        JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
        JSON_PARSER_PUSH_VALUE(&g_parse_ctx, json_val);
        JSON_PARSER_POP_STATE(&g_parse_ctx);
      } break;
      
      case STATE_PARSE_OBJ:
      {
        assert(JSON_PARSER_READ(&g_parse_ctx) == '{');
        JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
        
        JSON_PARSER_POP_STATE(&g_parse_ctx);
        JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
        
        Json_ID obj_id = json_storage_allocate_container(g_parse_ctx.storage.arena);
        JSON_PARSER_PUSH_OBJECT(&g_parse_ctx, obj_id);
        
        switch(JSON_PARSER_READ(&g_parse_ctx))
        {
          case '}':
          {
            // NOTE(fakhri): empty object
            JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
          } break;
          case '"':
          {
            // NOTE(fakhri): parse collection of string/value
            JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_PAIRS);
          } break;
        }
      } break;
      case STATE_PARSE_PAIRS:
      {
        assert(JSON_PARSER_READ(&g_parse_ctx) == '"');
        
        // NOTE(fakhri): parse string
        Json_ID string_index = json_parse_string(&g_parse_ctx);
        JSON_PARSER_PUSH_STRING(&g_parse_ctx, string_index);
        
        JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
        assert(JSON_PARSER_READ(&g_parse_ctx) == ':');
        JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
        
        JSON_PARSER_CHANGE_STATE(&g_parse_ctx, STATE_PARSE_OBJ_FINISH);
        JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_VALUE);
      } break;
      
      case STATE_PARSE_OBJ_FINISH:
      {
        Json_Pair pair;
        pair.string_id = JSON_PARSER_POP_STRING(&g_parse_ctx);
        pair.value        = JSON_PARSER_POP_VALUE(&g_parse_ctx);
        
        Json_Value pair_val = json_storage_allocate_pair(g_parse_ctx.storage.arena, &g_parse_ctx.storage.pairs_storage, pair);
        Json_ID obj_id = JSON_PARSER_CURR_OBJECT(&g_parse_ctx);
        json_container_push_value(&g_parse_ctx.storage, obj_id, pair_val);
        
        if (JSON_PARSER_READ(&g_parse_ctx) == ',')
        {
          // NOTE(fakhri): we still have pairs
          JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
          JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
          JSON_PARSER_CHANGE_STATE(&g_parse_ctx, STATE_PARSE_PAIRS);
        }
        else
        {
          assert(JSON_PARSER_READ(&g_parse_ctx) == '}');
          JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
          JSON_PARSER_POP_STATE(&g_parse_ctx);
        }
      } break;
      
      case STATE_PARSE_ARR:
      {
        assert(JSON_PARSER_READ(&g_parse_ctx) == '[');
        JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
        
        JSON_PARSER_POP_STATE(&g_parse_ctx);
        JSON_PARSER_CONSUME_WHITESPACES(&g_parse_ctx);
        
        
        Json_ID arr_id = json_storage_allocate_container(g_parse_ctx.storage.arena);
        JSON_PARSER_PUSH_ARRAY(&g_parse_ctx, arr_id);
        
        if(JSON_PARSER_READ(&g_parse_ctx) == ']')
        {
          // NOTE(fakhri): empty array
          JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
        }
        else
        {
          // NOTE(fakhri): parse list of values
          JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_ARR_FINISH);
          JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_VALUE);
        }
      } break;
      case STATE_PARSE_ARR_FINISH:
      {
        Json_Value value = JSON_PARSER_POP_VALUE(&g_parse_ctx);
        
        Json_ID arr_index = JSON_PARSER_CURR_ARRAY(&g_parse_ctx);
        json_container_push_value(&g_parse_ctx.storage, arr_index, value);
        
        if (JSON_PARSER_READ(&g_parse_ctx) == ',')
        {
          // NOTE(fakhri): we still have values
          JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
          JSON_PARSER_PUSH_STATE(&g_parse_ctx, STATE_PARSE_VALUE);
        }
        else
        {
          assert(JSON_PARSER_READ(&g_parse_ctx) == ']');
          JSON_PARSER_ADVANCE(&g_parse_ctx, 1);
          JSON_PARSER_POP_STATE(&g_parse_ctx);
        }
      } break;
    }
  }
  
  return JSON_PARSER_CURR_VALUE(&g_parse_ctx);
}

struct Json_Container_Iterator
{
  Json_Container_Part *chunk;
  u32 index;
};

internal Json_Container_Iterator
json_container_it(Json_Container *container)
{
  Json_Container_Iterator it;
  it.chunk = container->first;
  it.index = 0;
  return it;
}

internal b32
json_container_it_valid(Json_Container_Iterator *it)
{
  b32 result = it->chunk != 0;
  return result;
}

internal void
json_container_it_advance(Json_Container_Iterator *it)
{
  it->index += 1;
  if (it->index == it->chunk->count)
  {
    it->chunk = it->chunk->next;
    it->index = 0;
  }
}

internal b32
json_array_it_is_last(Json_Container_Iterator *it)
{
  b32 result = (it->index == it->chunk->count - 1) && (it->chunk->next == 0);
  return result;
}

internal String8
str8_from_json_str(Json_String string)
{
	String8 result = str8(g_parse_ctx.json_buffer.data + string.offset, string.size);
	return result;
}

internal Json_Value
json_get_value_in_object(Json_Value obj_value, String8 key)
{
  Json_Value result = {};
  Json_Container *json_obj = json_get_container_from_storage(json_id_from_value(obj_value));
  
  for(Json_Container_Iterator it = json_container_it(json_obj); 
			json_container_it_valid(&it); 
			json_container_it_advance(&it))
  {
    Json_Value pair_val = it.chunk->values[it.index];
    Json_Pair *pair = json_pair_ref_from_id(pair_val.pair_id);
    Json_String string = *json_string_ref_from_id(pair->string_id);
    if (str8_match(key, str8_from_json_str(string), 0))
    {
      result = pair->value;
    }
  }
  return result;
}