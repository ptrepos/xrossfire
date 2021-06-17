#pragma once

#include <xrossfire/error.h>

typedef void (*xf_timeout_procedure_t)(void *context);

typedef struct xf_timeout_handle xf_timeout_handle_t;
typedef struct xf_timeout xf_timeout_t;

struct xf_timeout_handle
{
	long long id;
	long long timestamp;
};

struct xf_timeout
{
	xf_timeout_t *left;
	xf_timeout_t *right;
	int height;
	xf_timeout_handle_t key;
	xf_timeout_procedure_t procedure;
	void *context;
};

XROSSFIRE_API xf_error_t xf_timeout_schedule(
	xf_timeout_t *self,
	int timeout, 
	xf_timeout_procedure_t procedure, 
	void *context);
XROSSFIRE_API void xf_timeout_cancel(xf_timeout_t *self);
