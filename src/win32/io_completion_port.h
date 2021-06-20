#pragma once

#include <xrossfire/error.h>
#include <xrossfire/async.h>
#include <xrossfire/net.h>
#include <xrossfire/strbuf.h>

#define XF_IO_TYPE_HANDLE	1
#define XF_IO_TYPE_EXIT		2

#define XF_IO_SOCKET_CONNECT_PHASE1	0
#define XF_IO_SOCKET_CONNECT_PHASE2	1
#define XF_IO_SOCKET_DISCONNECT		2
#define XF_IO_SOCKET_RECEIVE		3
#define XF_IO_SOCKET_SEND			4
#define XF_IO_SERVER_SOCKET_ACCEPT	5

typedef struct xf_io_async
{
	union {
		OVERLAPPED overlapped;
		WSAOVERLAPPED wsa_overlapped;
	} head;
	HANDLE handle;
	int io_type;
	union {
		struct {
			PADDRINFOEXW addrs;
			xf_socket_t **self;
		} connect;
		struct {
			xf_socket_t *self;
		} disconnect;
		struct {
			xf_socket_t *self;
			int *transfered;
		} receive;
		struct {
			xf_socket_t *self;
			int *transfered;
		} send;
		struct {
			xf_socket_t **out_accepted_socket;
			xf_server_socket_t *self;
			SOCKET accepted_socket;
			void *buf;
			DWORD received_bytes;
		} accept;
	} context;
} xf_io_async_t;

XROSSFIRE_PRIVATE xf_error_t xf_io_completion_port_register(HANDLE handle);

XROSSFIRE_PRIVATE xf_io_async_t *xf_async_get_io_async(xf_async_t *self);
XROSSFIRE_PRIVATE void xf_io_async_clear(xf_io_async_t *self);
XROSSFIRE_PRIVATE bool xf_io_async_cancel(xf_io_async_t *self);

XROSSFIRE_PRIVATE void xf_io_completed_socket_connect_phase2(DWORD error, xf_io_async_t *io_async);
XROSSFIRE_PRIVATE void xf_io_completed_socket_disconnect(DWORD error, xf_io_async_t *io_async);
XROSSFIRE_PRIVATE void xf_io_completed_socket_receive(DWORD error, DWORD transfered, xf_io_async_t *io_async);
XROSSFIRE_PRIVATE void xf_io_completed_socket_send(DWORD error, DWORD transfered, xf_io_async_t *io_async);
XROSSFIRE_PRIVATE void xf_io_completed_server_socket_accept(DWORD error, xf_io_async_t *io_async);
