/* date = April 12th 2025 5:21 pm */

#ifndef NETWORK_H
#define NETWORK_H

#include "third_party/openssl/ssl.h"

struct Socket
{
	b32 valid;
	b32 secured;
	SSL *ssl;
	Socket_Handle handle;
};

#define BUFFERED_SOCKET_CAPACITY 4096
struct Buffered_Socket
{
	Socket socket;
	i64 availabe_bytes;
	u64 offset;
	u8 buf[BUFFERED_SOCKET_CAPACITY];
};

internal SSL_CTX *ssl_context_init();
internal Socket secure_socket_connect(SSL_CTX *ssl_ctx, const char *hostname, const char *port);
internal b32 ssl_network_send_buffer(SSL *ssl, const void *data, u64 size);

internal void init_buffered_socket(Buffered_Socket *buf_socket, Socket socket);
internal i64 buffered_socket_read(Buffered_Socket *buf_socket, u8 *dst, u64 dst_size);
internal void buffered_socket_receive_buffer(Buffered_Socket *buf_socket, u8 *buf, u64 receive_bytes);
internal String8 buffered_socket_receive_line(Memory_Arena *arena, Buffered_Socket *buf_socket, u64 max_line_size = kilobytes(4));


//~ NOTE(fakhri): HTTP stuff
internal void http_send_request(Socket_Handle socket, Http_Request request);
internal void http_receive_body(Memory_Arena *arena, Buffered_Socket *buf_socket, Http_Response *response);
internal void http_receive_response(Memory_Arena *arena, Buffered_Socket *buf_socket, Http_Response *response);


#endif //NETWORK_H
