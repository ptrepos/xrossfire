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
	xf_monitor_t lock;
	xf_atomic_t ref;
	int type;
	bool canceled;
	xf_timeout_t timeout;
	xf_async_t *parent;
	xf_async_t *child_async;
	union {
		struct {
			bool called;
			xf_async_completed_t completed;
			void *context;
		} async;
		struct {
			int status;
			xf_error_t error;
		} wait;
		struct {
			xf_async_procedure_t procedure;
			int next_phase;
			void *context;
		} multi_phase;
	} u;
};

static void async_timeout(void *context);
static void xf_async_run_next_phase(xf_async_t *self, xf_error_t error);

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
	
	xf_monitor_init(&obj->lock);
	obj->ref = 1;
	obj->type = XF_ASYNC_ASYNC;
	obj->canceled = false;
	obj->child_async = NULL;
	obj->u.async.completed = completed;
	obj->u.async.context = context;

	obj->parent = parent;
	if (parent != NULL) {
		parent->child_async = obj;
	}
	
	if (timeout > 0) {
		xf_atomic_increment(&obj->ref);
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
		xf_monitor_destroy(&obj->lock);
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

	xf_monitor_init(&obj->lock);
	obj->ref = 1;
	obj->type = XF_ASYNC_WAIT;
	obj->canceled = false;
	obj->u.wait.status = 0;
	obj->u.wait.error = 0;

#if defined(_WIN32)
	memset(&obj->io_async, 0, sizeof(obj->io_async));
	obj->io_async.handle = (HANDLE)INVALID_SOCKET;
#endif
	
	if (timeout > 0) {
		xf_atomic_increment(&obj->ref);
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
		xf_monitor_destroy(&obj->lock);
	}
	free(obj);
	
	return err;
}

XROSSFIRE_API xf_async_t *xf_async_add_ref(xf_async_t *self)
{
	if (self == NULL)
		return self;
	xf_atomic_increment(&self->ref);

	return self;
}

XROSSFIRE_API void xf_async_release(xf_async_t *self)
{
	if (self == NULL)
		return;
	if (xf_atomic_decrement(&self->ref) > 0)
		return;

	xf_monitor_enter(&self->lock);

	xf_timeout_cancel(&self->timeout);

	if (self->parent != NULL) {
		self->parent->child_async = NULL;
	}
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		break;
	case XF_ASYNC_WAIT:
		break;
	case XF_ASYNC_MULTIPHASE:
		break;
	}

#if defined(_WIN32)
	xf_io_async_clear(&self->io_async);
#endif

	xf_monitor_leave(&self->lock);

	xf_monitor_destroy(&self->lock);
	
	free(self);
}

XROSSFIRE_API xf_error_t xf_async_wait(xf_async_t *self)
{
	xf_assert(self->type == XF_ASYNC_WAIT);
	
	xf_monitor_enter(&self->lock);
	
	while (self->u.wait.status == 0) {
		xf_monitor_wait(&self->lock, 10 * 1000);
	}
	
	xf_monitor_leave(&self->lock);
	
	return self->u.wait.error;
}

XROSSFIRE_API bool xf_async_is_canceled(xf_async_t *self)
{
	return self->canceled;
}

static void _xf_async_async_notify(xf_async_t *self, xf_error_t error)
{
	xf_monitor_enter(&self->lock);

	if (self->u.async.called == false) {
		self->u.async.called = true;
		xf_monitor_leave(&self->lock);

		self->u.async.completed(error, self->u.async.context);
	} else {
		xf_monitor_leave(&self->lock);
		return;
	}

	xf_async_release(self);
}

XROSSFIRE_API bool xf_async_enter(xf_async_t *self)
{
	xf_monitor_enter(&self->lock);

	if (self->canceled) {
		xf_async_notify(self, XF_ERROR_CANCEL);
		xf_monitor_leave(&self->lock);
		return false;
	}

	xf_atomic_increment(&self->ref);

	return true;
}

XROSSFIRE_API void xf_async_leave(xf_async_t *self)
{
	xf_monitor_leave(&self->lock);
	xf_async_release(self);
}

static void _xf_async_wait_notify(xf_async_t *self, xf_error_t error)
{
	xf_monitor_enter(&self->lock);
	
	self->u.wait.status = 1;
	self->u.wait.error = error;
	xf_monitor_notify(&self->lock);
	
	xf_monitor_leave(&self->lock);
}

static void _xf_async_multiphase_notify(xf_async_t *self, xf_error_t error)
{
	xf_async_run_next_phase(self, error);
}

XROSSFIRE_API void xf_async_notify(xf_async_t *self, xf_error_t error)
{
	xf_error_t err;

	if (xf_timeout_cancel(&self->timeout) == true) {
		xf_async_release(self);
	}
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		_xf_async_async_notify(self, error);
		break;
	case XF_ASYNC_WAIT:
		_xf_async_wait_notify(self, error);
		break;
	case XF_ASYNC_MULTIPHASE:
		_xf_async_multiphase_notify(self, error);
		break;
	default:
		err = XF_ERROR;
		break;
	}
}

static void _cancel(xf_async_t *self)
{
	xf_monitor_enter(&self->lock);

	self->canceled = true;

	if (self->child_async != NULL) {
		xf_async_cancel(self->child_async);
		xf_monitor_leave(&self->lock);
		return;
	}

#if defined(_WIN32)
	if (self->io_async.handle != (HANDLE)INVALID_SOCKET) {
		bool ret = xf_io_async_cancel(&self->io_async);
		xf_monitor_leave(&self->lock);

		if (ret == false) {
			xf_async_notify(self, XF_ERROR_CANCEL);
		}

		return;
	}
#endif

	xf_monitor_leave(&self->lock);

	xf_async_notify(self, XF_ERROR_CANCEL);
}

XROSSFIRE_API xf_error_t xf_async_cancel(xf_async_t *self)
{
	if (xf_timeout_cancel(&self->timeout) == true) {
		xf_async_release(self);
	}

	_cancel(self);

	return 0;
}

static void async_timeout(void *context)
{
	xf_async_t *async = (xf_async_t*)context;

	_cancel(async);

	xf_async_release(async);
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

	xf_monitor_init(&obj->lock);
	obj->ref = 1;
	obj->type = XF_ASYNC_MULTIPHASE;
	obj->canceled = false;
	obj->child_async = NULL;
	obj->u.multi_phase.next_phase = 0;
	obj->u.multi_phase.procedure = procedure;
	obj->u.multi_phase.context = context;

	obj->parent = parent;
	if (parent != NULL) {
		parent->child_async = obj;
	}

	if (timeout > 0) {
		xf_atomic_increment(&obj->ref);
		err = xf_timeout_schedule(&obj->timeout, timeout, async_timeout, obj);
		if (err != 0)
			goto _ERROR;
		timeout_setup = true;
	}

	xf_async_run_next_phase(obj, 0);

	return;
_ERROR:
	if (obj != NULL && timeout_setup) {
		xf_timeout_cancel(&obj->timeout);
	}
	if (obj != NULL) {
		xf_monitor_destroy(&obj->lock);
	}
	free(obj);

	return;
}

static void xf_async_run_next_phase(xf_async_t *self, xf_error_t error)
{
	if (error != 0) {
		xf_error_t err = self->u.multi_phase.procedure(
			self,
			XF_ASYNC_PHASE_ERROR,
			error,
			self->u.multi_phase.context,
			/*out*/&self->u.multi_phase.next_phase);

		xf_async_notify(self->parent, err);

		xf_async_release(self);
	} else {
		xf_error_t err = self->u.multi_phase.procedure(
			self,
			self->u.multi_phase.next_phase,
			0,
			self->u.multi_phase.context,
			/*out*/&self->u.multi_phase.next_phase);

		if (err != 0) {
			xf_async_run_next_phase(self, err);
			return;
		}

		if (self->u.multi_phase.next_phase == XF_ASYNC_PHASE_EXIT) {
			xf_async_notify(self->parent, 0);
			xf_async_release(self);
			return;
		}
	}
}
