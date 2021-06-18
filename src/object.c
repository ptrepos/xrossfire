#include <xrossfire/object.h>

struct xf_object
{
	xf_object_procedure_t procedure;
	xf_atomic_t ref;
	void *data;
};

XROSSFIRE_API xf_error_t xf_object_new(xf_object_procedure_t procedure, size_t body_size, xf_object_t **self)
{
	xf_error_t err;
	xf_object_t *o = NULL;

	o = (xf_object_t *)malloc(sizeof(xf_object_t) + body_size);
	if (o == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}

	o->procedure = procedure;
	o->ref = 1;
	o->data = NULL;

	*self = o;

	return 0;
_ERROR:
	free(o);

	return err;
}

XROSSFIRE_API void xf_object_release(xf_object_t *self)
{
	if (self == NULL)
		return;
	if (xf_atomic_decrement(&self->ref) > 0) {
		return;
	}

	xf_object_call(self, XF_MESSAGE_OBJECT_DESTROY, NULL);

	free(self);
}

XROSSFIRE_API xf_object_t *xf_object_add_ref(xf_object_t *self)
{
	if (self == NULL) {
		return NULL;
	}

	xf_atomic_increment(&self->ref);

	return self;
}

XROSSFIRE_API void *xf_object_get_body(xf_object_t *self)
{
	return (void*)(self + 1);
}

XROSSFIRE_API xf_error_t xf_object_call(xf_object_t *self, int message_id, void *args)
{
	return self->procedure(self, message_id, args);
}

XROSSFIRE_API xf_error_t xf_object_default_procedure(xf_object_t *obj, int message_id, void *args)
{
	return XF_ERROR;
}

XROSSFIRE_API xf_error_t xf_object_query_interface(xf_object_t *self, int interface_type, bool *provides)
{
	xf_object_query_interface_args_t args;
	args.interface_type = interface_type;
	args.provides = provides;

	return xf_object_call(self, XF_MESSAGE_OBJECT_QUERY_INTERFACE, &args);
}

XROSSFIRE_API xf_error_t xf_object_get_class_id(xf_object_t *self, xf_string_t **id)
{
	xf_object_get_class_id_args_t args;
	args.id = id;

	return xf_object_call(self, XF_MESSAGE_OBJECT_GET_CLASS_ID, &args);
}

XROSSFIRE_API void xf_object_set_data(xf_object_t *self, void *data)
{
	self->data = data;
}

XROSSFIRE_API void *xf_object_get_data(xf_object_t *self)
{
	return self->data;
}
