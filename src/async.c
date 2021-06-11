#include <xrossfire/async.h>
#include <xrossfire/timeout.h>

#if defined(_WIN32)
#include "win32/io_completion_port.h"
#endif

#define XF_ASYNC_ASYNC	(1)

struct xf_async {
	int type;
	xf_timeout_t timeout;
	xf_async_t *child_async;
#if defined(_WIN32)
	xf_io_command_t *command;
#endif
	union {
		struct {
			xf_async_completed_t completed;
			void *context;
		} async;
		//struct {
		//	;
		//} wait;
	} u;
};

static void async_timeout(void *context);

XROSSFIRE_API xf_error_t xf_async_new_2(int timeout, xf_async_completed_t completed, void *context, xf_async_t **self)
{
	xf_error_t err;
	xf_async_t *obj = NULL;
	bool timeout_setup = false;
	
	obj = (xf_async_t*)malloc(sizeof(xf_async_t));
	if (obj == NULL) {
		err = xf_error_stdc(errno);
		goto _ERROR;
	}
	
	obj->type = XF_ASYNC_ASYNC;
	obj->u.async.completed = completed;
	obj->u.async.context = context;
	
	if (timeout > 0) {
		err = xf_timeout_schedule(&obj->timeout, timeout, async_timeout, context);
		if (err != 0)
			goto _ERROR;
		timeout_setup = true;
	}
	
	*self = obj;
	
	return 0;
_ERROR:
	if (obj != NULL && timeout_setup) {
		xf_timeout_cancel(&obj->timeout);
	}
	free(obj);
	
	return err;
}

XROSSFIRE_API void xf_async_release(xf_async_t *self)
{
	xf_timeout_cancel(&self->timeout);
	free(self);
}

XROSSFIRE_API void _xf_async_async_notify(xf_async_t *self, xf_error_t error)
{
	self->u.async.completed(error, self->u.async.context);
}

XROSSFIRE_API void xf_async_notify(xf_async_t *self, xf_error_t error)
{
	xf_error_t err;
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		_xf_async_async_notify(self, error);
		break;
	default:
		err = XF_ERROR;
		break;
	}
}

static void _cancel(xf_async_t *self)
{
	if (self->child_async != NULL) {
		xf_async_cancel(self->child_async);
	}

#if defined(_WIN32)
	if (self->command != NULL) {
		xf_io_command_cancel(self->command);
	}
#endif
}

XROSSFIRE_API xf_error_t xf_async_cancel(xf_async_t *self)
{
	xf_error_t err;
	
	err = xf_timeout_cancel(&self->timeout);
	if (err != 0)
		goto _ERROR;

	_cancel(self);

	xf_async_notify(self, XF_ERROR_CANCEL);

	return 0;
_ERROR:
	
	return err;
}

static void async_timeout(void *context)
{
	xf_async_t *async = (xf_async_t*)context;

	_cancel(async);

	xf_async_notify(async, XF_ERROR_CANCEL);
}

#if defined(_WIN32)
XROSSFIRE_PRIVATE void xf_async_set_command(xf_async_t *self, xf_io_command_t *command)
{
	self->command = command;
}
#endif