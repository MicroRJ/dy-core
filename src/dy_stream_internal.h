#ifndef DY_STREAM_INTERNAL_H
#define DY_STREAM_INTERNAL_H

#include "dy_io.h"

dy_b32 dy_stream_get_native_handle(dy_Stream *stream, dy_u32 required_capability,
	dy_uptr *handle);

#endif
