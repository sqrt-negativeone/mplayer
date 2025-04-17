
internal void
network_socket_close(Socket socket)
{
	if (socket.secured)
	{
		SSL_shutdown(socket.ssl);
		SSL_free(socket.ssl);
	}
	platform->close_socket(socket.handle);
}

internal b32
network_socket_send(Socket socket, Buffer buf)
{
	b32 result = 0;
	if (socket.secured)
	{
		result = ssl_network_send_buffer(socket.ssl, buf.data, buf.size);
	}
	else
	{
		result = platform->network_send_buffer(socket.handle, buf);
	}
	
	return result;
}

internal i32 
network_socket_read(Socket socket, Buffer buf)
{
	i32 result = 0;
	if (socket.secured)
	{
		size_t ret = 0;
		if (!SSL_read_ex(socket.ssl, buf.data, buf.size, &ret))
		{
			if (SSL_get_error(socket.ssl, 0) != SSL_ERROR_ZERO_RETURN)
			{
				result = -1;
			}
		}
		else
			result = (i32)ret;
	}
	else
	{
		result = platform->network_receive_buffer(socket.handle, buf);
	}
	
	return result;
}

internal SSL_CTX *
ssl_context_init()
{
	SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());
	assert(ssl_ctx);
	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, 0);
	
	// TODO(fakhri): can we have this in memory?
	assert(SSL_CTX_load_verify_file(ssl_ctx, "cacert.pem"));
	assert(SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION));
	
	return ssl_ctx;
}

internal Socket
secure_socket_connect(SSL_CTX *ssl_ctx, const char *hostname, const char *port)
{
	Socket result = ZERO_STRUCT;
	
	Socket_Handle socket = platform->connect_to_server(hostname, port);
	if (platform->is_valid_socket(socket))
	{
		SSL *ssl = SSL_new(ssl_ctx);
		assert(ssl);
		
		BIO *bio = BIO_new(BIO_s_socket());
		assert(bio);
		
		union {Socket_Handle s; int ssl_s;} conv = {.s = socket};
		BIO_set_fd(bio, conv.ssl_s, BIO_CLOSE);
		SSL_set_bio(ssl, bio, bio);
		
		SSL_set_tlsext_host_name(ssl, hostname);
		SSL_set1_host(ssl, hostname);
		
		// NOTE(fakhri): tls handshake:
		if (SSL_connect(ssl) == 1)
		{
			result.secured = true;
			result.valid = 1;
			result.handle   = socket;
			result.ssl = ssl;
		}
		else
		{
			LogError("Failed to connect to the server");
			/*
			* If the failure is due to a verification error we can get more
			* information about it from SSL_get_verify_websocket().
			*/
			if (SSL_get_verify_result(ssl) != X509_V_OK)
			{
				LogError("Verify error: %s",
								 X509_verify_cert_error_string(SSL_get_verify_result(ssl)));
			}
		}
	}
	return result;
}


internal b32
ssl_network_send_buffer(SSL *ssl, const void *data, u64 size)
{
	char *buffer = (char *)data;
	b32 result = true;
	u64 bytes_to_send = size;
	i32 total_bytes_sent = 0;
	
	while(bytes_to_send)
	{
		size_t bytes_sent;
		if (!SSL_write_ex(ssl, buffer + total_bytes_sent, bytes_to_send, &bytes_sent))
		{
			LogError("send call failed");
			result = false;
			break;
		}
		total_bytes_sent += (i32)bytes_sent;
		bytes_to_send -= (i32)bytes_sent;
	}
	
	return result;
}

internal void
init_buffered_socket(Buffered_Socket *buf_socket, Socket socket)
{
	buf_socket->socket = socket;
	buf_socket->availabe_bytes = 0;
	buf_socket->offset         = 0;
}

internal i64
buffered_socket_read(Buffered_Socket *buf_socket, u8 *dst, u64 dst_size)
{
	if (!buf_socket->availabe_bytes)
	{
		buf_socket->offset = 0;
		buf_socket->availabe_bytes = network_socket_read(buf_socket->socket, 
																										 make_buffer(buf_socket->buf, BUFFERED_SOCKET_CAPACITY));
		if (buf_socket->availabe_bytes < 0)
			return -1;
	}
	
	u64 bytes_to_copy = MIN(dst_size, (u64)buf_socket->availabe_bytes);
	memory_copy(dst, buf_socket->buf + buf_socket->offset, bytes_to_copy);
	buf_socket->offset         += bytes_to_copy;
	buf_socket->availabe_bytes -= bytes_to_copy;
	return bytes_to_copy;
}

internal void
buffered_socket_receive_buffer(Buffered_Socket *buf_socket, u8 *buf, u64 receive_bytes)
{
	u8 *buf_ptr = buf;
	for (;receive_bytes;)
	{
		i64 received = buffered_socket_read(buf_socket, buf_ptr, receive_bytes);
		if (received < 0)
		{
			// NOTE(fakhri): network error
			// TODO(fakhri): handle this
			not_impemeneted();
		}
		receive_bytes -= received;
	}
}

internal String8
buffered_socket_receive_line(Memory_Arena *arena, Buffered_Socket *buf_socket, u64 max_line_size)
{
	Memory_Checkpoint scratch = begin_scratch(&arena, 1);
	
	u64 tmp_buf_size = max_line_size;
	String8 result = {};
	
	char *tmp_buf = m_arena_push_array(scratch.arena, char, tmp_buf_size); 
	char *tmp_buf_head = tmp_buf;
	char *tmp_buf_end   = tmp_buf_head + tmp_buf_size;
	
	b32 failed = 0;
	for (;tmp_buf_head != tmp_buf_end;)
	{
		i64 received = buffered_socket_read(buf_socket, (u8*)tmp_buf_head, 1);
		if (received == 1)
		{
			char recevied_char = *tmp_buf_head++;
			if (recevied_char == '\n')
			{
				break;
			}
		}
		else if (received == 0)
		{
			break;
		}
		else
		{
			failed = 1;
			break;
		}
	}
	
	if (!failed)
	{
		result = str8_clone(arena, str8((u8*)tmp_buf, tmp_buf_head - tmp_buf));
	}
	
	end_scratch(scratch);
	return result;
}


internal void
http_send_request(Socket socket, Http_Request request)
{
	Memory_Checkpoint scratch = begin_scratch(0, 0);
	
	// NOTE(fakhri): build the request message
	String8 message = ZERO_STRUCT;
	{
		String8_List stream = ZERO_STRUCT;
		str8_list_push_f(scratch.arena, &stream, "%s %.*s HTTP/1.1" CRLF, http_method_name(request.method), STR8_EXPAND(request.uri));
		
		for (Header_Field *field = request.header_fields.fields_first;
				 field;
				 field = field->next_list)
		{
			String_Join join_params = ZERO_STRUCT;
			join_params.sep = str8_lit(", ");
			String8 field_value = str8_list_join(scratch.arena, field->values, &join_params);
			
			str8_list_push_f(scratch.arena, &stream, "%.*s: %.*s" CRLF, STR8_EXPAND(field->name), STR8_EXPAND(field_value));
		}
		
		str8_list_push(scratch.arena, &stream, str8_lit(CRLF));
		str8_list_push(scratch.arena, &stream, request.body);
		
		message = str8_list_join(scratch.arena, stream, 0);
	}
	
	Log("Request Message: %.*s", STR8_EXPAND(message));
	
	network_socket_send(socket, to_buffer(message));
	end_scratch(scratch);
}


internal void
http_receive_body(Memory_Arena *arena, Buffered_Socket *buf_socket, Http_Response *response)
{
	Memory_Checkpoint scratch = begin_scratch(&arena, 1);
	if (http_header_match_field_value_case_insensitive(&response->header_fields, str8_lit("Transfer-Encoding"), str8_lit("chunked")))
	{
		String8_List body_stream = ZERO_STRUCT;
		
		for(;;)
		{
			String8 chunk_size_line = buffered_socket_receive_line(scratch.arena, buf_socket);
			chunk_size_line = str8_chop_first_occurence(chunk_size_line, str8_lit(CRLF));
			u32 ows_index = 0;
			while (ows_index < chunk_size_line.len && !char_is_ows(chunk_size_line.str[ows_index])) ows_index += 1;
			
			String8 chunk_size_hex_str = prefix8(chunk_size_line, ows_index);
			u64 chunk_size = u64_from_str8_base16(chunk_size_hex_str);
			if (chunk_size == 0)
			{
				break;
			}
			
			String8 body_chunk;
			body_chunk.str = m_arena_push_array(scratch.arena, u8, chunk_size + 2);
			body_chunk.len = chunk_size;
			buffered_socket_receive_buffer(buf_socket, body_chunk.str, body_chunk.len + 2);
			str8_list_push(arena, &response->body, body_chunk);
		}
	}
	else
	{
		Header_Field *content_len_field = http_header_find_field(&response->header_fields, str8_lit("Content-Length"));
		if (content_len_field)
		{
			u64 body_size = u64_from_str8_base10(content_len_field->values.first->str);
			String8 body;
			body.str = m_arena_push_array(scratch.arena, u8, body_size);;
			body.len = body_size;
			buffered_socket_receive_buffer(buf_socket, body.str, body.len);
			str8_list_push(arena, &response->body, body);
		}
	}
	end_scratch(scratch);
}

internal void
http_receive_response(Memory_Arena *arena, Buffered_Socket *buf_socket, Http_Response *response)
{
	Memory_Checkpoint scratch = begin_scratch(&arena, 1);
	
	// NOTE(fakhri): parse the status line
	{
		response->status_line = buffered_socket_receive_line(arena, buf_socket);
		response->http_version = HTTP_VERSION_UNSUPPORTED;
		String8 version_string = str8_chop_first_occurence(response->status_line, str8_lit(" "));
		if (find_substr8(version_string, str8_lit("HTTP/1.0"), 0, 0) < version_string.len)
		{
			response->http_version = HTTP_1_0;
		}
		else if (find_substr8(version_string, str8_lit("HTTP/1.1"), 0, 0) < version_string.len)
		{
			response->http_version = HTTP_1_1;
		}
		
		String8 status_code_str = str8_skip_first(response->status_line, version_string.len+ 1);
		status_code_str = str8_skip_ows(status_code_str);
		
		if (status_code_str.len>= 3 &&
				char_is_digit(status_code_str.str[0]) &&
				char_is_digit(status_code_str.str[1]) &&
				char_is_digit(status_code_str.str[2]))
		{
			status_code_str.len= 3;
			response->status_code = (u32)u64_from_str8_base10(status_code_str);
		}
	}
	
	String8_List headers = ZERO_STRUCT;
	// NOTE(fakhri): receive header fields
	{
		String8 header_field = buffered_socket_receive_line(arena, buf_socket);
		
		// TODO(fakhri): error detection
		while(header_field.len&& 
					header_field.len!= 2 && 
					header_field.str[0] != '\r' && header_field.str[1] != '\n')
		{
			str8_list_push(scratch.arena, &headers, header_field);
			header_field = buffered_socket_receive_line(arena, buf_socket);
		}
	}
	
	http_parse_header_fields(arena, headers, &response->header_fields);
	
	http_receive_body(arena, buf_socket, response);
	end_scratch(scratch);
}
