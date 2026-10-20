#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "dy_io.h"
#include "dy_win32.h"

static HANDLE dy_win32_standard_stream(dy_Standard_Stream stream)
{
	if (stream == DY_STANDARD_OUTPUT) return GetStdHandle(STD_OUTPUT_HANDLE);
	if (stream == DY_STANDARD_ERROR) return GetStdHandle(STD_ERROR_HANDLE);
	return NULL;
}

dy_b32 dy_stream_is_console(dy_Standard_Stream stream)
{
	HANDLE handle = dy_win32_standard_stream(stream);
	DWORD mode;
	return handle && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0;
}

dy_b32 dy_console_supports_colors(dy_Standard_Stream stream)
{
	HANDLE handle = dy_win32_standard_stream(stream);
	DWORD mode;
	return handle && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) &&
		(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

dy_Result dy_enable_console_colors(dy_Standard_Stream stream)
{
	dy_Result result = {0};
	HANDLE handle = dy_win32_standard_stream(stream);
	DWORD mode;
	if (!handle || handle == INVALID_HANDLE_VALUE)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!GetConsoleMode(handle, &mode))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	if (!SetConsoleMode(handle, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	return result;
}

dy_Result dy_write_console(dy_Standard_Stream stream, const void *data, dy_u64 size, dy_u64 *written)
{
	dy_Result result = {0};
	HANDLE handle = dy_win32_standard_stream(stream);
	dy_u64 total = 0;
	if (written) *written = 0;
	if (!written || !handle || handle == INVALID_HANDLE_VALUE || (!data && size))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (total < size)
	{
		dy_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD count;
		if (!WriteFile(handle, (const char *)data + total, request, &count, NULL))
		{
			*written = total;
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
			return result;
		}
		if (count == 0) break;
		total += count;
	}
	*written = total;
	return result;
}
