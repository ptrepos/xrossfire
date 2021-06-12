#include <xrossfire/error.h>
#include <xrossfire/string.h>

struct xf_error_info
{
	xf_atomic_t ref;
	int code;
	xf_string_t *type;
	xf_string_t *message;
	void *content;
	xf_error_info_t *cause;
};

static xf_string_t TYPE_XROSSFIRE = XF_STRING_INITIALIZER("xrossfire");
static xf_string_t TYPE_WIN32 = XF_STRING_INITIALIZER("win32");
static xf_string_t TYPE_C = XF_STRING_INITIALIZER("c");

XROSSFIRE_API xf_string_t *xf_error_type_xrossfire()
{
	return &TYPE_XROSSFIRE;
}

XROSSFIRE_API xf_string_t *xf_error_type_win32()
{
	return &TYPE_WIN32;
}

XROSSFIRE_API xf_string_t *xf_error_type_c()
{
	return &TYPE_C;
}


//--------------------------------------------
// error_info
static XF_THREADLOCAL xf_error_info_t *current;

static xf_string_t MESSAGE_OUT_OF_MEMORY = XF_STRING_INITIALIZER("Out of memory.");

static xf_error_info_t OUT_OF_MEMORY = {
	-1,
	1,
	&TYPE_XROSSFIRE,
	&MESSAGE_OUT_OF_MEMORY,
	NULL,
	NULL
};

XROSSFIRE_API xf_error_info_t *xf_error_info_add_ref(xf_error_info_t *self)
{
	if (self == NULL || self == &OUT_OF_MEMORY)
		return self;
	
	xf_atomic_increment(&self->ref);
	
	return self;
}

XROSSFIRE_API void xf_error_info_release(xf_error_info_t *self)
{
	if (self == NULL || self == &OUT_OF_MEMORY)
		return;
	
	xf_string_release(self->type);
	xf_string_release(self->message);
	xf_error_info_release(self->cause);
	free(self);
}

XROSSFIRE_API xf_error_info_t *xf_error_info_get()
{
	return current;
}

XROSSFIRE_API xf_error_t xf_error_set(
	xf_string_t *type, 
	int code, 
	xf_string_t *message,
	xf_error_info_t *cause,
	void *content)
{
	xf_error_info_t *self = NULL;
	
	self = (xf_error_info_t*)malloc(sizeof(xf_error_info_t));
	if (self == NULL) {
		return xf_error_set_info(&OUT_OF_MEMORY);
	}
	
	self->ref = 0;
	self->type = type;
	self->code = code;
	self->message = message;
	self->cause = cause;
	self->content = content;
	
	return xf_error_set_info(self);
}

XROSSFIRE_API xf_error_t xf_error_set_info(xf_error_info_t *info)
{
	xf_error_info_release(current);
	current = xf_error_info_add_ref(info);

	return XF_ERROR;
}

XROSSFIRE_API xf_string_t *xf_error_info_get_type(xf_error_info_t *self)
{
	return self->type;
}

XROSSFIRE_API int xf_error_info_get_code(xf_error_info_t *self)
{
	return self->code;
}

XROSSFIRE_API xf_string_t *xf_error_info_get_message(xf_error_info_t *self)
{
	return self->message;
}

XROSSFIRE_API void *xf_error_info_get_content(xf_error_info_t *self)
{
	return self->content;
}

XROSSFIRE_API xf_error_info_t *xf_error_info_get_cause(xf_error_info_t *self)
{
	return self->cause;
}

XROSSFIRE_API xf_error_t xf_error_libc(int error_num)
{
	return XF_ERROR;
}

XROSSFIRE_API xf_error_t xf_error_windows(int error_num)
{
	return XF_ERROR;
}