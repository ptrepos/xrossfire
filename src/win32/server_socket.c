#include <xrossfire/string.h>
#include <xrossfire/socket.h>

struct xf_server_socket
{
	SOCKET socket;
	int address_family;
	LPFN_ACCEPTEX accept;
	LPFN_GETACCEPTEXSOCKADDRS get_accept_socket_addr;
};

#define RECEIVED_BUFFER_SIZE 	128

XROSSFIRE_API xf_error_t xf_server_socket_new(
    const char *host,
    int port,
    /*out*/xf_server_socket_t **self)
{
    xf_error_t err;
    SOCKET listenfd = INVALID_SOCKET;
    int ret;
    int yes = 1;
    int address_family = 0;
    xf_server_socket_t *sock = NULL;
    
    struct sockaddr_in saddr4;
    saddr4.sin_family = AF_INET;
    saddr4.sin_port = htons(port);

    struct sockaddr_in6 saddr6;
    saddr6.sin6_family = AF_INET6;
    saddr6.sin6_port = htons(port);

    ret = parse_ip_address(host, /*out*/&saddr4.sin_addr, /*out*/&saddr6.sin6_addr);
    switch (ret) {
    case IPV4:
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

        ret = bind(listenfd, (struct sock_addr *) & saddr4, sizeof(saddr4));
        if (ret == SOCKET_ERROR) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }
        break;
    case IPV6:
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

        ret = bind(listenfd, (struct sock_addr *) & saddr6, sizeof(saddr6));
        if (ret == SOCKET_ERROR) {
            err = xf_error_windows(WSAGetLastError());
            goto _ERROR;
        }
        break;
    default:
        err = XF_ERROR;
        goto _ERROR;
    }

    ret = listen(listenfd, 5);
    if (ret == SOCKET_ERROR) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

	sock = (xf_server_socket_t*)malloc(sizeof(xf_server_socket_t));
    if (sock == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    sock->socket = listenfd;
    sock->address_family = address_family;

    DWORD numBytes = 0;
    GUID guid = WSAID_ACCEPTEX;
    ret = WSAIoctl(
        sock,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        (void *)&guid,
        sizeof(guid),
        (void *)&sock->accept,
        sizeof(sock->accept),
        &numBytes,
        NULL,
        NULL);
    if (ret) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

    DWORD numBytes = 0;
    GUID guid2 = WSAID_GETACCEPTEXSOCKADDRS;
    ret = WSAIoctl(
        sock,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        (void *)&guid2,
        sizeof(guid2),
        (void *)&sock->get_accept_socket_addr,
        sizeof(sock->get_accept_socket_addr),
        &numBytes,
        NULL,
        NULL);
    if (ret) {
        err = xf_error_windows(WSAGetLastError());
        goto _ERROR;
    }

    err = xf_io_completion_port_register((HANDLE)sock->socket);
    if (err != 0)
        goto _ERROR;
    
    return 0;
_ERROR:
    closesocket(listenfd);
    free(sock);

    return err;
}

XROSSFIRE_API xf_error_t xf_server_socket_release(
    xf_server_socket_t *self)
{
    if (self == NULL)
        return;

    closesocket(self->socket);
    free(self);
}

XROSSFIRE_API void xf_server_socket_accept(
	xf_server_socket_t *self,
	xf_socket_t **socket, 
	xf_async_t *async)
{
	xf_error_t err;
    BOOL ret;
    SOCKET *accepted_sock = INVALID_SOCKET;
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
    io_async->context.accept.accepted_socket = accepted_socket;
	
	receive_buffer = malloc(RECEIVED_BUFFER_SIZE);
	if (receive_buffer == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}
	
	io_async->context.accept.buf = receive_buffer;

    int addr_size = self->address_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    
    ret = self->accept(self->socket, accepted_sock, io_async->context.accept.buf, 0, addr_size, addr_size, NULL, &io_async->base.overlapped);
    if (ret == SOCKET_ERROR) {
        DWORD derr = WSAGetLastError();
        if (derr != WSA_IO_PENDING) {
            err = xf_error_windows(derr);
            goto _ERROR;
        }
    }

    return 0;
_ERROR:
    xf_io_async_clear(io_async);
	xf_async_notify(async, err);
}

XROSSFIRE_PRIVATE void xf_io_completed_server_socket_accept(DWORD error, xf_io_async_t *io_async)
{
	xf_async_t *async = (xf_async_t*)io_async;
	xf_socket_t *obj = NULL;
	
	xf_async_cancel_timeout(async);
	
	if (error != 0) {
		err = xf_error_windows(error);
		xf_io_async_clear(io_async);
		xf_async_notify(async, err);
		return;
	}
	
	xf_server_socket_t *server_socket = io_async->context.accept.self;

    int addr_size = server_socket->address_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    SOCKADDR_IN *local_addr, *remote_addr;
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
