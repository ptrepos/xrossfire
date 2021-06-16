#include <xrossfire/net.h>
#include <xrossfire/string.h>
#include "io_completion_port.h"

typedef struct xf_socket
{
	SOCKET handle;
    void *data;
} xf_socket_t;

static VOID WINAPI connect_phase1(
    _In_ DWORD Error,
    _In_ DWORD Bytes,
    _In_ LPOVERLAPPED Overlapped);
static xf_error_t socket_connect_async(
    xf_io_async_t *command,
    ADDRINFOEXW *addr);

XROSSFIRE_API xf_error_t xf_socket_new(xf_socket_t **self)
{
    xf_error_t err;
    xf_socket_t *obj = NULL;

    obj = (xf_socket_t *)malloc(sizeof(xf_socket_t));
    if (obj == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    obj->handle = INVALID_SOCKET;
    obj->data = NULL;

    *self = obj;

    return 0;
_ERROR:
    free(obj);

    return err;
}

XROSSFIRE_API void xf_socket_release(xf_socket_t *self)
{
    if (self == NULL)
        return;

    closesocket(self->handle);
    free(self);
}

XROSSFIRE_API void xf_socket_set_data(xf_socket_t *self, void *data)
{
    self->data = data;
}

XROSSFIRE_API void xf_socket_get_data(xf_socket_t *self, void **data)
{
    *data = self->data;
}

XROSSFIRE_API xf_error_t xf_socket_new_with_handle(SOCKET handle, xf_socket_t **self)
{
	xf_error_t err;
	xf_socket_t *obj = NULL;
	
	obj = (xf_socket_t*)malloc(sizeof(xf_socket_t));
	if (obj == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}
	
	obj->handle = handle;
    obj->data = NULL;
	
	*self = obj;
	
	return 0;
_ERROR:
    free(obj);
	
	return err;
}

XROSSFIRE_API void xf_socket_connect(
    xf_socket_t *self,
    xf_string_t *hostname,
    int port,
    int options,
    xf_async_t *async)
{
    xf_error_t err;
    ADDRINFOEX hints;
    xf_io_async_t *io_async = NULL;

    if (self->handle != INVALID_SOCKET) {
        err = XF_ERROR;
        goto _ERROR;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    io_async = xf_async_get_io_async(async);

    memset(io_async, 0, sizeof(xf_io_async_t));

    io_async->io_type = XF_IO_SOCKET_CONNECT_PHASE1;
	io_async->context.connect.self = self;

    INT ierr = GetAddrInfoExW(xf_string_to_cstr(hostname),
        NULL,
        NS_DNS,
        NULL,
        &hints,
        &io_async->context.connect.addrs,
        NULL,
        &io_async->head.overlapped,
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

	handle = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (handle == INVALID_SOCKET) {
		err = xf_error_windows(WSAGetLastError());
		goto _ERROR;
	}

	err = xf_io_completion_port_register((HANDLE)handle);
	if (err != 0)
		goto _ERROR;

	{
		struct sockaddr_in dummy_addr;
		ZeroMemory(&dummy_addr, sizeof(dummy_addr));
		dummy_addr.sin_family = addr->ai_family;
		dummy_addr.sin_addr.s_addr = INADDR_ANY;
		dummy_addr.sin_port = 0;
		iret = bind(handle, (SOCKADDR *)&dummy_addr, sizeof(dummy_addr));
		if (iret != 0) {
			err = xf_error_windows(WSAGetLastError());
			goto _ERROR;
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

	bret = connect_ex(handle, addr->ai_addr, (int)addr->ai_addrlen, NULL, 0, NULL, &io_async->head.overlapped);
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
        xf_io_async_clear(io_async);
        xf_async_notify((xf_async_t*)io_async, err);
        return;
    }

    io_async->context.connect.self->handle = (SOCKET)io_async->handle;
    io_async->handle = (HANDLE)INVALID_SOCKET;

    xf_async_notify((xf_async_t *)io_async, 0);
}

XROSSFIRE_API void xf_socket_disconnect(
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
        self->handle,
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

    bret = disconnect_ex(self->handle, &io_async->head.overlapped, 0, 0);
    if (bret == FALSE) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }

    self->handle = INVALID_SOCKET;

    return ;
_ERROR:
    xf_async_notify(async, err);
    return;
}

XROSSFIRE_PRIVATE void xf_io_completed_socket_disconnect(DWORD error, xf_io_async_t *io_async)
{
    xf_error_t err;

    closesocket(io_async->context.disconnect.self->handle);
    io_async->context.disconnect.self->handle = INVALID_SOCKET;

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify((xf_async_t*)io_async, err);
        return;
    }

    xf_async_notify((xf_async_t *)io_async, 0);
}

XROSSFIRE_API void xf_socket_receive(
    xf_socket_t *self,
    void *buffer,
    int length,
    /*out*/int *receive_length,
    xf_async_t *async)
{
    xf_error_t err;
    xf_io_async_t *io_async = xf_async_get_io_async(async);

    io_async->io_type = XF_IO_SOCKET_RECEIVE;
    io_async->handle = (HANDLE)self->handle;
    io_async->context.receive.transfered = receive_length;

    WSABUF buf;
    buf.buf = buffer;
    buf.len = length;

    DWORD flags = 0;
    int ret = WSARecv(self->handle, &buf, 1, NULL, &flags, &io_async->head.overlapped, NULL);
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
        xf_io_async_clear(io_async);
        xf_async_notify((xf_async_t *)io_async, err);
        return;
    }

    if (io_async->context.receive.transfered != NULL) {
        *io_async->context.receive.transfered = (int)transfered;
    }

    xf_io_async_clear(io_async);
    xf_async_notify((xf_async_t*)io_async, 0);
}

XROSSFIRE_API void xf_socket_send(
    xf_socket_t *self,
    void *buffer,
    int length,
    /*out*/int *send_length,
    xf_async_t *async)
{
    xf_error_t err;
    xf_io_async_t *io_async = xf_async_get_io_async(async);

    io_async->io_type = XF_IO_SOCKET_RECEIVE;
    io_async->handle = (HANDLE)self->handle;
    io_async->context.receive.transfered = send_length;

    WSABUF buf;
    buf.buf = buffer;
    buf.len = length;

    DWORD flags = 0;
    int ret = WSASend(self->handle, &buf, 1, NULL, flags, &io_async->head.overlapped, NULL);
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
        xf_io_async_clear(io_async);
        xf_async_notify((xf_async_t *)io_async, err);
        return;
    }

    if (io_async->context.receive.transfered != NULL) {
        *io_async->context.receive.transfered = (int)transfered;
    }

    xf_io_async_clear(io_async);
    xf_async_notify((xf_async_t *)io_async, 0);
}