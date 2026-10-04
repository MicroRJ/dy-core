#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

#include "dy_win32.h"

static void dy_win32_close_process_handle(HANDLE *handle)
{
	if (*handle) CloseHandle(*handle);
	*handle = NULL;
}

static dy_b32 dy_win32_create_process_pipe(HANDLE *read_pipe, HANDLE *write_pipe)
{
	SECURITY_ATTRIBUTES security = {0};
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;
	if (!CreatePipe(read_pipe, write_pipe, &security, 0)) return 0;
	if (SetHandleInformation(*read_pipe, HANDLE_FLAG_INHERIT, 0)) return 1;
	dy_win32_close_process_handle(read_pipe);
	dy_win32_close_process_handle(write_pipe);
	return 0;
}

dy_Result dy_start_process(dy_String command, dy_Process_Options options, dy_Process *process_result)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	STARTUPINFOA startup = {0};
	PROCESS_INFORMATION process = {0};
	HANDLE output_read = NULL;
	HANDLE output_write = NULL;
	HANDLE error_read = NULL;
	HANDLE error_write = NULL;
	char *mutable_command;
	char *working_directory = NULL;
	dy_b32 inherit_handles = options.capture_output || options.capture_error;

	if (process_result) *process_result = (dy_Process){0};
	if (!process_result || !command.data || command.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	mutable_command = dy_win32_text(scratch.arena, command);
	if (options.working_directory.size)
	{
		working_directory = dy_win32_path_text(scratch.arena, options.working_directory);
	}
	if (!mutable_command || (options.working_directory.size && !working_directory))
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (options.capture_output && !dy_win32_create_process_pipe(&output_read, &output_write)) goto failure;
	if (options.capture_error && !dy_win32_create_process_pipe(&error_read, &error_write)) goto failure;

	startup.cb = sizeof(startup);
	if (inherit_handles)
	{
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		startup.hStdOutput = output_write ? output_write : GetStdHandle(STD_OUTPUT_HANDLE);
		startup.hStdError = error_write ? error_write : GetStdHandle(STD_ERROR_HANDLE);
	}
	if (!CreateProcessA(NULL, mutable_command, NULL, NULL, inherit_handles,
		options.hide_window ? CREATE_NO_WINDOW : 0, NULL, working_directory, &startup, &process)) goto failure;

	dy_win32_close_process_handle(&process.hThread);
	dy_win32_close_process_handle(&output_write);
	dy_win32_close_process_handle(&error_write);
	process_result->handle = (dy_uptr)process.hProcess;
	process_result->standard_output = (dy_uptr)output_read;
	process_result->standard_error = (dy_uptr)error_read;
	dy_end_scratch(scratch);
	return result;

failure:
	result.os_error = GetLastError();
	result.error = dy_win32_error(result.os_error);
	dy_win32_close_process_handle(&process.hThread);
	dy_win32_close_process_handle(&process.hProcess);
	dy_win32_close_process_handle(&output_read);
	dy_win32_close_process_handle(&output_write);
	dy_win32_close_process_handle(&error_read);
	dy_win32_close_process_handle(&error_write);
	dy_end_scratch(scratch);
	return result;
}

dy_b32 dy_process_is_valid(dy_Process process)
{
	return process.handle != 0;
}

dy_Result dy_read_process(dy_Process *process, dy_Process_Stream stream, void *data,
	dy_u64 capacity, dy_u64 *size, dy_b32 *end_of_stream)
{
	dy_Result result = {0};
	dy_uptr *pipe_value;
	HANDLE pipe;
	DWORD available = 0;
	DWORD received = 0;

	if (size) *size = 0;
	if (end_of_stream) *end_of_stream = 0;
	if (!process || !size || !end_of_stream || (!data && capacity) ||
		(stream != DY_PROCESS_OUTPUT && stream != DY_PROCESS_ERROR))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	pipe_value = stream == DY_PROCESS_OUTPUT ? &process->standard_output : &process->standard_error;
	pipe = (HANDLE)*pipe_value;
	if (!pipe)
	{
		*end_of_stream = 1;
		return result;
	}
	if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_BROKEN_PIPE)
		{
			CloseHandle(pipe);
			*pipe_value = 0;
			*end_of_stream = 1;
			result.os_error = 0;
			return result;
		}
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	if (available == 0 || capacity == 0) return result;
	DWORD request = capacity < available ? (DWORD)capacity : available;
	if (capacity > MAXDWORD && available == MAXDWORD) request = MAXDWORD;
	if (!ReadFile(pipe, data, request, &received, NULL))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	*size = received;
	return result;
}

dy_Result dy_wait_process(dy_Process process, dy_u32 milliseconds, dy_b32 *completed, dy_u32 *exit_code)
{
	dy_Result result = {0};
	DWORD wait;
	DWORD code;
	if (completed) *completed = 0;
	if (exit_code) *exit_code = 0;
	if (!completed || !exit_code || !dy_process_is_valid(process))
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
	if (!GetExitCodeProcess((HANDLE)process.handle, &code))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	*completed = 1;
	*exit_code = code;
	return result;
}

dy_Result dy_close_process(dy_Process *process)
{
	dy_Result result = {0};
	HANDLE handles[3];
	if (!process)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	handles[0] = (HANDLE)process->handle;
	handles[1] = (HANDLE)process->standard_output;
	handles[2] = (HANDLE)process->standard_error;
	for (dy_u32 index = 0; index < DY_ARRAY_COUNT(handles); ++index)
	{
		if (handles[index] && !CloseHandle(handles[index]) && result.error == DY_ERROR_NONE)
		{
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
		}
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
