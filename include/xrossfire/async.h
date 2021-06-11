#pragma once

#include <xrossfire/base.h>
#include <xrossfire/error.h>

typedef void (*xf_async_completed_t)(xf_error_t err, void *context);

typedef struct xf_async xf_async_t;

XROSSFIRE_API xf_error_t xf_async_wait_new_1(xf_async_t **self);
XROSSFIRE_API xf_error_t xf_async_wait_new_2(int timeout, xf_async_t **self);
XROSSFIRE_API xf_error_t xf_async_new_1(xf_async_completed_t completed, void *context, xf_async_t **self);
XROSSFIRE_API xf_error_t xf_async_new_2(int timeout, xf_async_completed_t completed, void *context, xf_async_t **self);

XROSSFIRE_API void xf_async_release(xf_async_t *self);

XROSSFIRE_API xf_error_t xf_async_notify(xf_async_t *self, xf_error_t error);
XROSSFIRE_API xf_error_t xf_async_cancel(xf_async_t *self);
XROSSFIRE_API xf_error_t xf_async_wait(xf_async_t *self);

#define XF_ASYNC_CONTROL_METHOD_RELEASE	(1)
#define XF_ASYNC_CONTROL_METHOD_CANCEL	(2)

typedef xf_error_t (*xf_async_control_procedure_t)(void *control, int method_id, void *args);

XROSSFIRE_API xf_error_t _xf_async_set_control(xf_async_t *self, void *control, xf_async_control_procedure_t controlproc);
