
#define LASTFM_USER_AGENT "mplayer"
#define LASTFM_ROOT_URL "ws.audioscrobbler.com"

#include "mplayer_lastfm_keys.h"

struct Mplayer_Lastfm_Pair
{
	String8 key;
	String8 value;
};

struct Mplayer_Lastfm_Parameter
{
	String8 name;
	String8 value;
};

internal String8
mplayer_lastfm_construct_uri_with_signature(Memory_Arena *arena, Mplayer_Lastfm_Parameter *params, u32 params_count)
{
	Memory_Checkpoint scratch = get_scratch(&arena, 1);
	String8_List uri_args = ZERO_STRUCT;
	String8_List lastfm_param_scheme = ZERO_STRUCT;
	
	// TODO(fakhri): sort params alphabetically
	
	for (u32 i = 0; i < params_count; i += 1)
	{
		str8_list_push_f(scratch.arena, &uri_args, "%.*s=%.*s", STR8_EXPAND(params[i].name), STR8_EXPAND(params[i].value));
		str8_list_push_f(scratch.arena, &lastfm_param_scheme, "%.*s%.*s", STR8_EXPAND(params[i].name), STR8_EXPAND(params[i].value));
	}
	
	String_Join list_join = {.pre=str8_lit(""), .sep = str8_lit(""), .post = str8_lit(LASTFM_SHARED_SECRET)};
	String8 signature_str = str8_list_join(scratch.arena, lastfm_param_scheme, &list_join);
	u8 api_sig[16];
	u64 digest_len = 0;
	EVP_Q_digest(0, "MD5", 0, signature_str.str, signature_str.len, api_sig, &digest_len);
	assert(digest_len == 16);
	
	char hex_str_api_sig[32];
	for (u32 i = 0; i < array_count(api_sig); i += 1)
	{
		char temp[3];
		stbsp_sprintf(temp, "%02x", api_sig[i]);
		hex_str_api_sig[2 * i]     = temp[0];
		hex_str_api_sig[2 * i + 1] = temp[1];
	}
	
	str8_list_push_f(scratch.arena, &uri_args, "api_sig=%.*s", 32, hex_str_api_sig);
	list_join = {.pre=str8_lit("/2.0/?"), .sep = str8_lit("&"), .post = str8_lit("")};
	String8 result = str8_list_join(arena, uri_args, &list_join);
	return result;
}


#define LASTFM_PATH str8_lit("lastfm.mplayer")

internal void
mplayer_lastfm_save(Mplayer_Lastfm lastfm)
{
	if (lastfm.valid)
	{
		File_Handle *file = platform->open_file(LASTFM_PATH, File_Open_Write | File_Create_Always);
		if (file)
		{
			mplayer_serialize(file, lastfm.username);
			mplayer_serialize(file, lastfm.session_key);
			platform->close_file(file);
		}
	}
}

internal void
mplayer_lastfm_invalidate(Mplayer_Lastfm *lastfm)
{
	lastfm->valid = false;
	m_arena_free_all(&lastfm->arena);
	memory_zero_struct(lastfm);
}

internal void
mplayer_lastfm_start_authentication(Mplayer_Lastfm *lastfm)
{
	Memory_Checkpoint scratch = get_scratch(0, 0);
	
	mplayer_lastfm_invalidate(lastfm);
	Socket socket = network_socket_connect(LASTFM_ROOT_URL, "80");
	
	Buffered_Socket buf_socket = ZERO_STRUCT;
	init_buffered_socket(&buf_socket, socket);
	
	String8 token = ZERO_STRUCT;
	{
		Mplayer_Lastfm_Parameter params[] = {
			{str8_lit("api_key"), str8_lit(LASTFM_API_KEY)},
			{str8_lit("method"),  str8_lit("auth.getToken")},
		};
		Http_Request req = ZERO_STRUCT;
		req.method = HTTP_METHOD_GET;
		req.uri = mplayer_lastfm_construct_uri_with_signature(scratch.arena, params, array_count(params));
		http_send_request(socket, req);
		
		Http_Response response = ZERO_STRUCT;
		http_receive_response(scratch.arena, &buf_socket, &response);
		
		String8 body = str8_list_join(scratch.arena, response.body, 0);
		Log("Http response: %.*s", STR8_EXPAND(body));
		token = str8_find_enclosed_substr(body, str8_lit("<token>"), str8_lit("</token>"));
		Log("Token: %.*s", STR8_EXPAND(token));
	}
	
	if (token.len)
	{
		lastfm->token = str8_clone(&lastfm->arena, token);
		String8 auth_url = str8_f(scratch.arena, "http://www.last.fm/api/auth/?api_key=" LASTFM_API_KEY "&token=%.*s", STR8_EXPAND(token));
		platform->open_url_in_default_browser(auth_url);
		
		ui_open_modal_menu(mplayer_ctx->modal_menu_ids[MODAL_Lastfm_Wait_User_Confirmation]);
		network_socket_close(socket);
	}
	
}

internal void
mplayer_lastfm_finish_authentication(Mplayer_Lastfm *lastfm)
{
	Memory_Checkpoint scratch = get_scratch(0, 0);
	Socket socket = network_socket_connect(LASTFM_ROOT_URL, "80");
	Mplayer_Lastfm_Parameter params[] = {
		{str8_lit("api_key"), str8_lit(LASTFM_API_KEY)},
		{str8_lit("method"),  str8_lit("auth.getSession")},
		{str8_lit("token"),   lastfm->token},
	};
	
	Http_Request req = ZERO_STRUCT;
	req.method = HTTP_METHOD_GET;
	req.uri = mplayer_lastfm_construct_uri_with_signature(scratch.arena, params, array_count(params));;
	
	http_send_request(socket, req);
	
	Buffered_Socket buf_socket = ZERO_STRUCT;
	init_buffered_socket(&buf_socket, socket);
	Http_Response response = ZERO_STRUCT;
	http_receive_response(scratch.arena, &buf_socket, &response);
	
	String8 body = str8_list_join(scratch.arena, response.body, 0);
	Log("Http response: %.*s", STR8_EXPAND(body));
	
	String8 username = str8_find_enclosed_substr(body, str8_lit("<name>"), str8_lit("</name>"));
	String8 session_key = str8_find_enclosed_substr(body, str8_lit("<key>"), str8_lit("</key>"));
	Log("Name: %.*s", STR8_EXPAND(username));
	Log("Session Key: %.*s", STR8_EXPAND(session_key));
	
	if (username.len && session_key.len)
	{
		lastfm->valid = true;
		lastfm->username    = str8_clone(&lastfm->arena, username);
		lastfm->session_key = str8_clone(&lastfm->arena, session_key);
		mplayer_lastfm_save(*lastfm);
	}
	
	network_socket_close(socket);
}

internal Mplayer_Lastfm
mplayer_lastfm_init()
{
	Mplayer_Lastfm result = ZERO_STRUCT;
	Memory_Checkpoint scratch = get_scratch(0, 0);
	
	String8 username = ZERO_STRUCT;
	String8 session_key = ZERO_STRUCT;
	
	Buffer lastfm_conent = platform->load_entire_file(scratch.arena, LASTFM_PATH);
	if (lastfm_conent.size)
	{
		result.valid = true;
		Byte_Stream bs = make_byte_stream(lastfm_conent);
		byte_stream_read(&bs, &username);
		byte_stream_read(&bs, &session_key);
	}
	
	if (result.valid && username.len && session_key.len)
	{
		result.username    = str8_clone(&result.arena, username);
		result.session_key = str8_clone(&result.arena, session_key);
		mplayer_lastfm_save(result);
	}
	
	return result;
}

internal void
mplayer_lastfm_update_now_playing(Mplayer_Track *track)
{
	
}

internal void
mplayer_lastfm_scrobble_track(Mplayer_Track *track)
{
	
}