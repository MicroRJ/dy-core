#ifndef DY_LOG_H
#define DY_LOG_H

#include "dy.h"

typedef enum dy_Log_Level
{
	DY_LOG_DEBUG,
	DY_LOG_INFO,
	DY_LOG_WARNING,
	DY_LOG_ERROR,
}
dy_Log_Level;

typedef void dy_Log_Sink(void *context, dy_Log_Level level, dy_String subsystem, dy_String message);

// Setting a null sink restores the default standard-error sink. Configure the sink during startup; changing it concurrently with logging is unsupported.
void dy_log_set_sink(dy_Log_Sink *sink, void *context);
void dy_log(dy_Log_Level level, dy_String subsystem, dy_String message);
void dy_logf(dy_Log_Level level, dy_String subsystem, const char *format, ...);

#endif
