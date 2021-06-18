#include <winsock2.h>

#ifndef SECURITY_WIN32 
#define SECURITY_WIN32 
#endif

#include <subauth.h>
#include <security.h>
#include <schannel.h>
#include <sspi.h>
#include <wincrypt.h>
#include <xrossfire/net.h>

typedef struct xf_ssl_socket_data
{
    xf_socket_t *base_socket;
    CtxtHandle hContext;
} xf_ssl_socket_data_t;

typedef struct xf_ssl_socket_handshake_context
{
    bool first;
    xf_async_t *parent;
    xf_socket_t *base_socket;
    xf_string_t *hostname;

    SECURITY_STATUS ss;
    CredHandle hCredential;

    ULONG in_size;
    ULONG out_size;
    SecBuffer sec_buf_in[1];
    SecBuffer sec_buf_out[1];
    SecBufferDesc sec_buf_desc_in, sec_buf_desc_out;
    CtxtHandle hContext;

    int transfered_length;
} xf_ssl_socket_handshake_context_t;

XROSSFIRE_API xf_error_t xf_ssl_socket_procedure(xf_object_t *self, int message_id, void *args);

static void xf_ssl_socket_handshake_context_free(xf_ssl_socket_handshake_context_t *context)
{
    xf_socket_release(context->base_socket);

    FreeContextBuffer(context->sec_buf_out[0].pvBuffer);
    context->sec_buf_out[0].pvBuffer = NULL;

    free(context->sec_buf_in[0].pvBuffer);
    context->sec_buf_in[0].pvBuffer = NULL;

    free(context);
}

static void ssl_handshake_ph1(xf_error_t err, void *context);
static void ssl_handshake_ph2(xf_error_t err, void *context);
static void ssl_handshake_ph3(xf_error_t err, void *context);
static void ssl_handshake_ph4(xf_error_t err, void *context);
static void ssl_handshake_ph5(xf_error_t err, void *context);

static xf_error_t xf_ssl_client_handshake(xf_ssl_socket_handshake_context_t *context)
{
    xf_error_t err;
    xf_async_t *async = NULL;

    SCHANNEL_CRED sslCred = { 0 };
    sslCred.dwVersion = SCHANNEL_CRED_VERSION;
    sslCred.grbitEnabledProtocols = SP_PROT_SSL3;
    sslCred.dwFlags =
        SCH_CRED_NO_DEFAULT_CREDS |
        SCH_CRED_MANUAL_CRED_VALIDATION;

    TimeStamp tsExpiry;
    SECURITY_STATUS ss;

    ss = AcquireCredentialsHandle(
        NULL,
        UNISP_NAME,
        SECPKG_CRED_OUTBOUND,
        NULL,
        &sslCred,
        NULL, NULL, &context->hCredential, &tsExpiry);
    if (ss != SEC_E_OK) {
        err = XF_ERROR;
        goto _ERROR;
    }

    context->sec_buf_desc_in.cBuffers = 0;
    context->sec_buf_desc_in.ulVersion = SECBUFFER_VERSION;
    context->sec_buf_desc_in.pBuffers = NULL;

    context->ss = SEC_I_CONTINUE_NEEDED;

    ssl_handshake_ph1(0, context);

    return 0;
_ERROR:
    return err;
}

static void ssl_handshake_ph1(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    err = xf_async_new(10 * 1000, ssl_handshake_ph2, context, c->parent, &async);
    if (err != 0)
        goto _ERROR;

    xf_socket_receive(c->base_socket, &c->in_size, sizeof(c->in_size), &c->transfered_length, async);

    return;
_ERROR:
    async = c->parent;
    xf_ssl_socket_handshake_context_free(c);
    xf_async_notify(async, err);
}

static void ssl_handshake_ph2(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    if (err != 0) {
        goto _ERROR;
    }

    void *p = malloc(c->in_size);
    if (p == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    c->sec_buf_in[0].pvBuffer = p;

    err = xf_async_new(10 * 1000, ssl_handshake_ph3, context, c->parent, &async);
    if (err != 0)
        goto _ERROR;
    
    xf_socket_receive(c->base_socket, c->sec_buf_in[0].pvBuffer, c->in_size, &c->transfered_length, async);

    return;
_ERROR:
    async = c->parent;
    xf_ssl_socket_handshake_context_free(c);
    xf_async_notify(async, err);
}

static void ssl_handshake_ph3(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    c->sec_buf_in[0].BufferType = SECBUFFER_TOKEN;
    c->sec_buf_in[0].cbBuffer = c->in_size;
    c->sec_buf_desc_in.cBuffers = 1;
    c->sec_buf_desc_in.ulVersion = SECBUFFER_VERSION;
    c->sec_buf_desc_in.pBuffers = &c->sec_buf_in[0];

    c->sec_buf_out[0].BufferType = SECBUFFER_TOKEN;
    c->sec_buf_out[0].cbBuffer = 0;
    c->sec_buf_out[0].pvBuffer = NULL;
    c->sec_buf_desc_out.cBuffers = 1;
    c->sec_buf_desc_out.ulVersion = SECBUFFER_VERSION;
    c->sec_buf_desc_out.pBuffers = &c->sec_buf_out[0];

    DWORD attr = 0;

    c->ss = InitializeSecurityContextW(
    	&c->hCredential, 
    	c->first ? NULL : &c->hContext,
    	(WCHAR*)xf_string_to_cstr(c->hostname), 
    	attr | ISC_REQ_ALLOCATE_MEMORY, 
    	0,
        SECURITY_NETWORK_DREP, 
    	&c->sec_buf_desc_in, 
    	0, 
    	&c->hContext,
        &c->sec_buf_desc_out, 
    	&attr, 
    	NULL);

    if (FAILED(c->ss)) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }

    if (c->sec_buf_out[0].cbBuffer != 0) {
        c->out_size = c->sec_buf_out[0].cbBuffer;

        err = xf_async_new(10 * 1000, ssl_handshake_ph4, context, c->parent, &async);
        if (err != 0)
            goto _ERROR;

        xf_socket_send(c->base_socket, &c->out_size, sizeof(c->out_size), &c->transfered_length, async);
    }

    return ;
_ERROR:
    async = c->parent;
    xf_ssl_socket_handshake_context_free(c);
    xf_async_notify(async, err);
}

static void ssl_handshake_ph4(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    if (c->sec_buf_out[0].cbBuffer != 0) {
        err = xf_async_new(10 * 1000, ssl_handshake_ph5, context, c->parent, &async);
        if (err != 0)
            goto _ERROR;

        xf_socket_send(c->base_socket, c->sec_buf_out[0].pvBuffer, c->sec_buf_out[0].cbBuffer, &c->transfered_length, async);
    }

    return;
_ERROR:
    async = c->parent;
    xf_ssl_socket_handshake_context_free(c);
    xf_async_notify(async, err);
}

static void ssl_handshake_ph5(xf_error_t err, void *context)
{
    xf_object_t *obj = NULL;
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    FreeContextBuffer(c->sec_buf_out[0].pvBuffer);
	c->sec_buf_out[0].pvBuffer = NULL;
	
    free(c->sec_buf_in[0].pvBuffer);
	c->sec_buf_in[0].pvBuffer = NULL;
	
	c->first = false;

    if (c->ss == SEC_I_CONTINUE_NEEDED) {
        ssl_handshake_ph1(0, context);
    } else {
        err = xf_object_new(xf_ssl_socket_procedure, sizeof(xf_ssl_socket_data_t), &obj);
        if (err != 0)
            goto _ERROR;
        xf_ssl_socket_data_t *data = (xf_ssl_socket_data_t *)xf_object_get_body(obj);

        data->base_socket = c->base_socket;
        data->hContext = c->hContext;

        c->base_socket = NULL;
        
        async = c->parent;
        xf_ssl_socket_handshake_context_free(c);
        
        xf_async_notify(async, 0);
    }

    return;
_ERROR:
    async = c->parent;
    xf_ssl_socket_handshake_context_free(c);
    xf_async_notify(async, err);
}

static void xf_ssl_socket_ph1(xf_error_t err, void *context);

XROSSFIRE_API xf_error_t xf_ssl_socket_new(
    xf_string_t *hostname,
    int port,
    int options,
    xf_socket_t **self,
    xf_async_t *async)
{
    xf_error_t err;
    xf_socket_t *obj = NULL;
    xf_async_t *async1 = NULL;
    xf_ssl_socket_handshake_context_t *context = NULL;

    context = (xf_ssl_socket_handshake_context_t*)malloc(sizeof(xf_ssl_socket_handshake_context_t));
    if (context == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    context->hostname = hostname;
    context->first = true;
    context->parent = async;

    err = xf_async_new(30 * 1000, xf_ssl_socket_ph1, context, async, &async1);
    if (err != 0)
        goto _ERROR;

    err = xf_tcp_socket_new(hostname, port, options, &context->base_socket, async1);
    if (err != 0)
        goto _ERROR;

    *self = obj;

    return 0;
_ERROR:
    xf_object_release(obj);

    return err;
}

static void xf_ssl_socket_ph1(xf_error_t err, void *context)
{
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    if (err != 0) {
        free(context);
        xf_async_notify(c->parent, err);
        return;
    }

    err = xf_ssl_client_handshake(context);
    if (err != 0) {
        free(context);
        xf_async_notify(c->parent, err);
        return;
    }
}

static xf_string_t CLASS_ID = XF_STRING_INITIALIZER(_T("xrossfire.net.SSLSocket"));

XROSSFIRE_API xf_error_t xf_ssl_socket_procedure(xf_object_t *self, int message_id, void *args)
{
    xf_error_t err;
    xf_async_t *async = NULL;
    xf_ssl_socket_data_t *data = (xf_ssl_socket_data_t *)xf_object_get_body(self);

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
        if (data->base_socket != NULL) {
            xf_socket_release(data->base_socket);
            data->base_socket = NULL;
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
        __xf_socket_send(self, _args->buffer, _args->length, _args->send_length, _args->async);
        return 0;
    }
    }

    return xf_object_default_procedure(self, message_id, args);
}
