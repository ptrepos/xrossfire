#include <stdio.h>
#include <xrossfire/poll.h>
#include <xrossfire/net.h>

XROSSFIRE_PRIVATE xf_error_t xf_io_completion_port_init();
XROSSFIRE_PRIVATE xf_error_t xf_poll_init();
XROSSFIRE_PRIVATE xf_error_t xf_timeout_init();

typedef struct server_async_context
{
	xf_tcp_socket_t *socket;
	char data[1024];
	int length;
} server_async_context_t;

typedef struct client_async_context
{
	xf_tcp_socket_t *socket;
	char data[1024];
	int length;
} client_async_context_t;

static void server_phase1(xf_error_t err, void *context);
static void server_phase2(xf_error_t err, void *context);

static void client_phase1(xf_error_t err, void *context);
static void client_phase2(xf_error_t err, void *context);

int main(int argc, char *argv[])
{
	xf_error_t err;
	xf_async_t *async1 = NULL;
	xf_async_t *async2 = NULL;
	xf_tcp_server_t *tcp_server = NULL;
	xf_tcp_socket_t *socket = NULL;
	server_async_context_t *server_context = (server_async_context_t *)malloc(sizeof(server_async_context_t));
	client_async_context_t *client_context = (client_async_context_t *)malloc(sizeof(client_async_context_t));

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);

	err = xf_io_completion_port_init();
	if (err != 0)
		goto _ERROR;
	err = xf_poll_init();
	if (err != 0)
		goto _ERROR;
	err = xf_timeout_init();
	if (err != 0)
		goto _ERROR;

	xf_poll_start();

	xf_string_t hostname = XF_STRING_INITIALIZER(_T("*"));
	err = xf_tcp_server_new(&hostname, 9999, &tcp_server);
	if (err != 0)
		goto _ERROR;

	err = xf_async_new(10 * 1000, server_phase1, server_context, NULL, &async1);
	if (err != 0)
		goto _ERROR;

	xf_tcp_server_accept(tcp_server, &server_context->socket, async1);

	err = xf_tcp_socket_new(&socket);
	if (err != 0)
		goto _ERROR;

	client_context->socket = socket;

	err = xf_async_new(3 * 1000, client_phase1, client_context, NULL, &async2);
	if (err != 0)
		goto _ERROR;

	xf_string_t localhost = XF_STRING_INITIALIZER(_T("localhost"));
	xf_tcp_socket_connect(socket, &localhost, 9999, 0, async2);

	fgetc(stdin);

	xf_tcp_server_release(tcp_server);
	xf_tcp_socket_release(socket);
	WSACleanup();

	return 0;
_ERROR:
	xf_tcp_server_release(tcp_server);
	xf_tcp_socket_release(socket);
	WSACleanup();

	return 1;
}

static void server_phase1(xf_error_t err, void *context)
{
	xf_async_t *async = NULL;

	if (err != 0) {
		printf("SERVER ACCEPT ERROR\n");
		return;
	}

	printf("SERVER ACCEPTED\n");

	server_async_context_t *c = (server_async_context_t *)context;

	err = xf_async_new(10 * 1000, server_phase2, c, NULL, &async);
	if (err != 0)
		goto _ERROR;

	xf_tcp_socket_receive(c->socket, c->data, _countof(c->data), &c->length, async);

	return;
_ERROR:
	return;
}

static void server_phase2(xf_error_t err, void *context)
{
	if (err != 0) {
		printf("SERVER RECEIVED ERROR\n");
		return;
	}

	server_async_context_t *c = (server_async_context_t *)context;
	c->data[c->length] = 0;
	printf("SERVER RECEIVED \"%s\"\n", c->data);
}

static void client_phase1(xf_error_t err, void *context)
{
	xf_async_t *async = NULL;

	if (err != 0) {
		printf("CLIENT CONNECT ERROR\n");
		return;
	}

	printf("CLIENT CONNECTED\n");

	client_async_context_t *c = (client_async_context_t *)context;

	err = xf_async_new(10 * 1000, client_phase2, c, NULL, &async);
	if (err != 0)
		goto _ERROR;

	strcpy_s(c->data, 1000, "ASYNC SOCKET SAMPLE xf_tcp_socket_t.");
	int length = strlen(c->data);

	xf_tcp_socket_send(c->socket, c->data, length, &c->length, async);

	return;
_ERROR:
	return;
}

static void client_phase2(xf_error_t err, void *context)
{
	if (err != 0) {
		printf("CLIENT DATA SEND ERROR\n");
		return;
	}

	printf("CLIENT DATA SEND\n");
}
