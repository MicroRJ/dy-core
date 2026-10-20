#include "dy_io.h"

static dy_Result dy_stream_unsupported(void)
{
	return (dy_Result){ .error = DY_ERROR_NOT_SUPPORTED };
}

static dy_Result dy_stream_invalid(void)
{
	return (dy_Result){ .error = DY_ERROR_INVALID_ARGUMENT };
}

dy_Result dy_stream_read(dy_Stream *stream, void *data, dy_u64 size, dy_u32 flags,
	dy_u64 *bytes_read)
{
	if (bytes_read) *bytes_read = 0;
	if (!bytes_read || !stream || (!data && size) || (flags & ~DY_STREAM_READ_AVAILABLE))
		return dy_stream_invalid();
	if (!(stream->capabilities & DY_STREAM_CAN_READ) || !stream->ops || !stream->ops->read)
		return dy_stream_unsupported();
	if ((flags & DY_STREAM_READ_AVAILABLE) &&
		!(stream->capabilities & DY_STREAM_CAN_READ_AVAILABLE)) return dy_stream_unsupported();
	return stream->ops->read(stream, data, size, flags, bytes_read);
}

dy_Result dy_stream_write(dy_Stream *stream, const void *data, dy_u64 size,
	dy_u64 *bytes_written)
{
	if (bytes_written) *bytes_written = 0;
	if (!bytes_written || !stream || (!data && size)) return dy_stream_invalid();
	if (!(stream->capabilities & DY_STREAM_CAN_WRITE) || !stream->ops || !stream->ops->write)
		return dy_stream_unsupported();
	return stream->ops->write(stream, data, size, bytes_written);
}

void dy_stream_close(dy_Stream *stream)
{
	if (stream && stream->ops && stream->ops->destroy) stream->ops->destroy(stream);
}

void dy_pipe_close(dy_Pipe *pipe)
{
	if (!pipe) return;
	dy_stream_close(pipe->reader);
	dy_stream_close(pipe->writer);
	*pipe = (dy_Pipe){0};
}
