#pragma once

typedef xf_timeout_handle
{
	long long id;
	long long timestamp;
} xf_timeout_handle_t;

typedef struct xf_timeout
{
	xf_timeout_t *left;
	xf_timeout_t *right;
	int height;
	xf_timeout_handle_t key;
	xf_timeout_procedure_t procedure;
	void *context;
} xf_timeout_t;

XROSSFIRE_API xf_error_t xf_timeout_schedule(
	xf_timeout_t *self,
	int timeout, 
	xf_timeout_procedure_t *procedure, 
	void *context);
XROSSFIRE_API xf_error_t xf_timeout_cancel(xf_timeout_t *self);
