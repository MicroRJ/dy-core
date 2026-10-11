#ifndef DY_WIN32_H
#define DY_WIN32_H

#include "dy.h"

static inline dy_Error dy_win32_error(DWORD error)
{
	switch (error)
	{
		case ERROR_SUCCESS: return DY_ERROR_NONE;
		case ERROR_INVALID_PARAMETER: return DY_ERROR_INVALID_ARGUMENT;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_ENVVAR_NOT_FOUND: return DY_ERROR_NOT_FOUND;
		case ERROR_ACCESS_DENIED: return DY_ERROR_ACCESS_DENIED;
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS: return DY_ERROR_ALREADY_EXISTS;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY: return DY_ERROR_OUT_OF_MEMORY;
		case ERROR_NOT_SUPPORTED:
		case ERROR_CALL_NOT_IMPLEMENTED: return DY_ERROR_NOT_SUPPORTED;
		case ERROR_INSUFFICIENT_BUFFER:
		case ERROR_MORE_DATA: return DY_ERROR_BUFFER_TOO_SMALL;
		default: return DY_ERROR_UNKNOWN;
	}
}

static inline char *dy_win32_text(dy_Arena *arena, dy_String string)
{
	char *result;
	if (!arena || (!string.data && string.size) || string.size == UINT64_MAX) return NULL;
	result = dy_arena_reserve(arena, string.size + 1);
	if (!result) return NULL;
	if (string.size) memcpy(result, string.data, (size_t)string.size);
	result[string.size] = 0;
	arena->used += string.size + 1;
	return result;
}

static inline char *dy_win32_path_text(dy_Arena *arena, dy_String path)
{
	char *result = dy_win32_text(arena, path);
	if (!result) return NULL;
	for (dy_u64 index = 0; index < path.size; ++index)
	{
		if (result[index] == '/') result[index] = '\\';
	}
	return result;
}

static inline dy_i64 dy_win32_file_time_to_unix_ms(FILETIME time)
{
	ULARGE_INTEGER value;
	const dy_u64 unix_epoch = 116444736000000000ull;
	value.LowPart = time.dwLowDateTime;
	value.HighPart = time.dwHighDateTime;
	if (value.QuadPart < unix_epoch) return 0;
	return (dy_i64)((value.QuadPart - unix_epoch) / 10000ull);
}

#endif
