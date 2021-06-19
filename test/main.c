#include <stdio.h>
#include <xrossfire/poll.h>
#include <xrossfire/net.h>

XROSSFIRE_PRIVATE xf_error_t xf_io_completion_port_init();
XROSSFIRE_PRIVATE xf_error_t xf_poll_init();
XROSSFIRE_PRIVATE xf_error_t xf_timeout_init();

typedef struct server_async_context
{
	xf_socket_t *socket;
	char data[1024];
	int length;
} server_async_context_t;

typedef struct client_async_context
{
	xf_socket_t *socket;
	char data[1024];
	int length;
} client_async_context_t;

static void server_phase1(xf_error_t err, void *context);
static void server_phase2(xf_error_t err, void *context);

static void client_phase1(xf_error_t err, void *context);
static void client_phase2(xf_error_t err, void *context);

static void ssl_phase1(xf_error_t err, void *context);
static void ssl_phase2(xf_error_t err, void *context);
static void ssl_phase3(xf_error_t err, void *context);

int main(int argc, char *argv[])
{
	xf_error_t err;
	xf_async_t *async1 = NULL;
	xf_async_t *async2 = NULL;
	xf_async_t *async3 = NULL;
	xf_server_socket_t *tcp_server = NULL;
	server_async_context_t *server_context = (server_async_context_t *)malloc(sizeof(server_async_context_t));
	client_async_context_t *client_context = (client_async_context_t *)malloc(sizeof(client_async_context_t));
	client_async_context_t *ssl_context = (client_async_context_t *)malloc(sizeof(client_async_context_t));

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
	err = xf_server_socket_new(&hostname, 11621, &tcp_server);
	if (err != 0)
		goto _ERROR;

	err = xf_async_new(10 * 1000, server_phase1, server_context, NULL, &async1);
	if (err != 0)
		goto _ERROR;

	xf_server_socket_accept(tcp_server, &server_context->socket, async1);

	err = xf_async_new(3 * 1000, client_phase1, client_context, NULL, &async2);
	if (err != 0)
		goto _ERROR;

	xf_string_t localhost = XF_STRING_INITIALIZER(_T("localhost"));
	xf_tcp_socket_new(&localhost, 11621, 0, &client_context->socket, async2);

	err = xf_async_new(60 * 1000, ssl_phase1, ssl_context, NULL, &async3);
	if (err != 0)
		goto _ERROR;

	xf_string_t sample = XF_STRING_INITIALIZER(_T("svn.dachicraft.net"));
	xf_ssl_socket_new(&sample, 443, 0, &ssl_context->socket, async3);

	Sleep(60 * 1000);

	xf_server_socket_release(tcp_server);
	WSACleanup();

	return 0;
_ERROR:
	xf_server_socket_release(tcp_server);
	WSACleanup();

	return 1;
}

static void server_phase1(xf_error_t err, void *context)
{
	xf_async_t *async = NULL;

	if (err != 0) {
		printf("SERVER ACCEPT ERROR\n");
		goto _ERROR;
	}

	printf("SERVER ACCEPTED\n");

	server_async_context_t *c = (server_async_context_t *)context;

	err = xf_async_new(10 * 1000, server_phase2, c, NULL, &async);
	if (err != 0)
		goto _ERROR;

	xf_socket_receive(c->socket, c->data, _countof(c->data), &c->length, async);

	return;
_ERROR:
	free(context);
	return;
}

static void server_phase2(xf_error_t err, void *context)
{
	server_async_context_t *c = (server_async_context_t *)context;

	if (err != 0) {
		printf("SERVER RECEIVED ERROR\n");
		goto _ERROR;
	}

	c->data[c->length] = 0;
	printf("SERVER RECEIVED \"%s\"\n", c->data);

	xf_socket_release(c->socket);
	free(context);

	return;
_ERROR:
	xf_socket_release(c->socket);
	free(context);
	return;
}

static void client_phase1(xf_error_t err, void *context)
{
	client_async_context_t *c = (client_async_context_t *)context;
	xf_async_t *async = NULL;

	if (err != 0) {
		printf("CLIENT CONNECT ERROR\n");
		goto _ERROR;
	}

	printf("CLIENT CONNECTED\n");

	err = xf_async_new(10 * 1000, client_phase2, c, NULL, &async);
	if (err != 0)
		goto _ERROR;

	strcpy_s(c->data, 1000, "ASYNC SOCKET SAMPLE xf_tcp_socket_t.");
	int length = strlen(c->data);

	xf_socket_send(c->socket, c->data, length, async);

	return;
_ERROR:
	free(c);
	return;
}

static void client_phase2(xf_error_t err, void *context)
{
	client_async_context_t *c = (client_async_context_t *)context;
	if (err != 0) {
		printf("CLIENT DATA SEND ERROR\n");
		goto _ERROR;
	}

	printf("CLIENT DATA SEND\n");

	xf_socket_release(c->socket);
	return;
_ERROR:
	free(c);
}

static void ssl_phase1(xf_error_t err, void *context)
{
	xf_async_t *async = NULL;

	if (err != 0) {
		printf("SSL CONNECT ERROR\n");
		goto _ERROR;
	}

	printf("SSL CONNECTED\n");

	client_async_context_t *c = (client_async_context_t *)context;

	err = xf_async_new(10 * 1000, ssl_phase2, c, NULL, &async);
	if (err != 0)
		goto _ERROR;

	strcpy_s(c->data, 1000, "GET /\r\n");
	int length = strlen(c->data);

	xf_socket_send(c->socket, c->data, length, async);
	return;
_ERROR:
	free(context);
	return;
}

static void ssl_phase2(xf_error_t err, void *context)
{
	xf_async_t *async = NULL;
	client_async_context_t *c = (client_async_context_t *)context;

	if (err != 0) {
		printf("SSL SEND ERROR\n");
		goto _ERROR;
	}

	printf("SSL SEND COMPLETED\n");

	err = xf_async_new(10 * 1000, ssl_phase3, c, NULL, &async);
	if (err != 0)
		goto _ERROR;

	xf_socket_receive(c->socket, c->data, sizeof(c->data), &c->length, async);
	return;
_ERROR:
	free(context);
	return;
}

static void ssl_phase3(xf_error_t err, void *context)
{
	xf_async_t *async = NULL;
	client_async_context_t *c = (client_async_context_t *)context;

	if (err != 0) {
		printf("SSL RECEIVE ERROR\n");
		goto _ERROR;
	}

	c->data[c->length] = 0;

	printf("SSL RECEIVE COMPLETED %s\n", c->data);
	return;
_ERROR:
	free(context);
	return;
}
