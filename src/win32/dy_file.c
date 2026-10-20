#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

#include "dy_fs.h"
#include "dy_win32.h"

static HANDLE dy_win32_handle_from_file(dy_File file)
{
	return (HANDLE)file.value;
}

dy_Result dy_file_open(dy_String path, dy_File_Intent intent, dy_File_Access access, dy_File *file)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_path;
	DWORD desired_access = 0;
	DWORD share_mode = 0;
	DWORD disposition;
	DWORD attributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE handle;

	if (file) *file = (dy_File){0};
	if (!file || !path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}

	if (access & DY_FILE_READ) desired_access |= GENERIC_READ;
	if (access & DY_FILE_WRITE) desired_access |= GENERIC_WRITE;
	if (access & DY_FILE_EXECUTE) desired_access |= GENERIC_EXECUTE;
	if (access & DY_FILE_SHARE_READ) share_mode |= FILE_SHARE_READ;
	if (access & DY_FILE_SHARE_WRITE) share_mode |= FILE_SHARE_WRITE;
	if (access & DY_FILE_SHARE_DELETE) share_mode |= FILE_SHARE_DELETE;
	if (access & DY_FILE_NO_BUFFERING) attributes |= FILE_FLAG_NO_BUFFERING;

	switch (intent)
	{
		case DY_FILE_CREATE_ALWAYS: disposition = CREATE_ALWAYS; break;
		case DY_FILE_CREATE_NEW: disposition = CREATE_NEW; break;
		case DY_FILE_OPEN_ALWAYS: disposition = OPEN_ALWAYS; break;
		case DY_FILE_OPEN_EXISTING: disposition = OPEN_EXISTING; break;
		case DY_FILE_TRUNCATE_EXISTING: disposition = TRUNCATE_EXISTING; break;
		default: result.error = DY_ERROR_INVALID_ARGUMENT; return result;
	}

	scratch = dy_begin_scratch();
	native_path = dy_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}

	handle = CreateFileA(native_path, desired_access, share_mode, NULL, disposition, attributes, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	else
	{
		file->value = (dy_uptr)handle;
	}
	dy_end_scratch(scratch);
	return result;
}

dy_b32 dy_file_is_valid(dy_File file)
{
	return file.value != 0 && dy_win32_handle_from_file(file) != INVALID_HANDLE_VALUE;
}

dy_Result dy_file_close(dy_File file)
{
	dy_Result result = {0};
	if (!dy_file_is_valid(file))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!CloseHandle(dy_win32_handle_from_file(file)))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	return result;
}

dy_Result dy_file_info(dy_String path, dy_File_Info *info)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_path;
	WIN32_FILE_ATTRIBUTE_DATA data;
	ULARGE_INTEGER size;

	if (info) *info = (dy_File_Info){0};
	if (!info || !path.data || path.size == 0)
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
	if (!GetFileAttributesExA(native_path, GetFileExInfoStandard, &data))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		dy_end_scratch(scratch);
		return result;
	}
	dy_end_scratch(scratch);

	size.LowPart = data.nFileSizeLow;
	size.HighPart = data.nFileSizeHigh;
	info->size = size.QuadPart;
	info->created_unix_ms = dy_win32_file_time_to_unix_ms(data.ftCreationTime);
	info->accessed_unix_ms = dy_win32_file_time_to_unix_ms(data.ftLastAccessTime);
	info->modified_unix_ms = dy_win32_file_time_to_unix_ms(data.ftLastWriteTime);
	info->is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	info->is_symbolic_link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	return result;
}

dy_Result dy_file_size(dy_File file, dy_u64 *size)
{
	dy_Result result = {0};
	LARGE_INTEGER value;
	if (size) *size = 0;
	if (!size || !dy_file_is_valid(file))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!GetFileSizeEx(dy_win32_handle_from_file(file), &value) || value.QuadPart < 0)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	*size = (dy_u64)value.QuadPart;
	return result;
}

dy_Result dy_file_set_cursor(dy_File file, dy_Seek_Origin origin, dy_i64 distance, dy_u64 *position)
{
	dy_Result result = {0};
	static const DWORD origins[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
	LARGE_INTEGER move;
	LARGE_INTEGER value;
	if (position) *position = 0;
	if (!position || !dy_file_is_valid(file) || origin < DY_SEEK_BEGIN || origin > DY_SEEK_END)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	move.QuadPart = distance;
	if (!SetFilePointerEx(dy_win32_handle_from_file(file), move, &value, origins[origin]) || value.QuadPart < 0)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	*position = (dy_u64)value.QuadPart;
	return result;
}

dy_Result dy_file_read(dy_File file, void *data, dy_u64 size, dy_u64 *bytes_read)
{
	dy_Result result = {0};
	dy_u64 total = 0;
	if (bytes_read) *bytes_read = 0;
	if (!bytes_read || !dy_file_is_valid(file) || (!data && size))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (total < size)
	{
		dy_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD received = 0;
		if (!ReadFile(dy_win32_handle_from_file(file), (char *)data + total, request, &received, NULL))
		{
			*bytes_read = total;
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
			return result;
		}
		total += received;
		if (received < request) break;
	}
	*bytes_read = total;
	return result;
}

dy_Result dy_file_write(dy_File file, const void *data, dy_u64 size, dy_u64 *bytes_written)
{
	dy_Result result = {0};
	dy_u64 total = 0;
	if (bytes_written) *bytes_written = 0;
	if (!bytes_written || !dy_file_is_valid(file) || (!data && size))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (total < size)
	{
		dy_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD written = 0;
		if (!WriteFile(dy_win32_handle_from_file(file), (const char *)data + total, request, &written, NULL))
		{
			*bytes_written = total;
			result.os_error = GetLastError();
			result.error = dy_win32_error(result.os_error);
			return result;
		}
		total += written;
		if (written == 0)
		{
			*bytes_written = total;
			result.error = DY_ERROR_UNKNOWN;
			return result;
		}
	}
	*bytes_written = total;
	return result;
}

dy_Result dy_file_flush(dy_File file)
{
	dy_Result result = {0};
	if (!dy_file_is_valid(file))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
	}
	else if (!FlushFileBuffers(dy_win32_handle_from_file(file)))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	return result;
}
