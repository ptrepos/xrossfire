#pragma once

#include <xrossfire/error.h>
#include <xrossfire/async.h>
#include <xrossfire/net.h>
#include <xrossfire/strbuf.h>

#define XF_IO_TYPE_HANDLE	1
#define XF_IO_TYPE_EXIT		2

#define XF_IO_SOCKET_CONNECT_PHASE1	0
#define XF_IO_SOCKET_CONNECT_PHASE2	1
#define XF_IO_SOCKET_DISCONNECT	2
#define XF_IO_SOCKET_RECEIVE	3
#define XF_IO_SOCKET_SEND		4

typedef struct xf_io_command
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
			xf_socket_t *self;
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
	} context;
	xf_async_t *async;
} xf_io_command_t;

XROSSFIRE_PRIVATE xf_error_t xf_io_completion_port_register(HANDLE handle);

XROSSFIRE_PRIVATE void xf_async_set_command(xf_async_t *self, xf_io_command_t *command);

XROSSFIRE_PRIVATE void xf_io_command_cancel(xf_io_command_t *self);

XROSSFIRE_PRIVATE void xf_io_completed_socket_connect_phase2(DWORD error, xf_io_command_t *command);
XROSSFIRE_PRIVATE void xf_io_completed_socket_disconnect(DWORD error, xf_io_command_t *command);
XROSSFIRE_PRIVATE void xf_io_completed_socket_receive(DWORD error, xf_io_command_t *command);
XROSSFIRE_PRIVATE void xf_io_completed_socket_send(DWORD error, xf_io_command_t *command);
