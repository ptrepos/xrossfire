#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>

#include <process.h>
#endif

#include <stdbool.h>

#if defined(__cplusplus)
#define XF_EXTERN_C			extern "C" {
#define XF_END_EXTERN_C		}
#else
#define XF_EXTERN_C
#define XF_END_EXTERN_C	
#endif

#if !defined(_countof)
#define _countof(array)	(sizeof(array) / sizeof(array[0]))
#endif

#ifndef XF_THREADLOCAL
#	if defined(_WIN32) && defined(_MSC_VER)
#		define XF_THREADLOCAL	__declspec(thread)
#	else
#		error "Not supported architecture"
#	endif
#endif

#ifndef XROSSFIRE_API_EXPORT
#	if defined(_WIN32) && defined(_MSC_VER)
#		define XROSSFIRE_API_EXPORT	__declspec(dllexport)
#	elif defined(_WIN32) && (__GNUC__ >= 4 || defined(__clang__))
#		define XROSSFIRE_API_EXPORT	__attribute__ ((dllexport))
#	elif __GNUC__ >= 4 || defined(__clang__)
#		define XROSSFIRE_API_EXPORT
#	else
#		error "Not supported architecture"
#	endif
#endif

#ifndef XROSSFIRE_API_IMPORT
#	if defined(_WIN32) && defined(_MSC_VER)
#		define XROSSFIRE_API_IMPORT	__declspec(dllimport)
#	elif defined(_WIN32) && (__GNUC__ >= 4 || defined(__clang__))
#		define XROSSFIRE_API_IMPORT	__attribute__ ((dllimport))
#	elif __GNUC__ >= 4 || defined(__clang__)
#		define XROSSFIRE_API_IMPORT
#	else
#		error "Not supported architecture"
#	endif
#endif

#ifndef XROSSFIRE_PRIVATE
#	if defined(_MSC_VER)
#		define XROSSFIRE_PRIVATE	
#	elif __GNUC__ >= 4 || defined(__clang__)
#		define XROSSFIRE_PRIVATE	__attribute__ ((visibility ("hidden")))
#	else
#		define XROSSFIRE_PRIVATE	
#	endif
#endif

#define SHARED_LIB_EXTENSION    ".dll"

#if defined (_LIBXROSSFIRE_DLL)
#define XROSSFIRE_API	XROSSFIRE_API_EXPORT
#else
#define XROSSFIRE_API	XROSSFIRE_API_IMPORT
#endif

XF_EXTERN_C

#if defined(_WIN32)

static long long xf_ticks()
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

static void xf_monitor_notify(xf_monitor_t *self)
{
	WakeConditionVariable(&self->cond_var);
}

static void xf_monitor_notify_all(xf_monitor_t *self)
{
	WakeAllConditionVariable(&self->cond_var);
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

#if defined(_WIN32)
typedef WCHAR	xf_char_t;
#elif defined(__linux__) || define(__FreeBSD__)
typedef char	xf_char_t;
#endif

typedef struct xf_string {
	xf_atomic_t ref;
	int length;
	xf_char_t buf[];
} const xf_string_t;

#define XF_STRING_INITIALIZER(text)		{ -1, _countof(text), text }

#if defined(_WIN32)
#define _T(text)						L ## text
#endif

XF_END_EXTERN_C