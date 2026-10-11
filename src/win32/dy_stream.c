#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdlib.h>

#include "../dy_stream_internal.h"
#include "dy_win32.h"

typedef enum dy_Win32_Stream_Kind
{
	DY_WIN32_STREAM_FILE,
	DY_WIN32_STREAM_PIPE,
}
dy_Win32_Stream_Kind;

typedef struct dy_Win32_Stream
{
	dy_Stream base;
	HANDLE handle;
	dy_Win32_Stream_Kind kind;
}
dy_Win32_Stream;

static dy_Result dy_win32_stream_read(dy_Stream *base, void *data, dy_u64 size,
	dy_u32 flags, dy_u64 *bytes_read);
static dy_Result dy_win32_stream_write(dy_Stream *base, const void *data, dy_u64 size,
	dy_u64 *bytes_written);
static void dy_win32_stream_destroy(dy_Stream *base);

static const dy_Stream_Ops dy_win32_stream_ops = {
	.read = dy_win32_stream_read,
	.write = dy_win32_stream_write,
	.destroy = dy_win32_stream_destroy,
};

static dy_Result dy_win32_io_error(DWORD error)
{
	return (dy_Result){ .error = dy_win32_error(error), .os_error = error };
}

static dy_Result dy_win32_stream_read(dy_Stream *base, void *data, dy_u64 size,
	dy_u32 flags, dy_u64 *bytes_read)
{
	dy_Win32_Stream *stream = (dy_Win32_Stream *)base;
	DWORD received = 0;
	DWORD request;
	if (size == 0) return (dy_Result){0};
	if ((flags & DY_STREAM_READ_AVAILABLE) && stream->kind == DY_WIN32_STREAM_PIPE)
	{
		DWORD available = 0;
		if (!PeekNamedPipe(stream->handle, NULL, 0, NULL, &available, NULL))
		{
			DWORD error = GetLastError();
			if (error == ERROR_BROKEN_PIPE)
				return (dy_Result){ .error = DY_ERROR_END_OF_STREAM };
			return dy_win32_io_error(error);
		}
		if (available == 0) return (dy_Result){ .error = DY_ERROR_WOULD_BLOCK };
		if ((dy_u64)available < size) size = available;
	}
	request = size > MAXDWORD ? MAXDWORD : (DWORD)size;
	if (!ReadFile(stream->handle, data, request, &received, NULL))
	{
		DWORD error = GetLastError();
		if (error == ERROR_BROKEN_PIPE)
			return (dy_Result){ .error = DY_ERROR_END_OF_STREAM };
		return dy_win32_io_error(error);
	}
	if (received == 0) return (dy_Result){ .error = DY_ERROR_END_OF_STREAM };
	*bytes_read = received;
	return (dy_Result){0};
}

static dy_Result dy_win32_stream_write(dy_Stream *base, const void *data, dy_u64 size,
	dy_u64 *bytes_written)
{
	dy_Win32_Stream *stream = (dy_Win32_Stream *)base;
	DWORD written = 0;
	DWORD request;
	if (size == 0) return (dy_Result){0};
	request = size > MAXDWORD ? MAXDWORD : (DWORD)size;
	if (!WriteFile(stream->handle, data, request, &written, NULL))
		return dy_win32_io_error(GetLastError());
	if (written == 0) return dy_win32_io_error(ERROR_WRITE_FAULT);
	*bytes_written = written;
	return (dy_Result){0};
}

static void dy_win32_stream_destroy(dy_Stream *base)
{
	dy_Win32_Stream *stream = (dy_Win32_Stream *)base;
	if (stream->handle && stream->handle != INVALID_HANDLE_VALUE) CloseHandle(stream->handle);
	free(stream);
}

static dy_Result dy_win32_stream_create(HANDLE handle, dy_u32 capabilities,
	dy_Win32_Stream_Kind kind, dy_Stream **stream_result)
{
	dy_Result result = {0};
	dy_Win32_Stream *stream;
	if (stream_result) *stream_result = NULL;
	if (!stream_result || !handle || handle == INVALID_HANDLE_VALUE)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	stream = calloc(1, sizeof(*stream));
	if (!stream)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	stream->base.ops = &dy_win32_stream_ops;
	stream->base.capabilities = capabilities;
	stream->handle = handle;
	stream->kind = kind;
	*stream_result = &stream->base;
	return result;
}

dy_b32 dy_stream_get_native_handle(dy_Stream *stream, dy_u32 required_capability,
	dy_uptr *handle)
{
	if (handle) *handle = 0;
	if (!stream || !handle || stream->ops != &dy_win32_stream_ops ||
		(stream->capabilities & required_capability) != required_capability) return 0;
	*handle = (dy_uptr)((dy_Win32_Stream *)stream)->handle;
	return 1;
}

dy_Result dy_pipe_create(dy_Pipe *pipe)
{
	dy_Result result = {0};
	HANDLE reader = NULL;
	HANDLE writer = NULL;
	if (pipe) *pipe = (dy_Pipe){0};
	if (!pipe)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!CreatePipe(&reader, &writer, NULL, 0))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	result = dy_win32_stream_create(reader, DY_STREAM_CAN_READ | DY_STREAM_CAN_READ_AVAILABLE,
		DY_WIN32_STREAM_PIPE, &pipe->reader);
	if (result.error != DY_ERROR_NONE) goto failure;
	reader = NULL;
	result = dy_win32_stream_create(writer, DY_STREAM_CAN_WRITE, DY_WIN32_STREAM_PIPE,
		&pipe->writer);
	if (result.error != DY_ERROR_NONE) goto failure;
	writer = NULL;
	return result;

failure:
	if (reader) CloseHandle(reader);
	if (writer) CloseHandle(writer);
	dy_pipe_close(pipe);
	return result;
}

dy_Result dy_file_stream_open(dy_String path, dy_File_Intent intent,
	dy_File_Access access, dy_Stream **stream)
{
	dy_Result result;
	dy_File file = {0};
	dy_u32 capabilities = 0;
	if (stream) *stream = NULL;
	if (!stream)
	{
		return (dy_Result){ .error = DY_ERROR_INVALID_ARGUMENT };
	}
	result = dy_access_file(path, intent, access, &file);
	if (result.error != DY_ERROR_NONE) return result;
	if (access & DY_FILE_READ)
		capabilities |= DY_STREAM_CAN_READ | DY_STREAM_CAN_READ_AVAILABLE;
	if (access & DY_FILE_WRITE) capabilities |= DY_STREAM_CAN_WRITE;
	result = dy_win32_stream_create((HANDLE)file.value, capabilities,
		DY_WIN32_STREAM_FILE, stream);
	if (result.error != DY_ERROR_NONE) CloseHandle((HANDLE)file.value);
	return result;
}
