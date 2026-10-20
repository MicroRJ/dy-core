#ifndef DY_PROCESS_H
#define DY_PROCESS_H

#include "dy_io.h"

typedef struct dy_Env_Field
{
	dy_String name;
	dy_String value;
}
dy_Env_Field;

typedef struct dy_Env_Table
{
	dy_Env_Field *items;
	dy_u32 count;
}
dy_Env_Table;

typedef struct dy_Process
{
	dy_uptr handle;
}
dy_Process;

typedef struct dy_Process_Start_Options
{
	// The executable is also exposed to the child as argv[0]. The arguments array contains only the values that follow argv[0].
	dy_String executable;
	dy_String_Array arguments;
	dy_String working_directory;
	dy_Stream *standard_input;
	dy_Stream *standard_output;
	dy_Stream *standard_error;
	dy_b32 hide_window;
}
dy_Process_Start_Options;

typedef enum dy_Process_Status
{
	DY_PROCESS_RUNNING,
	DY_PROCESS_EXITED,
}
dy_Process_Status;

dy_Result dy_get_env_field(dy_Arena *arena, dy_String name, dy_String *value);
dy_Result dy_get_env_table(dy_Arena *arena, dy_Env_Table *table);
dy_Result dy_set_env_field(dy_String name, dy_String value);
dy_Result dy_remove_env_field(dy_String name);

dy_b32 dy_process_is_valid(dy_Process process);
// Process start borrows all streams and duplicates their native handles for the child. The caller retains ownership and should close unused pipe ends.
dy_Result dy_process_start(dy_Process_Start_Options options, dy_Process *process);
dy_Result dy_process_poll(dy_Process process, dy_Process_Status *status, dy_u32 *exit_code);
dy_Result dy_process_wait(dy_Process process, dy_u32 milliseconds, dy_Process_Status *status, dy_u32 *exit_code);
dy_Result dy_process_kill(dy_Process process, dy_u32 exit_code);
// Closing releases bookkeeping resources and does not terminate the child.
dy_Result dy_process_close(dy_Process *process);
dy_u64 dy_current_process_id(void);
void dy_exit_process(dy_i32 exit_code);
void dy_debug_break(void);

#endif
