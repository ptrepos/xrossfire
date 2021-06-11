#pragma once

#if defined(_WIN32)

long long xf_ticks()
{
	return (long long)GetTickCount64();
}

typedef struct xf_monitor
{
	CRITICAL_SECTION cs;
	CONDITION_VARIABLE cond_var;
} xf_monitor_t;

static void xf_monitor_init(xf_monitor_t *self)
{
	InitializeCriticalSection(&self->cs);
	InitializeConditionVariable(&self->cond_var);
}

static void xf_monitor_destroy(xf_monitor_t *self)
{
	DeleteCriticalSection(&self->cs);
}

static void xf_monitor_enter(xf_monitor_t *self)
{
	EnterCriticalSection(&self->cs);
}

static void xf_monitor_leave(xf_monitor_t *self)
{
	LeaveCriticalSection(&self->cs);
}

static bool xf_monitor_try_enter(xf_monitor_t *self)
{
	return TryEnterCriticalSection(&self->cs);
}

static void xf_monitor_wait(xf_monitor_t *self, int timeout)
{
	SleepConditionVariableCS(&self->cond_var, &self->cs, (DWORD)timeout);
}

static void xf_monitor_notify(xf_monitor_t *self, int timeout)
{
	WakeConditionVariableCS(&self->cond_var);
}

static void xf_monitor_notify_all(xf_monitor_t *self, int timeout)
{
	WakeAllConditionVariableCS(&self->cond_var);
}

#define xf_thread_start(procedure, context) _beginthread(procedure, 0, context)

#define xf_atomic_t LONG
#define xf_atomic_increment	InterlockedIncrement
#define xf_atomic_decrement	InterlockedDecrement
#define xf_atomic_exchange	InterlockedExchange
#define xf_atomic_compare_exchange	InterlockedCompareExchange

#elif defined(__linux__) || defined(__FreeBSD__)

long long xf_ticks()
{
	struct timeval val;
	struct timezone tz;
	
	gettimeofday(&val, &tz);
	
	return (long long)val.tv_sec * 1000 + (long long)val.tv_usec / 1000;
}

#else
#error "Not supported architecture."
#endif

typedef struct xf_string {
	xf_atomic_t ref;
	int length;
	char buf[];
} xf_string_t;

#include XF_STRING_INITIALIZER(text) { -1, sizeof(text), text }
