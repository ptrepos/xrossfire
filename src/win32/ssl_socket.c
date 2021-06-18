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

typedef struct xf_ssl_socket_handshake_context
{
    bool first;
    xf_async_t *parent;
    xf_socket_t *base_socket;

    ULONG in_size;
    ULONG out_size;

    SECURITY_STATUS ss;
    CredHandle hCredential;

    SecBuffer secBufIn[1];
    SecBuffer secBufOut[1];
    SecBufferDesc secBufDescIn, secBufDescOut;
    CtxtHandle hContext;

    int transfered_length;
} xf_ssl_socket_handshake_context_t;

static void ssl_handshake_ph1(xf_error_t err, void *context);
static void ssl_handshake_ph2(xf_error_t err, void *context);
static void ssl_handshake_ph3(xf_error_t err, void *context);
static void ssl_handshake_ph4(xf_error_t err, void *context);
static void ssl_handshake_ph5(xf_error_t err, void *context);


xf_error_t xf_ssl_socket_client_handshake(xf_socket_t *base_socket, xf_async_t *parent)
{
    xf_error_t err;
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *context = NULL;

    context = (xf_ssl_socket_handshake_context_t *)malloc(sizeof(xf_ssl_socket_handshake_context_t));
    if (context == NULL) {
        err = xf_error_libc(errno);
        goto _ERROR;
    }

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
        //err = XXXXXXCCCCCCCCCCCCCCCCCCCX;
        goto _ERROR;
    }

    context->base_socket = base_socket;
    context->parent = parent;
    context->first = true;
    context->secBufDescIn.cBuffers = 0;
    context->secBufDescIn.ulVersion = SECBUFFER_VERSION;
    context->secBufDescIn.pBuffers = NULL;

    context->ss = SEC_I_CONTINUE_NEEDED;

    ssl_handshake_ph1(0, context);

    return 0;
_ERROR:
    return err;
}

void ssl_handshake_ph1(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    err = xf_async_new(10 * 1000, ssl_handshake_ph2, context, c->parent, &async);
    if (err != 0)
        goto _ERROR;

    xf_socket_receive(c->base_socket, &c->in_size, sizeof(c->in_size), &c->transfered_length, async);

    return;
_ERROR:
    xf_async_notify(c->parent, err);
}

void ssl_handshake_ph2(xf_error_t err, void *context)
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

    c->secBufIn[0].pvBuffer = p;

    err = xf_async_new(10 * 1000, ssl_handshake_ph3, context, c->parent, &async);
    if (err != 0)
        goto _ERROR;
    
    xf_socket_receive(c->base_socket, c->secBufIn[0].pvBuffer, c->in_size, &c->transfered_length, async);

    return;
_ERROR:
    xf_async_notify(c->parent, err);
    return;
}

static void ssl_handshake_ph3(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    c->secBufIn[0].BufferType = SECBUFFER_TOKEN;
    c->secBufIn[0].cbBuffer = c->in_size;
    c->secBufDescIn.cBuffers = 1;
    c->secBufDescIn.ulVersion = SECBUFFER_VERSION;
    c->secBufDescIn.pBuffers = &c->secBufIn[0];

    c->secBufOut[0].BufferType = SECBUFFER_TOKEN;
    c->secBufOut[0].cbBuffer = 0;
    c->secBufOut[0].pvBuffer = NULL;
    c->secBufDescOut.cBuffers = 1;
    c->secBufDescOut.ulVersion = SECBUFFER_VERSION;
    c->secBufDescOut.pBuffers = &c->secBufOut[0];

    DWORD attr = 0;

    c->ss = InitializeSecurityContextW(&c->hCredential, c->first ? NULL : &c->hContext,
        _T("localhost"), attr | ISC_REQ_ALLOCATE_MEMORY, 0,
        SECURITY_NETWORK_DREP, &c->secBufDescIn, 0, &c->hContext,
        &c->secBufDescOut, &attr, NULL);

    if (FAILED(c->ss)) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }

    if (c->secBufOut[0].cbBuffer != 0) {
        c->out_size = c->secBufOut[0].cbBuffer;

        err = xf_async_new(10 * 1000, ssl_handshake_ph4, context, c->parent, &async);
        if (err != 0)
            goto _ERROR;

        xf_socket_send(c->base_socket, &c->out_size, sizeof(c->out_size), &c->transfered_length, async);
    }

    return ;
_ERROR:
    xf_async_notify(c->parent, err);
    return;
}

static void ssl_handshake_ph4(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    if (c->secBufOut[0].cbBuffer != 0) {
        err = xf_async_new(10 * 1000, ssl_handshake_ph5, context, c->parent, &async);
        if (err != 0)
            goto _ERROR;

        xf_socket_send(c->base_socket, c->secBufOut[0].pvBuffer, c->secBufOut[0].cbBuffer, &c->transfered_length, async);
    }

    return;
_ERROR:
    xf_async_notify(c->parent, err);
    return;
}

static void ssl_handshake_ph5(xf_error_t err, void *context)
{
    xf_async_t *async = NULL;
    xf_ssl_socket_handshake_context_t *c = (xf_ssl_socket_handshake_context_t *)context;

    FreeContextBuffer(c->secBufOut[0].pvBuffer);
    free(c->secBufIn[0].pvBuffer);

    c->first = false;

    if (c->ss == SEC_I_CONTINUE_NEEDED) {
        ssl_handshake_ph1(0, context);
    } else {
        xf_async_notify(c->parent, 0);
    }

    return;
_ERROR:
    xf_async_notify(c->parent, err);
    return;
}