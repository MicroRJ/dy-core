#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dy_process.h"
#include "../dy_stream_internal.h"
#include "dy_win32.h"

static void dy_win32_close_handle(HANDLE *handle)
{
	if (*handle && *handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(*handle);
	}
	*handle = NULL;
}

// Quote argv using the parsing rules used by CommandLineToArgvW and the
// Microsoft C runtime. Quoting every item keeps construction predictable.
static char *dy_win32_command_line(dy_Arena *arena, dy_String executable, dy_String_Array arguments)
{
	char *result = NULL;
	dy_b32 valid = arguments.count != UINT32_MAX;
	dy_u32 argument_count = valid ? arguments.count + 1 : 0;
	dy_u64 capacity = 1;
	for (dy_u32 index = 0; index < argument_count && valid; ++index)
	{
		dy_String argument = index == 0 ? executable : arguments.items[index - 1];
		valid = argument.data || argument.size == 0;
		valid = valid && !dy_string_contains(argument, 0);
		valid = valid && capacity <= UINT64_MAX - 3;
		valid = valid && argument.size <= (UINT64_MAX - capacity - 3) / 2;
		if (valid)
		{
			capacity += argument.size * 2 + 3;
		}
	}
	if (valid)
	{
		result = dy_arena_reserve(arena, capacity);
	}
	if (result)
	{
		char *out = result;
		for (dy_u32 index = 0; index < argument_count; ++index)
		{
			dy_String argument = index == 0 ? executable : arguments.items[index - 1];
			if (index)
			{
				*out++ = ' ';
			}
			*out++ = '"';
			for (dy_u64 offset = 0; offset < argument.size;)
			{
				dy_u64 slash_count = 0;
				while (offset < argument.size && argument.data[offset] == '\\')
				{
					++slash_count;
					++offset;
				}
				if (offset == argument.size)
				{
					for (dy_u64 slash = 0; slash < slash_count * 2; ++slash)
					{
						*out++ = '\\';
					}
				}
				else
				{
					if (argument.data[offset] == '"')
					{
						for (dy_u64 slash = 0; slash < slash_count * 2 + 1; ++slash)
						{
							*out++ = '\\';
						}
						*out++ = '"';
					}
					else
					{
						for (dy_u64 slash = 0; slash < slash_count; ++slash)
						{
							*out++ = '\\';
						}
						*out++ = argument.data[offset];
					}
					++offset;
				}
			}
			*out++ = '"';
		}
		*out++ = 0;
		arena->used += (dy_u64)(out - result);
	}
	return result;
}

static dy_Result dy_win32_duplicate_process_stream(dy_Stream *stream, DWORD standard_stream, dy_u32 required_capability, DWORD null_access, HANDLE *duplicate)
{
	dy_Result result = {0};
	HANDLE source = NULL;
	HANDLE temporary = NULL;
	*duplicate = NULL;
	if (stream)
	{
		dy_uptr value = 0;
		if (dy_stream_get_native_handle(stream, required_capability, &value))
		{
			source = (HANDLE)value;
		}
		else
		{
			result.error = DY_ERROR_NOT_SUPPORTED;
		}
	}
	else
	{
		source = GetStdHandle(standard_stream);
		if (!source || source == INVALID_HANDLE_VALUE)
		{
			temporary = CreateFileA("NUL", null_access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			if (temporary == INVALID_HANDLE_VALUE)
			{
				result = dy_win32_last_error();
			}
			else
			{
				source = temporary;
			}
		}
	}
	if (result.error == DY_ERROR_NONE && !DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), duplicate, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		result = dy_win32_last_error();
	}
	dy_win32_close_handle(&temporary);
	return result;
}

dy_Result dy_process_start(dy_Process_Start_Options options, dy_Process *process_result)
{
	dy_Result result = {0};
	if (process_result)
	{
		*process_result = (dy_Process){0};
	}
	dy_b32 options_valid = process_result && options.executable.data && options.executable.size != 0 && !dy_string_contains(options.executable, 0) && (options.arguments.count == 0 || options.arguments.items);
	for (dy_u32 index = 0; index < options.arguments.count && options_valid; ++index)
	{
		dy_String argument = options.arguments.items[index];
		options_valid = (argument.data || argument.size == 0) && !dy_string_contains(argument, 0);
	}
	options_valid = options_valid && (options.working_directory.data || options.working_directory.size == 0) && !dy_string_contains(options.working_directory, 0);
	if (!options_valid)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		dy_Scratch scratch = dy_begin_scratch();
		char *application = dy_win32_path_text(scratch.arena, options.executable);
		char *working_directory = options.working_directory.size ? dy_win32_path_text(scratch.arena, options.working_directory) : NULL;
		char *command = dy_win32_command_line(scratch.arena, options.executable, options.arguments);
		if (!application || (options.working_directory.size && !working_directory) || !command)
		{
			result.error = DY_ERROR_OUT_OF_MEMORY;
		}
		else
		{
			STARTUPINFOEXA startup = {0};
			startup.StartupInfo.cb = sizeof(startup);
			PROCESS_INFORMATION process = {0};
			HANDLE inherited[3] = {0};
			LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
			dy_b32 attributes_initialized = 0;
			dy_b32 redirect = options.standard_input || options.standard_output || options.standard_error;
			DWORD creation_flags = options.hide_window ? CREATE_NO_WINDOW : 0;
			if (redirect)
			{
				result = dy_win32_duplicate_process_stream(options.standard_input, STD_INPUT_HANDLE, DY_STREAM_CAN_READ, GENERIC_READ, inherited + 0);
				if (result.error == DY_ERROR_NONE)
				{
					result = dy_win32_duplicate_process_stream(options.standard_output, STD_OUTPUT_HANDLE, DY_STREAM_CAN_WRITE, GENERIC_WRITE, inherited + 1);
				}
				if (result.error == DY_ERROR_NONE)
				{
					result = dy_win32_duplicate_process_stream(options.standard_error, STD_ERROR_HANDLE, DY_STREAM_CAN_WRITE, GENERIC_WRITE, inherited + 2);
				}
				SIZE_T attribute_size = 0;
				if (result.error == DY_ERROR_NONE)
				{
					InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
					if (attribute_size == 0)
					{
						result = dy_win32_last_error();
					}
				}
				if (result.error == DY_ERROR_NONE)
				{
					attributes = malloc(attribute_size);
					if (!attributes)
					{
						result.error = DY_ERROR_OUT_OF_MEMORY;
					}
				}
				if (result.error == DY_ERROR_NONE && !InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size))
				{
					result = dy_win32_last_error();
				}
				if (result.error == DY_ERROR_NONE)
				{
					attributes_initialized = 1;
					if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), NULL, NULL))
					{
						result = dy_win32_last_error();
					}
				}
				if (result.error == DY_ERROR_NONE)
				{
					startup.lpAttributeList = attributes;
					startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
					startup.StartupInfo.hStdInput = inherited[0];
					startup.StartupInfo.hStdOutput = inherited[1];
					startup.StartupInfo.hStdError = inherited[2];
					creation_flags |= EXTENDED_STARTUPINFO_PRESENT;
				}
			}
			if (result.error == DY_ERROR_NONE)
			{
				if (CreateProcessA(application, command, NULL, NULL, redirect, creation_flags, NULL, working_directory, &startup.StartupInfo, &process))
				{
					process_result->handle = (dy_uptr)process.hProcess;
					process.hProcess = NULL;
				}
				else
				{
					result = dy_win32_last_error();
				}
			}
			dy_win32_close_handle(&process.hThread);
			dy_win32_close_handle(&process.hProcess);
			for (dy_u32 index = 0; index < DY_ARRAY_COUNT(inherited); ++index)
			{
				dy_win32_close_handle(inherited + index);
			}
			if (attributes_initialized)
			{
				DeleteProcThreadAttributeList(attributes);
			}
			free(attributes);
		}
		dy_end_scratch(scratch);
	}
	return result;
}

dy_b32 dy_process_is_valid(dy_Process process)
{
	return process.handle != 0;
}

dy_Result dy_process_poll(dy_Process process, dy_Process_Status *status, dy_u32 *exit_code)
{
	dy_Result result = {0};
	if (status)
	{
		*status = DY_PROCESS_RUNNING;
	}
	if (exit_code)
	{
		*exit_code = 0;
	}
	if (!status || !exit_code || !dy_process_is_valid(process))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		DWORD code = 0;
		if (!GetExitCodeProcess((HANDLE)process.handle, &code))
		{
			result = dy_win32_last_error();
		}
		else if (code != STILL_ACTIVE)
		{
			*status = DY_PROCESS_EXITED;
			*exit_code = code;
		}
	}
	return result;
}

dy_Result dy_process_wait(dy_Process process, dy_u32 milliseconds, dy_Process_Status *status, dy_u32 *exit_code)
{
	dy_Result result = {0};
	if (status)
	{
		*status = DY_PROCESS_RUNNING;
	}
	if (exit_code)
	{
		*exit_code = 0;
	}
	if (!status || !exit_code || !dy_process_is_valid(process))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		DWORD wait = WaitForSingleObject((HANDLE)process.handle, milliseconds);
		if (wait == WAIT_OBJECT_0)
		{
			result = dy_process_poll(process, status, exit_code);
		}
		else if (wait != WAIT_TIMEOUT)
		{
			result = dy_win32_last_error();
		}
	}
	return result;
}

dy_Result dy_process_kill(dy_Process process, dy_u32 exit_code)
{
	dy_Result result = {0};
	if (!dy_process_is_valid(process))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
	}
	else if (!TerminateProcess((HANDLE)process.handle, exit_code))
	{
		result = dy_win32_last_error();
	}
	return result;
}

dy_Result dy_process_close(dy_Process *process)
{
	dy_Result result = {0};
	if (!process)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		if (process->handle && !CloseHandle((HANDLE)process->handle))
		{
			result = dy_win32_last_error();
		}
		*process = (dy_Process){0};
	}
	return result;
}

dy_u64 dy_current_process_id(void)
{
	return GetCurrentProcessId();
}

void dy_exit_process(dy_i32 exit_code)
{
	ExitProcess((UINT)exit_code);
}

void dy_debug_break(void)
{
	DebugBreak();
}
