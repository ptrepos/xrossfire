#include <xrossfire/async.h>
#include <xrossfire/timeout.h>

typedef xf_timeout
{
	long long timestamp;
	int id;
} xf_timeout_t;

struct xf_async {
	int type;
	xf_timeout_t timeout;
	void *control;
	xf_async_control_procedure_t control_proc;
	union {
		struct {
			xf_async_procedure_t completed;
			void *context;
		} async;
		struct {
			;
		} wait;
	} u;
};

static void async_timeout(void *context);

XROSSFIRE_API xf_error_t xf_async_new_1(int timeout, xf_async_procedure_t completed, void *context, xf_async_t **self)
{
	xf_error_t err;
	xf_async_t *obj = NULL;
	
	obj = (xf_async_t*)malloc(sizeof(xf_async_t));
	if (obj == NULL) {
		err = xf_error_stdc(errno);
		goto _ERROR;
	}
	
	obj->type = XF_ASYNC_ASYNC;
	obj->u.async.completed = completed;
	obj->u.async.context = context;
	obj->timeout = xf_timeout_invalid();
	
	if (timeout > 0) {
		err = xf_timeout_schedule(timeout, async_timeout, context, &obj->timeout);
		if (err != 0)
			goto _ERROR;
	}
	
	*self = obj;
	
	return 0;
_ERROR:
	if (obj != NULL) {
		xf_timeout_cancel(obj->timeout);
	}
	free(obj);
	
	return err;
}

XROSSFIRE_API xf_error_t xf_async_release(xf_async_t *self)
{
	xf_timeout_cancel(self->timeout);
	free(self);
}

XROSSFIRE_API xf_error_t _xf_async_async_notify(xf_async_t *self, xf_error_t error)
{
	self->u.async.competed(error, obj->u.async.context);
	
	return 0;
}

XROSSFIRE_API xf_error_t xf_async_notify(xf_async_t *self, xf_error_t error)
{
	xf_error_t err;
	
	switch (self->type) {
	case XF_ASYNC_ASYNC:
		err = _xf_async_async_notify(self, error);
		break;
	default:
		err = XF_E_NOT_SUPPORTED;
		break;
	}
	
	return err;
}

XROSSFIRE_API xf_error_t xf_async_cancel(xf_async_t *self, int status)
{
	xf_error_t err;
	
	err = xf_timeout_cancel(self->timeout);
	if (err != 0)
		goto _ERROR;
	
	err = self->control_proc(self->control, XF_ASYNC_CONTROL_METHOD_CANCEL, NULL);
	if (err != 0)
		goto _ERROR;

	return 0;
_ERROR:
	
	return err;
}

static void async_timeout(void *context)
{
	xf_async_t *async = (xf_async_t*)context;
	
	self->control_proc(self->control, XF_ASYNC_CONTROL_METHOD_CANCEL, NULL);
}
