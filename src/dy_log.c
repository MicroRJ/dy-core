#include "dy.h"

#include <stdarg.h>
#include <stdio.h>

#define DY_LOG_MESSAGE_CAPACITY 4096

static dy_Log_Sink *dy_log_sink;
static void *dy_log_sink_context;

static dy_String dy_log_level_name(dy_Log_Level level)
{
	switch (level)
	{
		case DY_LOG_DEBUG:   return DY_LIT("debug");
		case DY_LOG_INFO:    return DY_LIT("info");
		case DY_LOG_WARNING: return DY_LIT("warning");
		case DY_LOG_ERROR:   return DY_LIT("error");
	}
	return DY_LIT("unknown");
}

static void dy_log_write(dy_String text)
{
	dy_u64 written;
	if (text.size) dy_write_console(DY_STANDARD_ERROR, text.data, text.size, &written);
}

static void dy_log_default_sink(void *context, dy_Log_Level level,
	dy_String subsystem, dy_String message)
{
	(void)context;
	dy_log_write(DY_LIT("["));
	dy_log_write(dy_log_level_name(level));
	dy_log_write(DY_LIT("]"));
	if (subsystem.size)
	{
		dy_log_write(DY_LIT(" "));
		dy_log_write(subsystem);
		dy_log_write(DY_LIT(":"));
	}
	dy_log_write(DY_LIT(" "));
	dy_log_write(message);
	dy_log_write(DY_LIT("\n"));
}

void dy_log_set_sink(dy_Log_Sink *sink, void *context)
{
	dy_log_sink = sink;
	dy_log_sink_context = context;
}

void dy_log(dy_Log_Level level, dy_String subsystem, dy_String message)
{
	dy_Log_Sink *sink = dy_log_sink ? dy_log_sink : dy_log_default_sink;
	sink(dy_log_sink_context, level, subsystem, message);
}

void dy_logf(dy_Log_Level level, dy_String subsystem, const char *format, ...)
{
	char buffer[DY_LOG_MESSAGE_CAPACITY];
	va_list arguments;
	va_start(arguments, format);
	int size = vsnprintf(buffer, sizeof(buffer), format, arguments);
	va_end(arguments);
	if (size < 0) return;
	if ((dy_u64)size >= sizeof(buffer)) size = sizeof(buffer) - 1;
	dy_log(level, subsystem, dy_string_from_data(buffer, (dy_u64)size));
}
