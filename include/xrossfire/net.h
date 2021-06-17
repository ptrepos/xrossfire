#pragma once

#include <xrossfire/error.h>
#include <xrossfire/async.h>

typedef struct xf_tcp_socket	xf_tcp_socket_t;
typedef struct xf_tcp_server	xf_tcp_server_t;

#define XF_IPV4	(1)
#define XF_IPV6	(2)

XROSSFIRE_API xf_error_t xf_tcp_server_new(xf_string_t *ip_address, int port, xf_tcp_server_t **self);
XROSSFIRE_API void xf_tcp_server_release(xf_tcp_server_t *self);

XROSSFIRE_API void xf_tcp_server_accept(
	xf_tcp_server_t *self,
	xf_tcp_socket_t **socket, 
	xf_async_t *async);
XROSSFIRE_API void xf_tcp_server_set_data(xf_tcp_server_t *self, void *data);
XROSSFIRE_API void xf_tcp_server_get_data(xf_tcp_server_t *self, void **data);

XROSSFIRE_API xf_error_t xf_tcp_socket_new(xf_tcp_socket_t **self);
XROSSFIRE_API void xf_tcp_socket_release(xf_tcp_socket_t *self);

#if defined(_WIN32)
XROSSFIRE_API xf_error_t xf_tcp_socket_new_with_handle(SOCKET handle, xf_tcp_socket_t **self);
#endif

XROSSFIRE_API void xf_tcp_socket_connect(
	xf_tcp_socket_t *self,
	xf_string_t *hostname,
	int port,
	int options,
	xf_async_t *async);
XROSSFIRE_API void xf_tcp_socket_disconnect(
	xf_tcp_socket_t *self,
	xf_async_t *async);

XROSSFIRE_API void xf_tcp_socket_set_data(xf_tcp_socket_t *self, void *data);
XROSSFIRE_API void xf_tcp_socket_get_data(xf_tcp_socket_t *self, void **data);

XROSSFIRE_API void xf_tcp_socket_receive(
	xf_tcp_socket_t *self, 
	void *buffer, 
	int length, 
	/*out*/int *receive_length,
	xf_async_t *async);
XROSSFIRE_API void xf_tcp_socket_send(
	xf_tcp_socket_t *self, 
	void *buffer, 
	int length, 
	/*out*/int *send_length,
	xf_async_t *async);
