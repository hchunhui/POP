#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include "xlog.h"

static int verbose_level = XLOG_INFO;

void xlog(int level, char *fmt, ...)
{
	va_list args;
	if(level < verbose_level)
		return;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void xlog_set_verbose(int level)
{
	verbose_level = level;
}

int xlog_get_verbose(void)
{
	return verbose_level;
}
