#include <xrossfire/base.h>
#include <xrossfire/error.h>

static xf_string_t _empty = XF_STRING_INITIALIZER(_T(""));

XROSSFIRE_API xf_string_t *xf_string_empty()
{
	return &_empty;
}

XROSSFIRE_API xf_error_t xf_string_new(xf_char_t *chars, int length, xf_string_t **self)
{
	xf_error_t err;
	struct xf_string *p = NULL;

	if (length <= 0) {
		*self = &_empty;
		return 0;
	}

	p = (struct xf_string *)malloc(sizeof(xf_atomic_t) + sizeof(int) + sizeof(xf_char_t) * (length + 1));
	if (p == NULL) {
		err = xf_error_libc(errno);
		goto _ERROR;
	}

	p->ref = 1;
	p->length = length;
	memcpy(p->buf, chars, sizeof(xf_char_t) * length);
	p->buf[length] = 0;

	*self = p;

	return 0;
_ERROR:
	free(p);

	return err;
}

XROSSFIRE_API void xf_string_release(xf_string_t *self)
{
	if (self == NULL || self->ref < 0)
		return;

	if (xf_atomic_decrement(&((struct xf_string *)self)->ref) <= 0) {
		free((void*)self);
	}
}

XROSSFIRE_API xf_string_t *xf_string_add_ref(xf_string_t *self)
{
	if (self == NULL || self->ref < 0)
		return self;

	xf_atomic_increment(&((struct xf_string*)self)->ref);

	return self;
}

XROSSFIRE_API int xf_string_get_ref(xf_string_t *self)
{
	if (self == NULL)
		return 0;
	return self->ref;
}

XROSSFIRE_API const xf_char_t *xf_string_to_cstr(xf_string_t *self)
{
	return self->buf;
}

XROSSFIRE_API int xf_string_get_length(xf_string_t *self)
{
	return self->length;
}

XROSSFIRE_API bool xf_string_equals(xf_string_t *self, xf_string_t *other)
{
	if (self == other)
		return true;
	else if (self == NULL || other == NULL)
		return false;
	else if (self->length != other->length)
		return false;

	return memcmp(self->buf, other->buf, sizeof(xf_char_t) * self->length) == 0;
}

XROSSFIRE_API int xf_string_get_hashcode(xf_string_t *self)
{
	int hash = 0;

	for (int i = 0; i < self->length; i++) {
		hash = hash * 12386131 + self->buf[i];
	}

	return 0;
}