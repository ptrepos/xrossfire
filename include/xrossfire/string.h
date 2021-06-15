#pragma once

#include <xrossfire/base.h>
#include <xrossfire/error.h>

XROSSFIRE_API xf_error_t xf_string_new(xf_char_t *chars, int length, xf_string_t **self);
XROSSFIRE_API void xf_string_release(xf_string_t *self);
XROSSFIRE_API void xf_string_add_ref(xf_string_t *self);
XROSSFIRE_API int xf_string_get_ref(xf_string_t *self);

XROSSFIRE_API const xf_char_t *xf_string_to_cstr(xf_string_t *self);
XROSSFIRE_API int xf_string_get_length(xf_string_t *self);
