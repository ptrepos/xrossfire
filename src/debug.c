#include <stdio.h>
#include <xrossfire/base.h>
#include <xrossfire/error.h>

XROSSFIRE_API XF_NORETURN void __xf_debug_assert(const char *text)
{
#if defined(_WIN32)
	DebugBreak();
#endif

	fprintf(stderr, "ASSERT: %s\n", text);

	abort();
}

XROSSFIRE_API XF_NORETURN void __xf_debug_abort()
{
#if defined(_WIN32)
	DebugBreak();
#endif

	fprintf(stderr, "ABORT: Abnormal terminate.\n");

	abort();
}
