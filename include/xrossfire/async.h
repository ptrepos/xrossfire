#pragma once

#include <xrossfire/base.h>
#include <xrossfire/error.h>

typedef void (*xf_async_completed_t)(xf_error_t err, void *context);

typedef struct xf_async xf_async_t;

XROSSFIRE_API xf_error_t xf_async_wait_new(
	int timeout,
	xf_async_t **self);
XROSSFIRE_API xf_error_t xf_async_new(
	int timeout, 
	xf_async_completed_t completed, 
	void *context, 
	xf_async_t *parent,
	xf_async_t **self);

XROSSFIRE_API void xf_async_release(xf_async_t *self);

XROSSFIRE_API void xf_async_notify(xf_async_t *self, xf_error_t error);
XROSSFIRE_API xf_error_t xf_async_cancel(xf_async_t *self);
XROSSFIRE_API xf_error_t xf_async_wait(xf_async_t *self);

#if defined(_WIN32)
XROSSFIRE_API void xf_async_set_handle(xf_async_t *self, HANDLE handle);
#endif
