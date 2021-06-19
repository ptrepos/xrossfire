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
    CredHandle hCredential;

    byte *received_buffer;
    int received_pos;
    int received_length;
} xf_ssl_socket_data_t;

typedef struct xf_ssl_socket_handshake_context
{
    bool first;
    xf_async_t *parent;
    xf_socket_t *base_socket;
    xf_string_t *hostname;
    xf_socket_t **self;

    SECURITY_STATUS ss;
    CredHandle hCredential;
    CtxtHandle hContext;

    ULONG in_size;
    ULONG out_size;
    SecBuffer sec_buf_in[1];
    SecBuffer sec_buf_out[1];
    SecBufferDesc sec_buf_desc_in, sec_buf_desc_out;
    DWORD attrs;

    int transfered_length;

    char buf[256];
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
    sslCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
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

    context->ss = SEC_I_CONTINUE_NEEDED;
    context->attrs = 0;

    //// read hello request.
    //err = xf_async_new(500, ssl_handshake_ph3, context, context->parent, &async);
    //if (err != 0)
    //    goto _ERROR;
    //xf_socket_receive(context->base_socket, context->buf, sizeof(context->buf), &context->transfered_length, async);

    ssl_handshake_ph3(0, context);

    return 0;
_ERROR:
    return err;
}

static void ssl_handshake_ph1(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    if (err != 0)
        goto _ERROR;

    //err = xf_async_new(30 * 1000, ssl_handshake_ph2, context, c->parent, &async);
    //if (err != 0)
    //    goto _ERROR;

    //xf_socket_receive(c->base_socket, &c->in_size, sizeof(c->in_size), &c->transfered_length, async);
    
    //SecPkgContext_Sizes PkgSize;

    //SECURITY_STATUS ss = QueryContextAttributes(&c->hContext, SECPKG_ATTR_SIZES, &PkgSize);

    //if (SEC_E_OK != ss) {
    //    printf("QueryContextAttributes Failed. - ss = 0x%08x\n", ss);
    //    return 1;
    //}

    c->in_size = 4096;
    
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

static void dump_data(byte *data, int length)
{
    printf("CLIENT HELLO: ");
    for (int i = 0; i < length; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}


static void ssl_handshake_ph3(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    if (err != 0)
        goto _ERROR;

    if (c->first) {
        c->sec_buf_desc_in.cBuffers = 0;
        c->sec_buf_desc_in.ulVersion = SECBUFFER_VERSION;
        c->sec_buf_desc_in.pBuffers = NULL;
    } else {
        c->in_size = c->transfered_length;
        c->sec_buf_in[0].BufferType = SECBUFFER_TOKEN;
        c->sec_buf_in[0].cbBuffer = c->in_size;
        c->sec_buf_desc_in.cBuffers = 1;
        c->sec_buf_desc_in.ulVersion = SECBUFFER_VERSION;
        c->sec_buf_desc_in.pBuffers = &c->sec_buf_in[0];
    }

    c->sec_buf_out[0].BufferType = SECBUFFER_TOKEN;
    c->sec_buf_out[0].cbBuffer = 0;
    c->sec_buf_out[0].pvBuffer = NULL;
    c->sec_buf_desc_out.cBuffers = 1;
    c->sec_buf_desc_out.ulVersion = SECBUFFER_VERSION;
    c->sec_buf_desc_out.pBuffers = &c->sec_buf_out[0];

    c->ss = InitializeSecurityContextW(
    	&c->hCredential, 
    	c->first ? NULL : &c->hContext,
    	(WCHAR*)xf_string_to_cstr(c->hostname), 
        ISC_REQ_ALLOCATE_MEMORY,
    	0,
        SECURITY_NETWORK_DREP, 
    	&c->sec_buf_desc_in, 
    	0, 
    	&c->hContext,
        &c->sec_buf_desc_out, 
    	&c->attrs, 
    	NULL);

    if (FAILED(c->ss)) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }

    if (c->sec_buf_out[0].cbBuffer != 0) {
        //dump_data((byte*)c->sec_buf_out[0].pvBuffer, c->sec_buf_out[0].cbBuffer);

        c->out_size = c->sec_buf_out[0].cbBuffer;

        err = xf_async_new(30 * 1000, ssl_handshake_ph5, context, c->parent, &async);
        if (err != 0)
            goto _ERROR;

        xf_socket_send(c->base_socket, c->sec_buf_out[0].pvBuffer, c->sec_buf_out[0].cbBuffer, async);

        //err = xf_async_new(30 * 1000, ssl_handshake_ph4, context, c->parent, &async);
        //if (err != 0)
        //    goto _ERROR;

        //xf_socket_send(c->base_socket, &c->out_size, sizeof(c->out_size), async);
        return;
    }

    ssl_handshake_ph5(0, context);

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

    if (err != 0)
        goto _ERROR;

    if (c->sec_buf_out[0].cbBuffer != 0) {
        err = xf_async_new(30 * 1000, ssl_handshake_ph5, context, c->parent, &async);
        if (err != 0)
            goto _ERROR;

        xf_socket_send(c->base_socket, c->sec_buf_out[0].pvBuffer, c->sec_buf_out[0].cbBuffer, async);
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

    if (err != 0)
        goto _ERROR;

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
        data->hCredential = c->hCredential;
        data->received_buffer = NULL;
        data->received_length = 0;
        data->received_pos = 0;

        c->base_socket = NULL;
        *c->self = obj;

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
    memset(context, 0, sizeof(xf_ssl_socket_handshake_context_t));

    context->hostname = hostname;
    context->first = true;
    context->parent = async;
    context->self = self;

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
        xf_async_t *async = c->parent;
        free(context);
        xf_async_notify(async, err);
        return;
    }

    err = xf_ssl_client_handshake(context);
    if (err != 0) {
        xf_async_t *async = c->parent;
        free(context);
        xf_async_notify(async, err);
        return;
    }
}

static void __xf_socket_close(
    xf_socket_t *self,
    xf_async_t *async)
{
    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t *)xf_object_get_body(self);

    xf_socket_close(body->base_socket, async);
    body->base_socket = NULL;
}

typedef struct xf_ssl_receive_context
{
    xf_socket_t *self;
    byte *buffer;
    int length;
    int *receive_length;
    xf_async_t *parent;

    int iobuf_size;
    byte *iobuf;

    int transfered;
} xf_ssl_receive_context_t;

static void copy_from_buffer(xf_ssl_socket_data_t *body, byte *buffer, int length, int *receive_length, xf_async_t *async);
static void xf_ssl_socket_receive_phase1(xf_ssl_receive_context_t *context);
static void xf_ssl_socket_receive_phase2(xf_error_t err, void *context);
static void xf_ssl_socket_receive_phase3(xf_error_t err, void *context);

static void __xf_socket_receive(
    xf_socket_t *self,
    void *buffer,
    int length,
    /*out*/int *receive_length,
    xf_async_t *async)
{
    xf_error_t err;
    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t *)xf_object_get_body(self);
    xf_ssl_receive_context_t *context = NULL;
    xf_async_t *async1 = NULL;
    PBYTE iobuf = NULL;

    if (body->received_pos < body->received_length) {
        copy_from_buffer(body, buffer, length, receive_length, async);
        return;
    }

    free(body->received_buffer);
    body->received_buffer = NULL;

    context = (xf_ssl_receive_context_t *)malloc(sizeof(xf_ssl_receive_context_t));
    if (context == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    context->self = self;
    context->buffer = buffer;
    context->length = length;
    context->receive_length = receive_length;
    context->parent = async;

    xf_ssl_socket_receive_phase1(context);

    return;
_ERROR:
    xf_async_notify(async, err);
}

static void copy_from_buffer(xf_ssl_socket_data_t *body, byte *buffer, int length, int *receive_length, xf_async_t *async)
{
    int copy_len = body->received_length - body->received_pos;
    if (length < copy_len)
        copy_len = length;

    memcpy(buffer, &body->received_buffer[body->received_pos], copy_len);
    body->received_pos += copy_len;

    *receive_length = copy_len;

    xf_async_notify(async, 0);
}

static void xf_ssl_socket_receive_phase1(xf_ssl_receive_context_t *context)
{
    xf_error_t err;
    xf_async_t *async = NULL;
    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t *)xf_object_get_body(context->self);

    //err = xf_async_new(30 * 1000, xf_ssl_socket_receive_phase2, context, context->parent, &async);
    //if (err != 0)
    //    goto _ERROR;
    //
    //xf_socket_receive(
    //    context->self,
    //    &context->iobuf_size,
    //    sizeof(context->iobuf_size),
    //    &context->transfered,
    //    async);

    SecPkgContext_StreamSizes PkgSize;

    SECURITY_STATUS ss = QueryContextAttributes(
        &body->hContext,
        SECPKG_ATTR_STREAM_SIZES,
        &PkgSize);

    if (SEC_E_OK != ss) {
        err = XF_ERROR;
        goto _ERROR;
    }

    context->iobuf_size = PkgSize.cbHeader + PkgSize.cbTrailer + PkgSize.cbMaximumMessage;

    context->iobuf = malloc(context->iobuf_size);
    if (context->iobuf == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    err = xf_async_new(30 * 1000, xf_ssl_socket_receive_phase3, context, context->parent, &async);
    if (err != 0)
        goto _ERROR;

    xf_socket_receive(
        body->base_socket,
        context->iobuf,
        context->iobuf_size,
        &context->transfered,
        async);
    return;
_ERROR:
    async = context->parent;
    free(context);

    xf_async_notify(async, err);
}

static void xf_ssl_socket_receive_phase2(xf_error_t err, void *context)
{
    xf_ssl_receive_context_t *c = (xf_ssl_receive_context_t *)context;
    xf_async_t *async = NULL;

    if (err != 0)
        goto _ERROR;

    c->iobuf = malloc(c->iobuf_size);
    if (c->iobuf == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    err = xf_async_new(30 * 1000, xf_ssl_socket_receive_phase3, c, c->parent, &async);
    if (err != 0)
        goto _ERROR;

    xf_socket_receive(
        c->self,
        &c->iobuf,
        c->iobuf_size,
        &c->transfered,
        async);

    return;
_ERROR:
    async = c->parent;
    free(c->iobuf);
    free(c);

    xf_async_notify(async, err);
}

static void xf_ssl_socket_receive_phase3(xf_error_t err, void *context)
{
    xf_ssl_receive_context_t *c = (xf_ssl_receive_context_t *)context;
    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t *)xf_object_get_body(c->self);
    xf_async_t *async = NULL;
    SecBuffer sec_buf_msg[4] = { 0 };
    SecBufferDesc sec_buf_desc_msg;

    if (err != 0)
        goto _ERROR;

    sec_buf_msg[0].BufferType = SECBUFFER_DATA;
    sec_buf_msg[0].cbBuffer = c->iobuf_size;
    sec_buf_msg[0].pvBuffer = c->iobuf;

    sec_buf_msg[1].BufferType = SECBUFFER_EMPTY;
    sec_buf_msg[2].BufferType = SECBUFFER_EMPTY;
    sec_buf_msg[3].BufferType = SECBUFFER_EMPTY;

    // バッファディスクリプション
    sec_buf_desc_msg.cBuffers = 4;
    sec_buf_desc_msg.ulVersion = SECBUFFER_VERSION;
    sec_buf_desc_msg.pBuffers = sec_buf_msg;

    ULONG lQual = 0;
    SECURITY_STATUS ss = DecryptMessage(&body->hContext, &sec_buf_desc_msg, 0, &lQual);
    if (ss != SEC_E_OK) {
        err = XF_ERROR;
        goto _ERROR;
    }

    body->received_buffer = c->iobuf;
    body->received_pos = sec_buf_msg[0].cbBuffer;
    body->received_length = sec_buf_msg[1].cbBuffer;

    copy_from_buffer(body, c->buffer, c->length, c->receive_length, c->parent);

    async = c->parent;
    free(c->iobuf);
    free(c);

    xf_async_notify(async, 0);

    return;
_ERROR:
    async = c->parent;
    free(c->iobuf);
    free(c);

    xf_async_notify(async, err);
}

typedef struct xf_ssl_send_context
{
    xf_socket_t *self;
    byte *send_buffer;
    int send_length;
    xf_async_t *parent;

    byte *iobuf;
    int iobuf_size;
    SecPkgContext_StreamSizes PkgSize;
} xf_ssl_send_context_t;

static void xf_ssl_socket_send_phase1(xf_ssl_send_context_t *context);
static void xf_ssl_socket_send_phase2(xf_error_t err, void *context);
static void xf_ssl_socket_send_phase3(xf_error_t err, void *context);

static void __xf_socket_send(
    xf_socket_t *self,
    void *buffer,
    int length,
    xf_async_t *async)
{
    xf_error_t err;
    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t*)xf_object_get_body(self);
    xf_ssl_send_context_t *context = NULL;
    xf_async_t *async1 = NULL;
    PBYTE iobuf = NULL;
    SecPkgContext_StreamSizes PkgSize;

    SECURITY_STATUS ss = QueryContextAttributes(
        &body->hContext,
        SECPKG_ATTR_STREAM_SIZES,
        &PkgSize);

    if (SEC_E_OK != ss) {
        err = XF_ERROR;
        goto _ERROR;
    }

    iobuf =(PBYTE)malloc(PkgSize.cbHeader + PkgSize.cbMaximumMessage + PkgSize.cbTrailer);
    if (iobuf == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    context = (xf_ssl_send_context_t *)malloc(sizeof(xf_ssl_send_context_t));
    if (context == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

    context->self = self;
    context->send_buffer = buffer;
    context->send_length = length;
    context->iobuf = iobuf;
    context->parent = async;
    context->PkgSize = PkgSize;
    
    xf_ssl_socket_send_phase1(context);

    return;
_ERROR:
    free(context);
    free(iobuf);

    xf_async_notify(async, err);
}

static void xf_ssl_socket_send_phase1(xf_ssl_send_context_t *context)
{
    xf_error_t err;
    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t *)xf_object_get_body(context->self);
    xf_async_t *async = NULL;
    xf_async_t *parent;

    int length = context->send_length;
    if (length > (int)context->PkgSize.cbMaximumMessage) {
        length = (int)context->PkgSize.cbMaximumMessage;
    }
    memcpy(context->iobuf + context->PkgSize.cbHeader, context->send_buffer, length);

    context->send_buffer += length;
    context->send_length -= length;

    SecBuffer sec_buf_msg[3];
    SecBufferDesc sec_buf_desc_msg;

    // シグネチャバッファ
    sec_buf_msg[0].BufferType = SECBUFFER_STREAM_HEADER;
    sec_buf_msg[0].cbBuffer = context->PkgSize.cbHeader;
    sec_buf_msg[0].pvBuffer = context->iobuf;

    // メッセージバッファ
    sec_buf_msg[1].BufferType = SECBUFFER_DATA;
    sec_buf_msg[1].cbBuffer = length;
    sec_buf_msg[1].pvBuffer = context->iobuf + context->PkgSize.cbHeader;

    // パディングバッファ
    sec_buf_msg[2].BufferType = SECBUFFER_STREAM_TRAILER;
    sec_buf_msg[2].cbBuffer = context->PkgSize.cbTrailer;
    sec_buf_msg[2].pvBuffer = context->iobuf + context->PkgSize.cbHeader + length;

    // バッファディスクリプション
    sec_buf_desc_msg.cBuffers = 3;
    sec_buf_desc_msg.ulVersion = SECBUFFER_VERSION;
    sec_buf_desc_msg.pBuffers = sec_buf_msg;

    SECURITY_STATUS ss = EncryptMessage(&body->hContext, 0, &sec_buf_desc_msg, 0);

    err = xf_async_new(30 * 1000, xf_ssl_socket_send_phase3, context, context->parent, &async);
    if (err != 0)
        goto _ERROR;

    int iobuf_size = sec_buf_msg[0].cbBuffer + sec_buf_msg[1].cbBuffer + sec_buf_msg[2].cbBuffer;
    context->iobuf_size = iobuf_size;
    xf_socket_send(body->base_socket, context->iobuf, context->iobuf_size, async);

    return;
_ERROR:
    parent = context->parent;

    xf_async_release(async);
    free(context->iobuf);
    free(context);
    
    xf_async_notify(parent, err);
}

static void xf_ssl_socket_send_phase2(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_send_context_t *c = (xf_ssl_send_context_t *)context;

    if (err != 0)
        goto _ERROR;

    xf_ssl_socket_data_t *body = (xf_ssl_socket_data_t *)xf_object_get_body(c->self);
    if (err != 0)
        goto _ERROR;

    err = xf_async_new(30 * 1000, xf_ssl_socket_send_phase3, c, c->parent, &async);
    if (err != 0)
        goto _ERROR;

    xf_socket_send(body->base_socket, c->iobuf, c->iobuf_size, async);

    return;
_ERROR:
    async = c->parent;

    free(c->iobuf);
    free(c);

    xf_async_notify(async, err);
}

static void xf_ssl_socket_send_phase3(xf_error_t err, void *context)
{
    xf_async_t *parent;
    xf_ssl_send_context_t *c = (xf_ssl_send_context_t*)context;

    if (err != 0)
        goto _ERROR;

    if (c->send_length > 0) {
        xf_ssl_socket_send_phase1(c);
        return;
    }

    parent = c->parent;

    free(c->iobuf);
    free(c);

    xf_async_notify(parent, 0);
    return;
_ERROR:
    parent = c->parent;

    free(c->iobuf);
    free(c);

    xf_async_notify(parent, err);
}

static xf_string_t CLASS_ID = XF_STRING_INITIALIZER(_T("xrossfire.net.SSLSocket"));

XROSSFIRE_API xf_error_t xf_ssl_socket_procedure(xf_object_t *self, int message_id, void *args)
{
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
        __xf_socket_send(self, _args->buffer, _args->length, _args->async);
        return 0;
    }
    }

    return xf_object_default_procedure(self, message_id, args);
}
