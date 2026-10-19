#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

#include "dy_win32.h"

static void dy_win32_canonicalize_path(char *path, dy_u64 size)
{
	for (dy_u64 index = 0; index < size; ++index)
	{
		if (path[index] == '\\') path[index] = '/';
	}
}

typedef struct dy_Win32_Directory
{
	HANDLE find;
	WIN32_FIND_DATAA data;
}
dy_Win32_Directory;

static void dy_win32_directory_entry(dy_Win32_Directory *directory, dy_Directory_Entry *entry)
{
	ULARGE_INTEGER size;
	size.LowPart = directory->data.nFileSizeLow;
	size.HighPart = directory->data.nFileSizeHigh;
	*entry = (dy_Directory_Entry){
		.name = dy_string_from_cstring(directory->data.cFileName),
		.info = {
			.size = size.QuadPart,
			.created_unix_ms = dy_win32_file_time_to_unix_ms(directory->data.ftCreationTime),
			.accessed_unix_ms = dy_win32_file_time_to_unix_ms(directory->data.ftLastAccessTime),
			.modified_unix_ms = dy_win32_file_time_to_unix_ms(directory->data.ftLastWriteTime),
			.is_directory = (directory->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
			.is_symbolic_link = (directory->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0,
		},
	};
}

static dy_Result dy_win32_find_next_file(dy_Win32_Directory *directory,
	dy_Directory_Entry *entry, dy_Directory_Status *status, dy_b32 use_current)
{
	dy_Result result = {0};
	for (;;)
	{
		if (!use_current && !FindNextFileA(directory->find, &directory->data))
		{
			result.os_error = GetLastError();
			if (result.os_error == ERROR_NO_MORE_FILES)
			{
				result.os_error = 0;
				*status = DY_DIRECTORY_END;
				return result;
			}
			result.error = dy_win32_error(result.os_error);
			return result;
		}
		use_current = 0;
		if (strcmp(directory->data.cFileName, ".") == 0 || strcmp(directory->data.cFileName, "..") == 0) continue;
		dy_win32_directory_entry(directory, entry);
		*status = DY_DIRECTORY_ENTRY;
		return result;
	}
}

dy_Result dy_get_executable_path(dy_Arena *arena, dy_String *path)
{
	dy_Result result = {0};
	dy_u64 mark;
	char *data;
	DWORD length;
	const dy_u64 capacity = 32768;

	if (path) *path = (dy_String){0};
	if (!arena || !path)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	mark = dy_arena_mark(arena);
	data = dy_arena_reserve(arena, capacity);
	if (!data)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	length = GetModuleFileNameA(NULL, data, (DWORD)capacity);
	if (!length || length >= capacity)
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? dy_win32_error(result.os_error) : DY_ERROR_BUFFER_TOO_SMALL;
		dy_arena_restore(arena, mark);
		return result;
	}
	dy_win32_canonicalize_path(data, length);
	arena->used += (dy_u64)length + 1;
	*path = dy_string_from_data(data, length);
	return result;
}

dy_Result dy_get_current_directory(dy_Arena *arena, dy_String *path)
{
	dy_Result result = {0};
	dy_u64 mark;
	DWORD required;
	DWORD length;
	char *data;

	if (path) *path = (dy_String){0};
	if (!arena || !path)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	required = GetCurrentDirectoryA(0, NULL);
	if (!required)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	mark = dy_arena_mark(arena);
	data = dy_arena_reserve(arena, required);
	if (!data)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	length = GetCurrentDirectoryA(required, data);
	if (!length || length >= required)
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? dy_win32_error(result.os_error) : DY_ERROR_BUFFER_TOO_SMALL;
		dy_arena_restore(arena, mark);
		return result;
	}
	dy_win32_canonicalize_path(data, length);
	arena->used += (dy_u64)length + 1;
	*path = dy_string_from_data(data, length);
	return result;
}

dy_Result dy_set_current_directory(dy_String path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_path;
	if (!path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_path = dy_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!SetCurrentDirectoryA(native_path))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_get_absolute_path(dy_Arena *arena, dy_String path, dy_String *absolute_path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	dy_u64 mark;
	char *native_path;
	char *absolute;
	DWORD required;
	DWORD length;

	if (absolute_path) *absolute_path = (dy_String){0};
	if (!arena || !absolute_path || !path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_different_scratch(arena);
	native_path = dy_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	required = GetFullPathNameA(native_path, 0, NULL, NULL);
	if (!required)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		dy_end_scratch(scratch);
		return result;
	}
	mark = dy_arena_mark(arena);
	absolute = dy_arena_reserve(arena, required);
	if (!absolute)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	length = GetFullPathNameA(native_path, required, absolute, NULL);
	if (!length || length >= required)
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? dy_win32_error(result.os_error) : DY_ERROR_BUFFER_TOO_SMALL;
		dy_arena_restore(arena, mark);
		dy_end_scratch(scratch);
		return result;
	}
	dy_win32_canonicalize_path(absolute, length);
	arena->used += (dy_u64)length + 1;
	*absolute_path = dy_string_from_data(absolute, length);
	dy_end_scratch(scratch);
	return result;
}

dy_b32 dy_executable_resolves(dy_String name)
{
	dy_Scratch scratch;
	char *native_name;
	char *resolved;
	DWORD length;
	const dy_u64 capacity = 32768;

	if (!name.data || name.size == 0) return 0;
	scratch = dy_begin_scratch();
	native_name = dy_win32_path_text(scratch.arena, name);
	resolved = dy_arena_push(scratch.arena, capacity, 1);
	if (!native_name || !resolved)
	{
		dy_end_scratch(scratch);
		return 0;
	}
	length = SearchPathA(NULL, native_name, ".exe", (DWORD)capacity, resolved, NULL);
	dy_end_scratch(scratch);
	return length > 0 && length < capacity;
}

dy_Result dy_file_copy(dy_String source, dy_String destination, dy_b32 overwrite)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_source;
	char *native_destination;
	if (!source.data || source.size == 0 || !destination.data || destination.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_source = dy_win32_path_text(scratch.arena, source);
	native_destination = dy_win32_path_text(scratch.arena, destination);
	if (!native_source || !native_destination)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!CopyFileA(native_source, native_destination, !overwrite))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_file_remove(dy_String path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_path;
	if (!path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_path = dy_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!DeleteFileA(native_path))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_FILE_NOT_FOUND || result.os_error == ERROR_PATH_NOT_FOUND) result.os_error = 0;
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_file_move(dy_String source, dy_String destination, dy_b32 overwrite)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_source;
	char *native_destination;
	DWORD flags = overwrite ? MOVEFILE_REPLACE_EXISTING : 0;
	if (!source.data || source.size == 0 || !destination.data || destination.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_source = dy_win32_path_text(scratch.arena, source);
	native_destination = dy_win32_path_text(scratch.arena, destination);
	if (!native_source || !native_destination)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!MoveFileExA(native_source, native_destination, flags))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_create_directory(dy_String path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_path;
	DWORD attributes;

	if (!path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_path = dy_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!CreateDirectoryA(native_path, NULL))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_ALREADY_EXISTS)
		{
			attributes = GetFileAttributesA(native_path);
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) result.os_error = 0;
		}
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_remove_directory(dy_String path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_path;

	if (!path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_path = dy_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!RemoveDirectoryA(native_path))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_FILE_NOT_FOUND || result.os_error == ERROR_PATH_NOT_FOUND) result.os_error = 0;
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_find_first_file(dy_Path_Builder *path, dy_Directory *directory,
	dy_Directory_Entry *entry, dy_Directory_Status *status)
{
	dy_Result result = {0};
	dy_Path_Mark mark;
	dy_Win32_Directory *state;

	if (directory) *directory = (dy_Directory){0};
	if (entry) *entry = (dy_Directory_Entry){0};
	if (status) *status = DY_DIRECTORY_END;
	if (!path || !path->data || !directory || !entry || !status)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	state = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
	if (!state)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	mark = dy_path_mark(path);
	if (!dy_path_push(path, DY_LIT("*")))
	{
		HeapFree(GetProcessHeap(), 0, state);
		result.error = DY_ERROR_BUFFER_TOO_SMALL;
		return result;
	}
	state->find = FindFirstFileA(path->data, &state->data);
	dy_path_pop(path, mark);
	if (state->find == INVALID_HANDLE_VALUE)
	{
		result.os_error = GetLastError();
		HeapFree(GetProcessHeap(), 0, state);
		if (result.os_error == ERROR_FILE_NOT_FOUND)
		{
			result.os_error = 0;
			return result;
		}
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	directory->value = (dy_uptr)state;
	return dy_win32_find_next_file(state, entry, status, 1);
}

dy_Result dy_find_next_file(dy_Directory *directory, dy_Directory_Entry *entry, dy_Directory_Status *status)
{
	dy_Result result = {0};
	if (entry) *entry = (dy_Directory_Entry){0};
	if (status) *status = DY_DIRECTORY_END;
	if (!directory || !directory->value || !entry || !status)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	return dy_win32_find_next_file((dy_Win32_Directory *)directory->value, entry, status, 0);
}

dy_Result dy_close_directory(dy_Directory *directory)
{
	dy_Result result = {0};
	dy_Win32_Directory *state;
	if (!directory)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!directory->value) return result;
	state = (dy_Win32_Directory *)directory->value;
	if (!FindClose(state->find))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	HeapFree(GetProcessHeap(), 0, state);
	*directory = (dy_Directory){0};
	return result;
}
