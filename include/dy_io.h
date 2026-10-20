#ifndef DY_IO_H
#define DY_IO_H

#include "dy.h"

#define DY_WAIT_INFINITE ((dy_u32)-1)

typedef struct dy_Stream dy_Stream;

typedef struct dy_Stream_Ops
{
	dy_Result (*read)(dy_Stream *stream, void *data, dy_u64 size, dy_u32 flags, dy_u64 *bytes_read);
	dy_Result (*write)(dy_Stream *stream, const void *data, dy_u64 size, dy_u64 *bytes_written);
	void (*destroy)(dy_Stream *stream);
}
dy_Stream_Ops;

typedef enum dy_Stream_Capability
{
	DY_STREAM_CAN_READ           = 1 << 0,
	DY_STREAM_CAN_READ_AVAILABLE = 1 << 1,
	DY_STREAM_CAN_WRITE          = 1 << 2,
}
dy_Stream_Capability;

typedef enum dy_Stream_Read_Flag
{
	// Return DY_ERROR_WOULD_BLOCK instead of deliberately waiting for new data.
	DY_STREAM_READ_AVAILABLE = 1 << 0,
}
dy_Stream_Read_Flag;

struct dy_Stream
{
	// Concrete streams embed this structure as their first member.
	const dy_Stream_Ops *ops;
	dy_u32 capabilities;
};

typedef struct dy_Pipe
{
	// Endpoints are independently owned and must each be closed.
	dy_Stream *reader;
	dy_Stream *writer;
}
dy_Pipe;

typedef enum dy_Standard_Stream
{
	DY_STANDARD_INPUT,
	DY_STANDARD_OUTPUT,
	DY_STANDARD_ERROR,
}
dy_Standard_Stream;

dy_Result dy_stream_read(dy_Stream *stream, void *data, dy_u64 size, dy_u32 flags, dy_u64 *bytes_read);
dy_Result dy_stream_write(dy_Stream *stream, const void *data, dy_u64 size, dy_u64 *bytes_written);
// Closing destroys the stream. The caller must clear every pointer to it.
void dy_stream_close(dy_Stream *stream);

dy_Result dy_pipe_create(dy_Pipe *pipe);
void dy_pipe_close(dy_Pipe *pipe);
dy_Result dy_standard_stream_open(dy_Standard_Stream standard, dy_Stream **stream);

dy_b32 dy_stream_is_console(dy_Standard_Stream stream);
dy_b32 dy_console_supports_colors(dy_Standard_Stream stream);
dy_Result dy_enable_console_colors(dy_Standard_Stream stream);
dy_Result dy_write_console(dy_Standard_Stream stream, const void *data, dy_u64 size, dy_u64 *written);

#endif
