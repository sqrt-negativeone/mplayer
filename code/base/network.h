/* date = April 12th 2025 5:21 pm */

#ifndef NETWORK_H
#define NETWORK_H

#include "third_party/openssl/ssl.h"

struct Secured_Socket
{
	SSL *ssl;
	Socket_Handle s;
	b32 valid;
};

#define BUFFERED_SOCKET_CAPACITY 4096
struct Secured_Buffered_Socket
{
	Secured_Socket secured_socket;
	u64 availabe_bytes;
	u64 offset;
	u8 buf[BUFFERED_SOCKET_CAPACITY];
};

internal SSL_CTX *ssl_context_init();
internal Secured_Socket secure_socket_connect(SSL_CTX *ssl_ctx, const char *hostname);
internal b32 ssl_network_send_buffer(SSL *ssl, const void *data, u64 size);

internal void init_buffered_socket(Secured_Buffered_Socket *buf_socket, Secured_Socket secured_socket);
internal i64 buffered_socket_read(Secured_Buffered_Socket *buf_socket, u8 *dst, u64 dst_size);
internal void buffered_socket_receive_buffer(Secured_Buffered_Socket *buf_socket, u8 *buf, u64 receive_bytes);
internal String8 buffered_socket_receive_line(Memory_Arena *arena, Secured_Buffered_Socket *buf_socket, u64 max_line_size = kilobytes(4));

#endif //NETWORK_H
