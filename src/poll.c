#include <xrossfire/poll.h>

static xf_monitor_t lock;
static int n_pollings = 0;
static xf_poll_procedure_t pollings[16];

XROSSFIRE_PRIVATE xf_error_t xf_poll_init()
{
	xf_monitor_init(&lock);

	return 0;
}

static long long xf_poll_get_wait_timeout()
{
	long long min_timeout = 300 * 1000;
	
	for (int i = 0; i < n_pollings; i++) {
		Xf_poll_get_wait_timeout_args_t args = {0};
		pollings[i](XF_POLLING_GET_WAIT_TIMEOUT, &args);
		
		if (min_timeout > args.timeout) {
			min_timeout = args.timeout;
		}
	}
	
	return min_timeout;
}

static void xf_poll_process()
{
	for (int i = 0; i < n_pollings; i++) {
		xf_poll_process_args_t args;
		pollings[i](XF_POLLING_PROCESS, &args);
	}
}

XROSSFIRE_API xf_error_t xf_poll_add(xf_poll_procedure_t obj)
{
	pollings[n_pollings++] = obj;
	return 0;
}

XROSSFIRE_API void xf_poll_remove(xf_poll_procedure_t obj)
{
	for (int i = 0; i < n_pollings; i++) {
		if (pollings[i] == obj) {
			for (int j = i; j + 1 < n_pollings; j++) {
				pollings[j] = pollings[j+1];
			}
			n_pollings--;
			return;
		}
	}
}

XROSSFIRE_API void xf_poll_loop()
{
	xf_monitor_enter(&lock);
	
	for (;;) {
		long long timeout = xf_poll_get_wait_timeout();
		
		if (timeout > 0) {
			xf_monitor_wait(&lock, (int)timeout);
		} else {
			xf_poll_process();
		}
	}

	xf_monitor_leave(&lock);
}

XROSSFIRE_API void xf_poll_enter()
{
	xf_monitor_enter(&lock);
}

XROSSFIRE_API void xf_poll_leave()
{
	xf_monitor_leave(&lock);
}

XROSSFIRE_API void xf_poll_wakeup()
{
	xf_monitor_notify(&lock);
}

static void xf_poll_thread_main(void *p)
{
	xf_poll_loop();
}

XROSSFIRE_API xf_error_t xf_poll_start()
{
	xf_thread_start(xf_poll_thread_main, NULL);

	return 0;
}
