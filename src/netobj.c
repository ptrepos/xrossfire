#include <xrossfire/net.h>

XROSSFIRE_API void xf_server_socket_accept(
	xf_server_socket_t *self,
	xf_socket_t **accepted_socket,
	xf_async_t *async)
{
	xf_server_socket_accept_args_t args;
	args.accepted_socket = accepted_socket;
	args.async = async;

	xf_object_call(self, XF_MESSAGE_SERVER_SOCKET_ACCEPT, &args);
}

XROSSFIRE_API void xf_socket_close(
	xf_socket_t *self,
	xf_async_t *async)
{
	xf_socket_close_args_t args;
	args.async = async;

	xf_object_call(self, XF_MESSAGE_SOCKET_CLOSE, &args);
}

XROSSFIRE_API void xf_socket_receive(
	xf_socket_t *self,
	void *buffer,
	int length,
	/*out*/int *receive_length,
	xf_async_t *async)
{
	xf_socket_receive_args_t args;
	args.buffer = buffer;
	args.length = length;
	args.receive_length = receive_length;
	args.async = async;

	xf_object_call(self, XF_MESSAGE_SOCKET_RECEIVE, &args);
}

XROSSFIRE_API void xf_socket_send(
	xf_socket_t *self,
	void *buffer,
	int length,
	xf_async_t *async)
{
	xf_socket_send_args_t args;
	args.buffer = buffer;
	args.length = length;
	args.async = async;

	xf_object_call(self, XF_MESSAGE_SOCKET_SEND, &args);
}
