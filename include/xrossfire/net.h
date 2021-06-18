#pragma once

#include <xrossfire/error.h>
#include <xrossfire/async.h>
#include <xrossfire/object.h>

#define XF_IPV4	(1)
#define XF_IPV6	(2)

typedef xf_object_t		xf_socket_t;
typedef xf_object_t		xf_server_socket_t;

#define XF_MESSAGE_SOCKET_CLOSE				XF_MESSAGE(XF_INTERFACE_SOCKET, 1)
#define XF_MESSAGE_SOCKET_RECEIVE			XF_MESSAGE(XF_INTERFACE_SOCKET, 2)
#define XF_MESSAGE_SOCKET_SEND				XF_MESSAGE(XF_INTERFACE_SOCKET, 3)

#define XF_MESSAGE_SERVER_SOCKET_ACCEPT		XF_MESSAGE(XF_INTERFACE_SERVER_SOCKET, 1)

typedef struct xf_socket_close_args
{
	xf_async_t *async;
} xf_socket_close_args_t;

typedef struct xf_socket_receive_args
{
	byte *buffer;
	int length;
	int *receive_length;
	xf_async_t *async;
} xf_socket_receive_args_t;

typedef struct xf_socket_send_args
{
	byte *buffer;
	int length;
	int *send_length;
	xf_async_t *async;
} xf_socket_send_args_t;

typedef struct xf_server_socket_accept_args
{
	xf_socket_t **accepted_socket;
	xf_async_t *async;
} xf_server_socket_accept_args_t;

XROSSFIRE_API xf_error_t xf_server_socket_new(xf_string_t *ip_address, int port, xf_server_socket_t **self);
XROSSFIRE_API xf_error_t xf_ssl_server_socket_new(xf_string_t *ip_address, int port, xf_server_socket_t **self);
XROSSFIRE_API void xf_server_socket_release(xf_server_socket_t *self);

XROSSFIRE_API void xf_server_socket_accept(
	xf_server_socket_t *self,
	xf_socket_t **socket, 
	xf_async_t *async);

XROSSFIRE_API xf_error_t xf_tcp_socket_new(
	xf_string_t *hostname,
	int port,
	int options, 
	xf_socket_t **self,
	xf_async_t *async);
XROSSFIRE_API xf_error_t xf_ssl_socket_new(
	xf_string_t *hostname,
	int port,
	int options,
	xf_socket_t **self,
	xf_async_t *async);
XROSSFIRE_API void xf_socket_release(xf_socket_t *self);

#if defined(_WIN32)
XROSSFIRE_API xf_error_t xf_tcp_socket_new_with_handle(SOCKET handle, xf_socket_t **self);
#endif

XROSSFIRE_API void xf_socket_close(
	xf_socket_t *self,
	xf_async_t *async);

XROSSFIRE_API void xf_socket_receive(
	xf_socket_t *self, 
	void *buffer, 
	int length, 
	/*out*/int *receive_length,
	xf_async_t *async);
XROSSFIRE_API void xf_socket_send(
	xf_socket_t *self, 
	void *buffer, 
	int length, 
	/*out*/int *send_length,
	xf_async_t *async);
