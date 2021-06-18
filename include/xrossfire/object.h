#pragma once

#include <xrossfire/base.h>
#include <xrossfire/error.h>

typedef struct xf_object	xf_object_t;

typedef xf_error_t (*xf_object_procedure_t)(xf_object_t *self, int message_id, void *args);

typedef struct xf_empty_args	xf_empty_args_t;

#define XF_INTERFACE_UNKNOWN		0
#define XF_INTERFACE_STREAM			1
#define XF_INTERFACE_SOCKET			2
#define XF_INTERFACE_SERVER_SOCKET	3

#define XF_MESSAGE(interface_type, message_id)	((interface_type) << 8 | (message_id))

#define XF_MESSAGE_OBJECT_QUERY_INTERFACE	XF_MESSAGE(XF_INTERFACE_UNKNOWN, 1)
#define XF_MESSAGE_OBJECT_GET_CLASS_ID		XF_MESSAGE(XF_INTERFACE_UNKNOWN, 2)
#define XF_MESSAGE_OBJECT_DESTROY			XF_MESSAGE(XF_INTERFACE_UNKNOWN, 3)

typedef struct xf_object_query_interface_args
{
	int interface_type;
	bool *provides;
} xf_object_query_interface_args_t;

typedef struct xf_object_get_class_id_args
{
	xf_string_t **id;
} xf_object_get_class_id_args_t;

XROSSFIRE_API xf_error_t xf_object_default_procedure(xf_object_t *self, int message_id, void *args);

XROSSFIRE_API xf_error_t xf_object_new(xf_object_procedure_t procedure, size_t body_size, xf_object_t **self);
XROSSFIRE_API void xf_object_release(xf_object_t *self);
XROSSFIRE_API xf_object_t *xf_object_add_ref(xf_object_t *self);

XROSSFIRE_API void *xf_object_get_body(xf_object_t *self);
XROSSFIRE_API xf_error_t xf_object_call(xf_object_t *self, int message_id, void *args);

XROSSFIRE_API void xf_object_set_data(xf_object_t *self, void *data);
XROSSFIRE_API void *xf_object_get_data(xf_object_t *self);

XROSSFIRE_API xf_error_t xf_object_query_interface(xf_object_t *self, int interface_type, bool *includes);
XROSSFIRE_API xf_error_t xf_object_get_class_id(xf_object_t *self, xf_string_t **id);