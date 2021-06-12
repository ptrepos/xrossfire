#include <xrossfire/net.h>
#include "io_completion_port.h"

typedef struct xf_socket
{
	SOCKET handle;
} xf_socket_t;

static void xf_io_command_free(xf_io_command_t *command)
{
    if (command == NULL)
        return;

    switch (command->io_type) {
    case XF_IO_SOCKET_CONNECT_PHASE1:
        xf_strbuf16_destroy(&command->context.connect.hostname);
        FreeAddrInfo(&command->context.connect.addrs);
        closesocket(command->handle);
        break;
    case XF_IO_SOCKET_CONNECT_PHASE2:
        xf_strbuf16_destroy(&command->context.connect.hostname);
        FreeAddrInfo(&command->context.connect.addrs);
        closesocket(command->handle);
        break;
    }

    free(command);
}

static VOID WINAPI connect_phase1(
    _In_ DWORD Error,
    _In_ DWORD Bytes,
    _In_ LPOVERLAPPED Overlapped);
static xf_error_t socket_connect_async(
    xf_io_command_t *command,
    ADDRINFOA *addr);

XROSSFIRE_API xf_error_t xf_socket_connect(
	xf_socket_t *self,
	xf_string_t *hostname,
	int port,
	xf_async_t *async)
{
    xf_error_t err;
    ADDRINFOEX hints;
    xf_io_command_t *command = NULL;

    if (self->handle != INVALID_SOCKET) {
        err = XF_ERROR;
        goto _ERROR;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    command = (xf_io_command_t *)malloc(sizeof(xf_io_command_t));
    if (command == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    memset(command, 0, sizeof(xf_io_command_t));

    command->io_type = XF_IO_SOCKET_CONNECT_PHASE1;
    command->async = async;
    xf_strbuf16_init(&command->context.connect.hostname);
	command->context.connect.self = self;

	xf_async_set_command(async, command);

    err = xf_string_to_unicode(hostname, &command->context.connect.hostname);
    if (err != 0)
        goto _ERROR;

    INT ierr = GetAddrInfoExW(xf_strbuf16_to_cstr(&command->context.connect.hostname),
        NULL,
        NS_DNS,
        NULL,
        &hints,
        &command->context.connect.addrs,
        NULL,
        &command->head.overlapped,
        connect_phase1,
        &command->handle);
    if (ierr != WSA_IO_PENDING)
    {
        connect_phase1(ierr, 0, &command->head.overlapped);
        xf_io_command_free(command);
        goto _SUCCESS;
    }
    
_SUCCESS:
    return 0;
_ERROR:
    xf_io_command_free(command);
    return err;
}

static VOID WINAPI connect_phase1(
    _In_ DWORD Error,
    _In_ DWORD Bytes,
    _In_ LPOVERLAPPED Overlapped)
{
    xf_error_t err;
    xf_io_command_t *command = (xf_io_command_t*)Overlapped;

    if (Error != 0) {
        err = xf_error_windows(Error);
        xf_async_notify(command->async, err);
        xf_io_command_free(command);
        return;
    }

    err = socket_connect_async(command, command->context.connect.addrs);
    if (err != 0) {
        xf_async_notify(command->async, err);
        xf_io_command_free(command);
        return;
    }
}

static xf_error_t socket_connect_async(
	xf_io_command_t *command,
	ADDRINFOA *addr)
{
	xf_error_t err;
	int iret;
	BOOL bret;
	SOCKET handle = INVALID_SOCKET;
	xf_io_command_t *command = NULL;

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

    command->io_type = XF_IO_SOCKET_CONNECT_PHASE2;

	bret = connect_ex(handle, addr->ai_addr, (int)addr->ai_addrlen, NULL, 0, NULL, &command->head.overlapped);
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

XROSSFIRE_PRIVATE void xf_io_completed_socket_connect_phase2(DWORD error, xf_io_command_t *command)
{
    xf_error_t err;

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify(command->async, err);
        xf_io_command_free(command);
        return;
    }

    command->context.connect.self->handle = command->handle;
    command->handle = INVALID_SOCKET;

    xf_async_notify(command->async, 0);

    xf_io_command_free(command);
}

XROSSFIRE_API xf_error_t xf_socket_disconnect(
    xf_socket_t *self,
    xf_async_t *async)
{
    xf_error_t err;
    int iret;
    BOOL bret;
    xf_io_command_t *command = NULL;

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

    command = (xf_io_command_t*)malloc(sizeof(xf_io_command_t));
    if (command != NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    command->io_type = XF_IO_SOCKET_DISCONNECT;
    command->async = async;
    command->context.disconnect.self = self;

    bret = disconnect_ex(self->handle, &command->head.overlapped, 0, 0);
    if (bret == FALSE) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }

    self->handle = INVALID_SOCKET;

    return 0;
_ERROR:
    return err;
}

XROSSFIRE_PRIVATE void xf_io_completed_socket_disconnect(DWORD error, xf_io_command_t *command)
{
    xf_error_t err;

    closesocket(command->context.disconnect.self->handle);
    command->context.disconnect.self->handle = INVALID_SOCKET;

    if (error != 0) {
        err = xf_error_windows(error);
        xf_async_notify(command->async, err);
        xf_io_command_free(command);
        return;
    }

    xf_async_notify(command->async, 0);
    xf_io_command_free(command);
}
