#include <xrossfire/string.h>
#include <xrossfire/net.h>
#include "io_completion_port.h"

struct xf_server_socket
{
	SOCKET socket;
	int address_family;
	LPFN_ACCEPTEX accept;
	LPFN_GETACCEPTEXSOCKADDRS get_accept_socket_addr;
    void *data;
};

XROSSFIRE_PRIVATE bool xf_inet_parse(xf_string_t *s, int *ip_type, /*out*/struct in_addr *in_addr, /*out*/struct in6_addr *in6_addr);

#define RECEIVED_BUFFER_SIZE 	128

XROSSFIRE_API xf_error_t xf_server_socket_new(
    xf_string_t *host,
    int port,
    /*out*/xf_server_socket_t **self)
{
    xf_error_t err;
    SOCKET listenfd = INVALID_SOCKET;
    int ret;
    int yes = 1;
    int address_family = 0;
    xf_server_socket_t *server_socket = NULL;
    
    struct sockaddr_in saddr4;
    saddr4.sin_family = AF_INET;
    saddr4.sin_port = htons(port);

    struct sockaddr_in6 saddr6;
    saddr6.sin6_family = AF_INET6;
    saddr6.sin6_port = htons(port);

    int ip_type;
    if (xf_inet_parse(host, /*out*/&ip_type, /*out*/&saddr4.sin_addr, /*out*/&saddr6.sin6_addr) == false) {
        err = XF_ERROR;
        goto _ERROR;
    }
    switch (ip_type) {
    case XF_IPV4:
        address_family = AF_INET;

        listenfd = socket(address_family, SOCK_STREAM, 0);
        if (listenfd == INVALID_SOCKET) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }

        ret = setsockopt(listenfd,
            SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
        if (ret == SOCKET_ERROR) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }

        ret = bind(listenfd, (struct sockaddr *)&saddr4, sizeof(saddr4));
        if (ret == SOCKET_ERROR) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }
        break;
    case XF_IPV6:
        address_family = AF_INET6;

        listenfd = socket(address_family, SOCK_STREAM, 0);
        if (listenfd == INVALID_SOCKET) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }

        ret = setsockopt(listenfd,
            SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
        if (ret == SOCKET_ERROR) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }

        ret = bind(listenfd, (struct sockaddr *)&saddr6, sizeof(saddr6));
        if (ret == SOCKET_ERROR) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }
        break;
    default:
        xf_abort();
    }

    ret = listen(listenfd, 5);
    if (ret == SOCKET_ERROR) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

	server_socket = (xf_server_socket_t*)malloc(sizeof(xf_server_socket_t));
    if (server_socket == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    server_socket->socket = listenfd;
    server_socket->address_family = address_family;
    server_socket->data = NULL;

    DWORD numBytes = 0;
    GUID guid = WSAID_ACCEPTEX;
    ret = WSAIoctl(
        listenfd,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        (void *)&guid,
        sizeof(guid),
        (void *)&server_socket->accept,
        sizeof(server_socket->accept),
        &numBytes,
        NULL,
        NULL);
    if (ret) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

    numBytes = 0;
    GUID guid2 = WSAID_GETACCEPTEXSOCKADDRS;
    ret = WSAIoctl(
        listenfd,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        (void *)&guid2,
        sizeof(guid2),
        (void *)&server_socket->get_accept_socket_addr,
        sizeof(server_socket->get_accept_socket_addr),
        &numBytes,
        NULL,
        NULL);
    if (ret) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

    err = xf_io_completion_port_register((HANDLE)server_socket->socket);
    if (err != 0)
        goto _ERROR;
    
    return 0;
_ERROR:
    closesocket(listenfd);
    free(server_socket);

    return err;
}

XROSSFIRE_API void xf_server_socket_release(
    xf_server_socket_t *self)
{
    if (self == NULL)
        return;

    closesocket(self->socket);
    free(self);
}

XROSSFIRE_API void xf_server_socket_set_data(xf_server_socket_t *self, void *data)
{
    self->data = data;
}

XROSSFIRE_API void xf_server_socket_get_data(xf_server_socket_t *self, void **data)
{
    *data = self->data;
}

XROSSFIRE_API void xf_server_socket_accept(
	xf_server_socket_t *self,
	xf_socket_t **accepted_socket, 
	xf_async_t *async)
{
	xf_error_t err;
    BOOL ret;
    SOCKET accepted_sock = INVALID_SOCKET;
    xf_io_async_t *io_async = NULL;
	void *receive_buffer = NULL;

    accepted_sock = socket(self->address_family, SOCK_STREAM, 0);
    if (accepted_sock == INVALID_SOCKET) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

	io_async = xf_async_get_io_async(async);
	
    io_async->io_type = XF_IO_SERVER_SOCKET_ACCEPT;
    io_async->context.accept.self = self;
    io_async->context.accept.out_accepted_socket = accepted_socket;
    io_async->context.accept.accepted_socket = accepted_sock;
	
	receive_buffer = malloc(RECEIVED_BUFFER_SIZE);
	if (receive_buffer == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}
	
	io_async->context.accept.buf = receive_buffer;

    int addr_size = self->address_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    
    ret = self->accept(self->socket, accepted_sock, io_async->context.accept.buf, 0, addr_size, addr_size, NULL, &io_async->head.overlapped);
    if (ret == SOCKET_ERROR) {
        DWORD derr = WSAGetLastError();
        if (derr != WSA_IO_PENDING) {
            err = xf_error_windows(derr);
            goto _ERROR;
        }
    }

    return;
_ERROR:
    xf_io_async_clear(io_async);
	xf_async_notify(async, err);
}

XROSSFIRE_PRIVATE void xf_io_completed_server_socket_accept(DWORD error, xf_io_async_t *io_async)
{
    xf_error_t err;
	xf_async_t *async = (xf_async_t*)io_async;
	xf_socket_t *obj = NULL;
	
	if (error != 0) {
		err = xf_error_windows(error);
		xf_io_async_clear(io_async);
		xf_async_notify(async, err);
		return;
	}
	
	xf_server_socket_t *server_socket = io_async->context.accept.self;

    int addr_size = server_socket->address_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    SOCKADDR *local_addr, *remote_addr;
    int local_addr_len, remote_addr_len;
    server_socket->get_accept_socket_addr(
        io_async->context.accept.buf, 
        0,
        addr_size,
    	addr_size,
    	&local_addr,
    	&local_addr_len,
    	&remote_addr,
    	&remote_addr_len
        );

	err = xf_socket_new_with_handle(io_async->context.accept.accepted_socket, io_async->context.accept.out_accepted_socket);
	if (err != 0)
		goto _ERROR;
	io_async->context.accept.accepted_socket = INVALID_SOCKET;
	
	xf_io_async_clear(io_async);
	xf_async_notify(async, 0);
	return ;
_ERROR:
	xf_io_async_clear(io_async);
	xf_async_notify(async, err);
	return ;
}
