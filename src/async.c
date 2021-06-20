#include <xrossfire/async.h>
#include <xrossfire/timeout.h>

#if defined(_WIN32)
#include "win32/io_completion_port.h"
#endif

#define XF_ASYNC_ASYNC		(1)
#define XF_ASYNC_WAIT		(2)
#define XF_ASYNC_MULTIPHASE (3)

struct xf_async
{
#if defined(_WIN32)
	xf_io_async_t io_async;
#endif
	int type;
	xf_timeout_t timeout;
	xf_async_t *parent;
	xf_async_t *child_async;
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
		struct {
			xf_monitor_t lock;
			xf_async_procedure_t procedure;
			int next_phase;
			void *context;
		} multi_phase;
	} u;
};

static void async_timeout(void *context);
static void xf_async_multipart_run_next_phase(xf_async_t *self, xf_error_t error);

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

	memset(obj, 0, sizeof(xf_async_t));
	obj->io_async.handle = INVALID_HANDLE_VALUE;
	
	obj->type = XF_ASYNC_ASYNC;
	obj->child_async = NULL;
	obj->u.async.completed = completed;
	obj->u.async.context = context;

	obj->parent = parent;
	if (parent != NULL) {
		parent->child_async = obj;
	}
	
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

#if defined(_WIN32)
	memset(&obj->io_async, 0, sizeof(obj->io_async));
	obj->io_async.handle = (HANDLE)INVALID_SOCKET;
#endif
	
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

	if (self->parent != NULL) {
		self->parent->child_async = NULL;
	}
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		break;
	case XF_ASYNC_WAIT:
		xf_monitor_destroy(&self->u.wait.lock);
		break;
	case XF_ASYNC_MULTIPHASE:
		xf_monitor_destroy(&self->u.multi_phase.lock);
		break;
	}

#if defined(_WIN32)
	xf_io_async_clear(&self->io_async);
#endif
	
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

	xf_async_release(self);
}

static void _xf_async_wait_notify(xf_async_t *self, xf_error_t error)
{
	xf_monitor_enter(&self->u.wait.lock);
	
	self->u.wait.status = 1;
	self->u.wait.error = error;
	xf_monitor_notify(&self->u.wait.lock);
	
	xf_monitor_leave(&self->u.wait.lock);
}

static void _xf_async_multipart_notify(xf_async_t *self, xf_error_t error)
{
	xf_async_multipart_run_next_phase(self, error);
}

XROSSFIRE_API void xf_async_notify(xf_async_t *self, xf_error_t error)
{
	xf_error_t err;

	xf_timeout_cancel(&self->timeout);
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		_xf_async_async_notify(self, error);
		break;
	case XF_ASYNC_WAIT:
		_xf_async_wait_notify(self, error);
		break;
	case XF_ASYNC_MULTIPHASE:
		_xf_async_multipart_notify(self, error);
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
	if (self->io_async.handle != (HANDLE)INVALID_SOCKET) {
		if (xf_io_async_cancel(&self->io_async) == false) {
			xf_async_multipart_run_next_phase(self, XF_ERROR_CANCEL);
		}
	}
#endif
}

XROSSFIRE_API xf_error_t xf_async_cancel(xf_async_t *self)
{
	_cancel(self);

	return 0;
}

static void async_timeout(void *context)
{
	xf_async_t *async = (xf_async_t*)context;

	_cancel(async);
}

#if defined(_WIN32)
XROSSFIRE_PRIVATE xf_io_async_t *xf_async_get_io_async(xf_async_t *self)
{
	return &self->io_async;
}
#endif

XROSSFIRE_API void xf_async_call(
	int timeout,
	xf_async_procedure_t procedure,
	void *context,
	xf_async_t *parent)
{
	xf_error_t err;
	xf_async_t *obj = NULL;
	bool timeout_setup = false;

	obj = (xf_async_t *)malloc(sizeof(xf_async_t));
	if (obj == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}

	memset(obj, 0, sizeof(xf_async_t));
	obj->io_async.handle = INVALID_HANDLE_VALUE;

	obj->type = XF_ASYNC_MULTIPHASE;
	obj->child_async = NULL;
	xf_monitor_init(&obj->u.multi_phase.lock);
	obj->u.multi_phase.next_phase = 0;
	obj->u.multi_phase.procedure = procedure;
	obj->u.multi_phase.context = context;

	obj->parent = parent;
	if (parent != NULL) {
		parent->child_async = obj;
	}

	if (timeout > 0) {
		err = xf_timeout_schedule(&obj->timeout, timeout, async_timeout, obj);
		if (err != 0)
			goto _ERROR;
		timeout_setup = true;
	}

	xf_async_multipart_run_next_phase(obj, 0);

	return;
_ERROR:
	if (obj != NULL && timeout_setup) {
		xf_timeout_cancel(&obj->timeout);
	}
	free(obj);

	return;
}

static void xf_async_multipart_run_next_phase(xf_async_t *self, xf_error_t error)
{
	if (error != 0) {
		xf_monitor_enter(&self->u.wait.lock);

		xf_error_t err = self->u.multi_phase.procedure(
			self,
			XF_ASYNC_PHASE_ERROR,
			error,
			self->u.multi_phase.context,
			/*out*/&self->u.multi_phase.next_phase);

		xf_monitor_leave(&self->u.wait.lock);

		xf_async_notify(self->parent, err);
		xf_async_release(self);
	} else {
		xf_monitor_enter(&self->u.wait.lock);

		xf_error_t err = self->u.multi_phase.procedure(
			self,
			self->u.multi_phase.next_phase,
			0,
			self->u.multi_phase.context,
			/*out*/&self->u.multi_phase.next_phase);

		xf_monitor_leave(&self->u.wait.lock);

		if (err != 0) {
			xf_async_multipart_run_next_phase(self, err);
			return;
		}

		if (self->u.multi_phase.next_phase == XF_ASYNC_PHASE_EXIT) {
			xf_async_notify(self->parent, 0);
			xf_async_release(self);
			return;
		}
	}
}
