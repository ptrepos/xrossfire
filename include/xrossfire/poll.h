#include <xrossfire/base.h>
#include <xrossfire/error.h>

typedef void (*xf_poll_procedure_t)(int method_id, void *args);

#define XF_POLLING_GET_WAIT_TIMEOUT		(1)
#define XF_POLLING_PROCESS				(2)

typedef struct xf_polling_get_wait_timeout_args 
{
	long long timeout;
} Xf_poll_get_wait_timeout_args_t;

typedef struct xf_polling_process_args
{
	long long reserve0;
} xf_poll_process_args_t;

XROSSFIRE_API xf_error_t xf_poll_add(xf_poll_procedure_t procedure);
XROSSFIRE_API void xf_poll_remove(xf_poll_procedure_t procedure);
XROSSFIRE_API void xf_poll_loop();

XROSSFIRE_API void xf_poll_enter();
XROSSFIRE_API void xf_poll_leave();
XROSSFIRE_API void xf_poll_wakeup();
