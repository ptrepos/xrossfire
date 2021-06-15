#include <xrossfire/async.h>
#include <xrossfire/timeout.h>

#if defined(_WIN32)
#include "win32/io_completion_port.h"
#endif

#define XF_ASYNC_ASYNC		(1)
#define XF_ASYNC_WAIT		(2)

struct xf_async
{
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
		struct {
			xf_monitor_t lock;
			int status;
			xf_error_t error;
		} wait;
	} u;
};

static void async_timeout(void *context);

XROSSFIRE_API xf_error_t xf_async_new(int timeout, xf_async_completed_t completed, void *context, xf_async_t *parent, xf_async_t **self)
{
	xf_error_t err;
	xf_async_t *obj = NULL;
	bool timeout_setup = false;
	
	obj = (xf_async_t*)malloc(sizeof(xf_async_t));
	if (obj == NULL) {
		err = xf_error_libc(errno);
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
	
	if (parent != NULL) {
		parent->child_async = obj;
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

XROSSFIRE_API xf_error_t xf_async_wait_new(int timeout, xf_async_t **self)
{
	xf_error_t err;
	xf_async_t *obj = NULL;
	bool timeout_setup = false;
	
	obj = (xf_async_t*)malloc(sizeof(xf_async_t));
	if (obj == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}
	
	obj->type = XF_ASYNC_WAIT;
	xf_monitor_init(&obj->u.wait.lock);
	obj->u.wait.status = 0;
	obj->u.wait.error = 0;
	
	if (timeout > 0) {
		err = xf_timeout_schedule(&obj->timeout, timeout, async_timeout, obj);
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
	if (obj != NULL) {
		xf_monitor_destroy(&obj->u.wait.lock);
	}
	free(obj);
	
	return err;
}

XROSSFIRE_API void xf_async_release(xf_async_t *self)
{
	if (self == NULL)
		return;
		
	xf_timeout_cancel(&self->timeout);
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		break;
	case XF_ASYNC_WAIT:
		xf_monitor_destroy(&self->u.wait.lock);
		break;
	}
	
	free(self);
}

XROSSFIRE_API xf_error_t xf_async_wait(xf_async_t *self)
{
	xf_assert(self->type == XF_ASYNC_WAIT);
	
	xf_monitor_enter(&self->u.wait.lock);
	
	while (self->u.wait.status == 0) {
		xf_monitor_wait(&self->u.wait.lock, 5 * 1000);
	}
	
	xf_monitor_leave(&self->u.wait.lock);
	
	return self->u.wait.error;
}

static void _xf_async_async_notify(xf_async_t *self, xf_error_t error)
{
	self->u.async.completed(error, self->u.async.context);
}


static void _xf_async_wait_notify(xf_async_t *self, xf_error_t error)
{
	xf_monitor_enter(&self->u.wait.lock);
	
	self->u.wait.status = 1;
	self->u.wait.error = error;
	xf_monitor_notify(&self->u.wait.lock);
	
	xf_monitor_leave(&self->u.wait.lock);
}

XROSSFIRE_API void xf_async_notify(xf_async_t *self, xf_error_t error)
{
	xf_error_t err;
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		_xf_async_async_notify(self, error);
		break;
	case XF_ASYNC_WAIT:
		_xf_async_wait_notify(self, error);
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
