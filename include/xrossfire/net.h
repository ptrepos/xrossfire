#pragma once

#include <xrossfire/error.h>
#include <xrossfire/async.h>

typedef struct xf_socket	xf_socket_t;
typedef struct xf_server_socket	xf_server_socket_t;

XROSSFIRE_API xf_error_t xf_server_socket_new(xf_string_t *ip_address, short port, xf_server_socket_t **self);
XROSSFIRE_API xf_error_t xf_server_socket_release(xf_server_socket_t *self);

XROSSFIRE_API xf_error_t xf_server_socket_accept(
	xf_server_socket_t *self,
	xf_socket_t **socket, 
	xf_async_t *async);
XROSSFIRE_API xf_error_t xf_server_socket_set_data(xf_server_socket_t *self, void *data);
XROSSFIRE_API xf_error_t xf_server_socket_get_data(xf_server_socket_t *self, void **data);

XROSSFIRE_API xf_error_t xf_socket_new(xf_socket_t **self);
XROSSFIRE_API xf_error_t xf_socket_release(xf_socket_t *self);

XROSSFIRE_API xf_error_t xf_socket_connect(
	xf_socket_t *self,
	xf_string_t *hostname,
	int port,
	xf_async_t *async);
XROSSFIRE_API xf_error_t xf_socket_disconnect(
	xf_socket_t *self,
	xf_async_t *async);

XROSSFIRE_API xf_error_t xf_socket_set_data(xf_socket_t *self, void *data);
XROSSFIRE_API xf_error_t xf_socket_get_data(xf_socket_t *self, void **data);

XROSSFIRE_API xf_error_t xf_socket_receive(
	xf_socket_t *self, 
	void *buffer, 
	int length, 
	/*out*/int *receive_length,
	xf_async_t *async);
XROSSFIRE_API xf_error_t xf_socket_send(
	xf_socket_t *self, 
	void *buffer, 
	int length, 
	/*out*/int *send_length,
	xf_async_t *async);
