#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../dy_stream_internal.h"
#include "dy_win32.h"

static void dy_win32_close_handle(HANDLE *handle)
{
	if (*handle && *handle != INVALID_HANDLE_VALUE) CloseHandle(*handle);
	*handle = NULL;
}

static dy_b32 dy_win32_string_has_zero(dy_String string)
{
	for (dy_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == 0) return 1;
	}
	return 0;
}

// Quote one argv item using the parsing rules used by CommandLineToArgvW and
// the Microsoft C runtime. Quoting every item keeps construction predictable.
static char *dy_win32_command_line(dy_Arena *arena, dy_String_Array arguments)
{
	dy_u64 capacity = 1;
	char *result;
	char *out;

	for (dy_u32 index = 0; index < arguments.count; ++index)
	{
		dy_String argument = arguments.items[index];
		if ((!argument.data && argument.size) || dy_win32_string_has_zero(argument) ||
			capacity > UINT64_MAX - 3 ||
			argument.size > (UINT64_MAX - capacity - 3) / 2) return NULL;
		capacity += argument.size * 2 + 3;
	}
	result = dy_arena_reserve(arena, capacity);
	if (!result) return NULL;
	out = result;
	for (dy_u32 index = 0; index < arguments.count; ++index)
	{
		dy_String argument = arguments.items[index];
		if (index) *out++ = ' ';
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
				for (dy_u64 slash = 0; slash < slash_count * 2; ++slash) *out++ = '\\';
				break;
			}
			if (argument.data[offset] == '"')
			{
				for (dy_u64 slash = 0; slash < slash_count * 2 + 1; ++slash) *out++ = '\\';
				*out++ = '"';
			}
			else
			{
				for (dy_u64 slash = 0; slash < slash_count; ++slash) *out++ = '\\';
				*out++ = argument.data[offset];
			}
			++offset;
		}
		*out++ = '"';
	}
	*out++ = 0;
	arena->used += (dy_u64)(out - result);
	return result;
}

static dy_Result dy_win32_duplicate_process_stream(dy_Stream *stream, DWORD standard_stream, dy_u32 required_capability, DWORD null_access, HANDLE *duplicate)
{
	dy_Result result = {0};
	dy_uptr value = 0;
	HANDLE source;
	HANDLE temporary = NULL;
	*duplicate = NULL;
	if (stream)
	{
		if (!dy_stream_get_native_handle(stream, required_capability, &value))
		{
			result.error = DY_ERROR_NOT_SUPPORTED;
			return result;
		}
		source = (HANDLE)value;
	}
	else
	{
		source = GetStdHandle(standard_stream);
		if (!source || source == INVALID_HANDLE_VALUE)
		{
			temporary = CreateFileA("NUL", null_access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			if (temporary == INVALID_HANDLE_VALUE)
			{
				result.os_error = GetLastError();
				result.error = dy_win32_error(result.os_error);
				return result;
			}
			source = temporary;
		}
	}
	if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), duplicate, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	if (temporary) CloseHandle(temporary);
	return result;
}

static dy_Result dy_win32_start_process_command(char *command, char *working_directory
,          dy_Stream  *standard_input
,          dy_Stream  *standard_output
,          dy_Stream  *standard_error
,          dy_b32      hide_window
,          dy_Process *process_result)
{
	dy_Result result = {0};
	STARTUPINFOEXA startup = {0};
	PROCESS_INFORMATION process = {0};
	HANDLE inherited[3] = {0};
	LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
	SIZE_T attribute_size = 0;
	DWORD creation_flags = hide_window ? CREATE_NO_WINDOW : 0;
	dy_b32 redirect = standard_input || standard_output || standard_error;
	dy_b32 attributes_initialized = 0;

	*process_result = (dy_Process){0};
	startup.StartupInfo.cb = sizeof(startup);
	if (redirect)
	{
		result = dy_win32_duplicate_process_stream(standard_input, STD_INPUT_HANDLE, DY_STREAM_CAN_READ, GENERIC_READ, inherited + 0);
		if (result.error != DY_ERROR_NONE) goto cleanup;
		result = dy_win32_duplicate_process_stream(standard_output, STD_OUTPUT_HANDLE, DY_STREAM_CAN_WRITE, GENERIC_WRITE, inherited + 1);
		if (result.error != DY_ERROR_NONE) goto cleanup;
		result = dy_win32_duplicate_process_stream(standard_error, STD_ERROR_HANDLE, DY_STREAM_CAN_WRITE, GENERIC_WRITE, inherited + 2);
		if (result.error != DY_ERROR_NONE) goto cleanup;

		InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
		if (attribute_size == 0)
		{
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
			goto cleanup;
		}
		attributes = malloc(attribute_size);
		if (!attributes)
		{
			result.error = DY_ERROR_OUT_OF_MEMORY;
			goto cleanup;
		}
		if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size))
		{
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
			goto cleanup;
		}
		attributes_initialized = 1;
		if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
				inherited, sizeof(inherited), NULL, NULL))
		{
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
			goto cleanup;
		}
		startup.lpAttributeList = attributes;
		startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
		startup.StartupInfo.hStdInput = inherited[0];
		startup.StartupInfo.hStdOutput = inherited[1];
		startup.StartupInfo.hStdError = inherited[2];
		creation_flags |= EXTENDED_STARTUPINFO_PRESENT;
	}

	if (!CreateProcessA(NULL, command, NULL, NULL, redirect, creation_flags,
		NULL, working_directory, &startup.StartupInfo, &process))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		goto cleanup;
	}
	process_result->handle = (dy_uptr)process.hProcess;
	process.hProcess = NULL;

cleanup:
	dy_win32_close_handle(&process.hThread);
	dy_win32_close_handle(&process.hProcess);
	for (dy_u32 index = 0; index < DY_ARRAY_COUNT(inherited); ++index)
		dy_win32_close_handle(inherited + index);
	if (attributes_initialized) DeleteProcThreadAttributeList(attributes);
	if (attributes)
	{
		free(attributes);
	}
	return result;
}

dy_Result dy_process_start(dy_Process_Start_Options options, dy_Process *process_result)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *command;
	char *working_directory = NULL;
	dy_b32 has_arguments = options.arguments.count != 0;
	dy_b32 has_command_line = options.command_line.size != 0;
	if (process_result) *process_result = (dy_Process){0};
	if (!process_result || has_arguments == has_command_line || (has_arguments && (!options.arguments.items || !options.arguments.items[0].data || options.arguments.items[0].size == 0)) || (has_command_line && (!options.command_line.data || dy_win32_string_has_zero(options.command_line))))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	for (dy_u32 index = 0; has_arguments && index < options.arguments.count; ++index)
	{
		if ((!options.arguments.items[index].data && options.arguments.items[index].size) ||
			dy_win32_string_has_zero(options.arguments.items[index]))
		{
			result.error = DY_ERROR_INVALID_ARGUMENT;
			return result;
		}
	}
	if ((options.working_directory.size && !options.working_directory.data) ||
		dy_win32_string_has_zero(options.working_directory))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}

	scratch = dy_begin_scratch();
	if (options.working_directory.size)
		working_directory = dy_win32_path_text(scratch.arena, options.working_directory);
	if (has_command_line)
	{
		command = dy_arena_push_data(scratch.arena, options.command_line.data, options.command_line.size);
		if (command && !dy_arena_push_char(scratch.arena, 0)) command = NULL;
	}
	else
	{
		command = dy_win32_command_line(scratch.arena, options.arguments);
	}
	if ((options.working_directory.size && !working_directory) || !command)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	result = dy_win32_start_process_command(command, working_directory,
		options.standard_input, options.standard_output, options.standard_error,
		options.hide_window, process_result);
	dy_end_scratch(scratch);
	return result;
}

dy_b32 dy_process_is_valid(dy_Process process)
{
	return process.handle != 0;
}

dy_Result dy_process_poll(dy_Process process, dy_Process_Status *status, dy_u32 *exit_code)
{
	dy_Result result = {0};
	DWORD code;
	if (status) *status = DY_PROCESS_RUNNING;
	if (exit_code) *exit_code = 0;
	if (!status || !exit_code || !dy_process_is_valid(process))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!GetExitCodeProcess((HANDLE)process.handle, &code))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	if (code != STILL_ACTIVE)
	{
		*status = DY_PROCESS_EXITED;
		*exit_code = code;
	}
	return result;
}

dy_Result dy_process_wait(dy_Process process, dy_u32 milliseconds,
	dy_Process_Status *status, dy_u32 *exit_code)
{
	dy_Result result = {0};
	DWORD wait;
	if (status) *status = DY_PROCESS_RUNNING;
	if (exit_code) *exit_code = 0;
	if (!status || !exit_code || !dy_process_is_valid(process))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	wait = WaitForSingleObject((HANDLE)process.handle, milliseconds);
	if (wait == WAIT_TIMEOUT) return result;
	if (wait != WAIT_OBJECT_0)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	return dy_process_poll(process, status, exit_code);
}

dy_Result dy_process_kill(dy_Process process, dy_u32 exit_code)
{
	dy_Result result = {0};
	if (!dy_process_is_valid(process))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!TerminateProcess((HANDLE)process.handle, exit_code))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	return result;
}

dy_Result dy_process_close(dy_Process *process)
{
	dy_Result result = {0};
	if (!process)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (process->handle && !CloseHandle((HANDLE)process->handle))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	*process = (dy_Process){0};
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
