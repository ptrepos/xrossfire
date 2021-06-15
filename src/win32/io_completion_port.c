#include "io_completion_port.h"

static xf_monitor_t g_lock;
static int g_max_workers = 8;
static int g_current_workers;
static bool g_exited = false;
static HANDLE g_hIOCP = INVALID_HANDLE_VALUE;

static xf_error_t prepare_thread();
static void io_compiletion_main(PVOID pv);

XROSSFIRE_PRIVATE xf_error_t xf_io_completion_port_init()
{
    xf_error_t err;

    xf_monitor_init(&g_lock);

    HANDLE hIOCPTmp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, g_hIOCP, 0, g_max_workers);
    if (hIOCPTmp == NULL) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }

    g_hIOCP = hIOCPTmp;

    return 0;
_ERROR:
    return err;
}

static xf_error_t prepare_thread()
{
    xf_error_t err;

    if (g_current_workers < g_max_workers) {
        for (int i = g_current_workers; i < g_max_workers; i++) {
            uintptr_t pthread = xf_thread_start(io_compiletion_main, NULL);
            if (pthread == (uintptr_t)INVALID_HANDLE_VALUE) {
                err = xf_error_libc(errno);
                goto _ERROR;
            }
        }
    } else if (g_max_workers < g_current_workers) {
        for (int i = g_max_workers; i < g_current_workers; i++) {
            PostQueuedCompletionStatus(g_hIOCP, 0, XF_IO_TYPE_EXIT, NULL);
        }
    }

    return 0;
_ERROR:
    return err;
}

XROSSFIRE_PRIVATE xf_error_t xf_io_completion_port_register(HANDLE handle)
{
    xf_error_t err;

    HANDLE hIOCPTmp = CreateIoCompletionPort(handle, g_hIOCP, XF_IO_TYPE_HANDLE, g_max_workers);
    if (hIOCPTmp == NULL) {
        err = xf_error_windows(GetLastError());
        goto _ERROR;
    }
    g_hIOCP = hIOCPTmp;

    err = prepare_thread();
    if (err != 0)
        goto _ERROR;

    return 0;
_ERROR:
    return err;
}

XROSSFIRE_API xf_error_t xf_io_set_max_workers(int max_workers)
{
    xf_monitor_enter(&g_lock);

    g_max_workers = max_workers;

    xf_monitor_leave(&g_lock);

    return 0;
}

XROSSFIRE_PRIVATE void xf_io_command_cancel(xf_io_command_t *self)
{
	CancelIoEx(self->handle, &self->head.overlapped);
}

static void io_compiletion_main(PVOID pv)
{
    DWORD transfered_bytes = 0;
    ULONG_PTR completion_key;
    LPOVERLAPPED overlapped = NULL;
    DWORD derr;
    xf_io_command_t *command;

    xf_monitor_enter(&g_lock);
    g_current_workers++;
    xf_monitor_leave(&g_lock);

    for (;;) {
        derr = 0;

        BOOL bRet = GetQueuedCompletionStatus(
            g_hIOCP,
            &transfered_bytes,
            &completion_key,
            &overlapped,
            INFINITE);
        if (!bRet) {
            derr = GetLastError();
        }

        switch (completion_key) {
        case XF_IO_TYPE_EXIT:
            xf_monitor_enter(&g_lock);
            if (g_current_workers > g_max_workers || g_exited) {
                g_current_workers--;
                xf_monitor_leave(&g_lock);
                goto _EXIT_WHILE;
            }
            xf_monitor_leave(&g_lock);
            break;
        case XF_IO_TYPE_HANDLE:
            command = (xf_io_command_t*)overlapped;
            switch (command->io_type)
            {
            case XF_IO_SOCKET_CONNECT_PHASE2:
                xf_io_completed_socket_connect_phase2(derr, command);
                break;
            case XF_IO_SOCKET_DISCONNECT:
                xf_io_completed_socket_disconnect(derr, command);
                break;
            case XF_IO_SOCKET_RECEIVE:
                xf_io_completed_socket_receive(derr, command);
                break;
            case XF_IO_SOCKET_SEND:
                xf_io_completed_socket_send(derr, command);
                break;
            }
            break;
        }
    }
_EXIT_WHILE:
    ;
}
