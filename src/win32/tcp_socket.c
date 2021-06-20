#include <xrossfire/net.h>
#include "io_completion_port.h"

typedef struct xf_tcp_socket_data
{
    xf_monitor_t lock;
	SOCKET handle;
} xf_tcp_socket_data_t;

static void xf_socket_connect_tcp(
    xf_socket_t *self,
    xf_string_t *hostname,
    int port,
    int options,
    xf_async_t *async);
static VOID WINAPI connect_phase1(
    _In_ DWORD Error,
    _In_ DWORD Bytes,
    _In_ LPOVERLAPPED Overlapped);
static xf_error_t socket_connect_async(
    xf_io_async_t *command,
    ADDRINFOEXW *addr);

#define GET_HANDLE(_socket) ((xf_tcp_socket_data_t*)xf_object_get_body(_socket))->handle
#define SET_HANDLE(_socket, _handle) ((xf_tcp_socket_data_t*)xf_object_get_body(_socket))->handle = _handle

XROSSFIRE_API xf_error_t xf_tcp_socket_procedure(xf_object_t *self, int message_id, void *args);

XROSSFIRE_API void xf_tcp_socket_new(
    xf_string_t *hostname,
    int port,
    int options,
    xf_socket_t **self,
    xf_async_t *async)
{
    xf_error_t err;
    xf_socket_t *obj = NULL;

    err = xf_object_new(xf_tcp_socket_procedure, sizeof(xf_tcp_socket_data_t), &obj);
    if (obj == NULL) {
        goto _ERROR;
    }

    xf_tcp_socket_data_t *data = (xf_tcp_socket_data_t *)xf_object_get_body(obj);
    xf_monitor_init(&data->lock);
    data->handle = INVALID_SOCKET;

    *self = obj;

    xf_socket_connect_tcp(obj, hostname, port, options, async);

    return ;
_ERROR:
    xf_object_release(obj);
    xf_async_notify(async, err);
}

XROSSFIRE_API xf_error_t xf_tcp_socket_new_with_handle(SOCKET handle, xf_socket_t **self)
{
    xf_error_t err;
    xf_socket_t *obj = NULL;

    err = xf_io_completion_port_register((HANDLE)handle);
    if (err != 0)
        goto _ERROR;

    err = xf_object_new(xf_tcp_socket_procedure, sizeof(xf_tcp_socket_data_t), &obj);
    if (obj == NULL) {
        goto _ERROR;
    }

    xf_tcp_socket_data_t *data = (xf_tcp_socket_data_t *)xf_object_get_body(obj);
    xf_monitor_init(&data->lock);
    data->handle = handle;

    *self = obj;

    return 0;
_ERROR:
    xf_object_release(obj);

    return err;
}

XROSSFIRE_API void xf_socket_release(xf_socket_t *self)
{
    xf_object_release(self);
}

static void xf_socket_connect_tcp(
    xf_socket_t *self,
    xf_string_t *hostname,
    int port,
    int options,
    xf_async_t *async)
{
    xf_error_t err;
    ADDRINFOEX hints;
    WCHAR port_buf[16];
    xf_io_async_t *io_async = NULL;
    xf_tcp_socket_data_t *data = (xf_tcp_socket_data_t*)xf_object_get_body(self);

    if (data->handle != INVALID_SOCKET) {
        err = XF_ERROR;
        goto _ERROR;
    }

    wsprintfW(port_buf, L"%d", port);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    io_async = xf_async_get_io_async(async);

    memset(io_async, 0, sizeof(xf_io_async_t));

    io_async->io_type = XF_IO_SOCKET_CONNECT_PHASE1;
	io_async->context.connect.self = self;

    memset(&io_async->head.wsa_overlapped, 0, sizeof(io_async->head.wsa_overlapped));
    INT ierr = GetAddrInfoExW(xf_string_to_cstr(hostname),
        port_buf,
        NS_DNS,
        NULL,
        &hints,
        &io_async->context.connect.addrs,
        NULL,
        &io_async->head.wsa_overlapped,
        connect_phase1,
        &io_async->handle);
    if (ierr != WSA_IO_PENDING)
    {
        connect_phase1(ierr, 0, &io_async->head.overlapped);
        goto _SUCCESS;
    }
    
_SUCCESS:
    return ;
_ERROR:
    xf_async_notify(async, err);
    return ;
}

static VOID WINAPI connect_phase1(
    _In_ DWORD Error,
    _In_ DWORD Bytes,
    _In_ LPOVERLAPPED Overlapped)
{
    xf_error_t err;
    xf_io_async_t *io_async = (xf_io_async_t*)Overlapped;

    io_async->handle = (HANDLE)INVALID_SOCKET;

    if (Error != 0) {
        err = xf_error_windows(Error);
        xf_async_notify((xf_async_t*)io_async, err);
        return;
    }

    err = socket_connect_async(io_async, io_async->context.connect.addrs);
    if (err != 0) {
        xf_async_notify((xf_async_t*)io_async, err);
        return;
    }
}

static xf_error_t socket_connect_async(
	xf_io_async_t *io_async,
	ADDRINFOEXW *addr)
{
	xf_error_t err;
	int iret;
	BOOL bret;
	SOCKET handle = INVALID_SOCKET;

    while (addr != NULL && addr->ai_family != AF_INET)
        addr = addr->ai_next;
    if (addr == NULL) {
        err = XF_ERROR;
		goto _ERROR;
    }

	handle = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (handle == INVALID_SOCKET) {
		err = xf_error_windows(WSAGetLastError());
		goto _ERROR;
	}

	err = xf_io_completion_port_register((HANDLE)handle);
	if (err != 0)
		goto _ERROR;

	{
        if (addr->ai_family == AF_INET) {
            struct sockaddr_in dummy_addr;
            ZeroMemory(&dummy_addr, sizeof(dummy_addr));
            dummy_addr.sin_family = AF_INET;
            dummy_addr.sin_addr.s_addr = INADDR_ANY;
            dummy_addr.sin_port = 0;
            iret = bind(handle, (SOCKADDR *)&dummy_addr, sizeof(dummy_addr));
            if (iret != 0) {
                err = xf_error_windows(WSAGetLastError());
                goto _ERROR;
            }
        } else if (addr->ai_family == AF_INET6) {
            struct in6_addr addr = IN6ADDR_ANY_INIT;
            struct sockaddr_in6 dummy_addr;
            ZeroMemory(&dummy_addr, sizeof(dummy_addr));
            dummy_addr.sin6_family = AF_INET6;
            dummy_addr.sin6_addr = addr;
            dummy_addr.sin6_port = 0;
            iret = bind(handle, (SOCKADDR *)&dummy_addr, sizeof(dummy_addr));
        }
	}

	DWORD numBytes = 0;
	GUID guid = WSAID_CONNECTEX;
	LPFN_CONNECTEX connect_ex = NULL;
	iret = WSAIoctl(
		handle,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void *)&guid,
		sizeof(guid),
		(void *)&connect_ex,
		sizeof(connect_ex),
		&numBytes,
		NULL,
		NULL);
	if (iret) {
		err = xf_error_windows(WSAGetLastError());
		goto _ERROR;
	}

    io_async->io_type = XF_IO_SOCKET_CONNECT_PHASE2;
    io_async->handle = (HANDLE)handle;

    memset(&io_async->head.wsa_overlapped, 0, sizeof(io_async->head.wsa_overlapped));

	bret = connect_ex(handle, addr->ai_addr, (int)addr->ai_addrlen, NULL, 0, NULL, &io_async->head.wsa_overlapped);
	if (!bret) {
		DWORD derr = WSAGetLastError();
		if (derr != WSA_IO_PENDING) {
			err = xf_error_windows(WSAGetLastError());
			goto _ERROR;
		}
	}

	return 0;
_ERROR:
	closesocket(handle);

	return err;
}

XROSSFIRE_PRIVATE void xf_io_completed_socket_connect_phase2(DWORD error, xf_io_async_t *io_async)
{
    xf_error_t err;

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify((xf_async_t*)io_async, err);
        return;
    }

    xf_tcp_socket_data_t *data = (xf_tcp_socket_data_t *)xf_object_get_body(io_async->context.connect.self);

    data->handle = (SOCKET)io_async->handle;
    io_async->context.connect.self = NULL;
    io_async->handle = (HANDLE)INVALID_SOCKET;

    xf_async_notify((xf_async_t *)io_async, 0);
}

static void __xf_socket_close(
    xf_socket_t *self,
    xf_async_t *async)
{
    xf_error_t err;
    int iret;
    BOOL bret;
    xf_io_async_t *io_async;

    DWORD numBytes = 0;
    GUID guid = WSAID_DISCONNECTEX;
    LPFN_DISCONNECTEX disconnect_ex = NULL;
    iret = WSAIoctl(
        GET_HANDLE(self),
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        (void *)&guid,
        sizeof(guid),
        (void *)&disconnect_ex,
        sizeof(disconnect_ex),
        &numBytes,
        NULL,
        NULL);
    if (iret) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

    io_async = xf_async_get_io_async(async);

    io_async->io_type = XF_IO_SOCKET_DISCONNECT;
    io_async->context.disconnect.self = self;

    xf_tcp_socket_data_t *body = (xf_tcp_socket_data_t *)xf_object_get_body(self);

    memset(&io_async->head.wsa_overlapped, 0, sizeof(io_async->head.wsa_overlapped));
    xf_monitor_enter(&body->lock);
    bret = disconnect_ex(body->handle, &io_async->head.wsa_overlapped, 0, 0);
    xf_monitor_leave(&body->lock);
    if (bret == FALSE) {
        DWORD derr = WSAGetLastError();
        if (derr != ERROR_IO_PENDING) {
            err = xf_error_windows(GetLastError());
            goto _ERROR;
        }
    }

    return ;
_ERROR:
    xf_async_notify(async, err);
    return;
}

XROSSFIRE_PRIVATE void xf_io_completed_socket_disconnect(DWORD error, xf_io_async_t *io_async)
{
    xf_error_t err;

    xf_tcp_socket_data_t *body = (xf_tcp_socket_data_t *)xf_object_get_body(io_async->context.disconnect.self);

    xf_monitor_enter(&body->lock);
    closesocket(body->handle);
    xf_monitor_leave(&body->lock);

    body->handle = INVALID_SOCKET;

    xf_monitor_destroy(&body->lock);

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify((xf_async_t*)io_async, err);
        return;
    }

    xf_async_notify((xf_async_t *)io_async, 0);
}

static void __xf_socket_receive(
    xf_socket_t *self,
    void *buffer,
    int length,
    /*out*/int *receive_length,
    xf_async_t *async)
{
    xf_error_t err;
    xf_io_async_t *io_async = xf_async_get_io_async(async);

    io_async->io_type = XF_IO_SOCKET_RECEIVE;
    io_async->handle = (HANDLE)GET_HANDLE(self);
    io_async->context.receive.transfered = receive_length;

    WSABUF buf;
    buf.buf = buffer;
    buf.len = length;

    xf_tcp_socket_data_t *body = (xf_tcp_socket_data_t *)xf_object_get_body(self);

    memset(&io_async->head.wsa_overlapped, 0, sizeof(io_async->head.wsa_overlapped));
    DWORD flags = 0;

    xf_monitor_enter(&body->lock);
    int ret = WSARecv(GET_HANDLE(self), &buf, 1, NULL, &flags, &io_async->head.wsa_overlapped, NULL);
    xf_monitor_leave(&body->lock);

    if (ret == SOCKET_ERROR) {
        DWORD derr = WSAGetLastError();
        if (derr != WSA_IO_PENDING) {
            err = xf_error_windows(derr);
            goto _ERROR;
        }
    }

    return;
_ERROR:
    xf_async_notify(async, err);
}

XROSSFIRE_PRIVATE void xf_io_completed_socket_receive(DWORD error, DWORD transfered, xf_io_async_t *io_async)
{
    xf_error_t err;

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify((xf_async_t *)io_async, err);
        return;
    }

    if (io_async->context.receive.transfered != NULL) {
        *io_async->context.receive.transfered = (int)transfered;
    }

    xf_async_notify((xf_async_t*)io_async, 0);
}

static void __xf_socket_send(
    xf_socket_t *self,
    void *buffer,
    int length,
    xf_async_t *async)
{
    xf_error_t err;
    xf_io_async_t *io_async = xf_async_get_io_async(async);

    io_async->io_type = XF_IO_SOCKET_SEND;
    io_async->handle = (HANDLE)GET_HANDLE(self);

    WSABUF buf;
    buf.buf = buffer;
    buf.len = length;

    xf_tcp_socket_data_t *body = (xf_tcp_socket_data_t *)xf_object_get_body(self);

    memset(&io_async->head.wsa_overlapped, 0, sizeof(io_async->head.wsa_overlapped));
    DWORD flags = 0;

    xf_monitor_enter(&body->lock);
    int ret = WSASend(body->handle, &buf, 1, NULL, flags, &io_async->head.overlapped, NULL);
    xf_monitor_leave(&body->lock);

    if (ret == SOCKET_ERROR) {
        DWORD derr = WSAGetLastError();
        if (derr != WSA_IO_PENDING) {
            err = xf_error_windows(derr);
            goto _ERROR;
        }
    }

    return;
_ERROR:
    xf_async_notify(async, err);
}

XROSSFIRE_PRIVATE void xf_io_completed_socket_send(DWORD error, DWORD transfered, xf_io_async_t *io_async)
{
    xf_error_t err;

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify((xf_async_t *)io_async, err);
        return;
    }

    xf_async_notify((xf_async_t *)io_async, 0);
}

static xf_string_t CLASS_ID = XF_STRING_INITIALIZER(_T("xrossfire.net.TcpSocket"));

XROSSFIRE_API xf_error_t xf_tcp_socket_procedure(xf_object_t *self, int message_id, void *args)
{
    xf_error_t err;
    xf_async_t *async = NULL;
    xf_tcp_socket_data_t *data = (xf_tcp_socket_data_t *)xf_object_get_body(self);

    switch (message_id) {
    case XF_MESSAGE_OBJECT_QUERY_INTERFACE:
        {
            xf_object_query_interface_args_t *_args = (xf_object_query_interface_args_t *)args;
            *_args->provides = 
                _args->interface_type == XF_INTERFACE_UNKNOWN || 
                _args->interface_type == XF_INTERFACE_SOCKET;
            return 0;
        }
    case XF_MESSAGE_OBJECT_GET_CLASS_ID:
        {
            xf_object_get_class_id_args_t *_args = (xf_object_get_class_id_args_t *)args;
            *_args->id = &CLASS_ID;
            return 0;
        }
    case XF_MESSAGE_OBJECT_DESTROY:
        {
            if (data->handle != INVALID_SOCKET) {
                err = xf_async_wait_new(10*1000, &async);
                if (err != 0)
                    return 0;

                __xf_socket_close(self, async);

                xf_async_wait(async);
            }

            return 0;
        }
    case XF_MESSAGE_SOCKET_CLOSE:
        {
            xf_socket_close_args_t *_args = (xf_socket_close_args_t *)args;
            __xf_socket_close(self, _args->async);
            return 0;
        }
    case XF_MESSAGE_SOCKET_RECEIVE:
        {
            xf_socket_receive_args_t *_args = (xf_socket_receive_args_t *)args;
            __xf_socket_receive(self, _args->buffer, _args->length, _args->receive_length, _args->async);
            return 0;
        }
    case XF_MESSAGE_SOCKET_SEND:
        {
            xf_socket_send_args_t *_args = (xf_socket_send_args_t *)args;
            __xf_socket_send(self, _args->buffer, _args->length, _args->async);
            return 0;
        }
    }

    return xf_object_default_procedure(self, message_id, args);
}
