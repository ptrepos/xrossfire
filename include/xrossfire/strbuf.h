#pragma once

#include <uchar.h>
#include <xrossfire/base.h>
#include <xrossfire/error.h>

typedef struct xf_strbuf
{
	char *buf;
	int length;
	int capacity;
} xf_strbuf_t;

typedef struct xf_strbuf16
{
	char16_t *buf;
	int length;
	int capacity;
} xf_strbuf16_t;

#define XF_STRBUF_INITIALIZER	{ NULL, 0, 16 }
#define XF_STRBUF_INITIALIZER_LOCAL(buffer)	{ buffer, 0, _countof(buffer) - 1 }

#define XF_STRBUF16_INITIALIZER	{ NULL, 0, 16 }
#define XF_STRBUF16_INITIALIZER_LOCAL(buffer)	{ buffer, 0, _countof(buffer) - 1 }

XROSSFIRE_API void xf_strbuf16_init(xf_strbuf16_t *self);
XROSSFIRE_API void xf_strbuf16_destroy(xf_strbuf16_t *self);

XROSSFIRE_API char16_t *xf_strbuf16_to_cstr(xf_strbuf16_t *self);
XROSSFIRE_API int xf_strbuf16_get_length(xf_strbuf16_t *self);

XROSSFIRE_API xf_error_t xf_strbuf16_append_char(xf_strbuf16_t *self, char16_t ch);
XROSSFIRE_API xf_error_t xf_strbuf16_append_chars(xf_strbuf16_t *self, char16_t *chars, int length);
