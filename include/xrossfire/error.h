#pragma once

#if defined(_WIN32)
#include <windows.h>
#endif

#include <errno.h>
#include <xrossfire/base.h>

typedef int xf_error_t;

#define XF_ERROR	(1)

XROSSFIRE_API xf_string_t *xf_error_type_xrossfire();
XROSSFIRE_API xf_string_t *xf_error_type_win32();
XROSSFIRE_API xf_string_t *xf_error_type_c();

typedef struct xf_error_info xf_error_info_t;

XROSSFIRE_API xf_error_info_t *xf_error_info_add_ref(xf_error_info_t *self);
XROSSFIRE_API void xf_error_info_release(xf_error_info_t *self);

XROSSFIRE_API xf_error_info_t *xf_error_info_get();

XROSSFIRE_API xf_string_t *xf_error_info_get_type(xf_error_info_t *self);
XROSSFIRE_API int xf_error_info_get_code(xf_error_info_t *self);
XROSSFIRE_API xf_string_t *xf_error_info_get_message(xf_error_info_t *self);
XROSSFIRE_API void *xf_error_info_get_content(xf_error_info_t *self);
XROSSFIRE_API xf_error_info_t *xf_error_info_get_cause(xf_error_info_t *self);

XROSSFIRE_API xr_error_t xf_error_set(
	xf_string_t *type, 
	int code, 
	xf_string_t *message,
	xf_error_info_t *cause,
	void *content);
XROSSFIRE_API xr_error_t xf_error_set_info(xf_error_info_t *info);
XROSSFIRE_API xr_error_t xf_error_win32(DWORD error);
XROSSFIRE_API xr_error_t xf_error_stdc(int error);
