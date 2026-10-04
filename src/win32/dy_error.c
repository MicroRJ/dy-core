#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string.h>

#include "dy_win32.h"

dy_Result dy_error_message(dy_Arena *arena, dy_u32 os_error, dy_String *message_result)
{
	dy_Result result = {0};
	dy_u64 mark;
	char *system_message = NULL;
	char *message;
	DWORD length;
	if (message_result) *message_result = (dy_String){0};
	if (!arena || !message_result || os_error == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS, NULL, os_error, 0, (char *)&system_message, 0, NULL);
	if (!length)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	while (length && (system_message[length - 1] == '\r' || system_message[length - 1] == '\n' ||
		system_message[length - 1] == ' ')) --length;
	mark = dy_arena_mark(arena);
	message = dy_arena_push_data(arena, system_message, length);
	if (!message || !dy_arena_push_char(arena, 0))
	{
		dy_arena_restore(arena, mark);
		result.error = DY_ERROR_OUT_OF_MEMORY;
	}
	else *message_result = dy_string_from_data(message, length);
	LocalFree(system_message);
	return result;
}
