#ifndef DY_H
#define DY_H

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t   dy_u8;
typedef uint16_t  dy_u16;
typedef uint32_t  dy_u32;
typedef uint64_t  dy_u64;
typedef int8_t    dy_i8;
typedef int16_t   dy_i16;
typedef int32_t   dy_i32;
typedef int64_t   dy_i64;
typedef float     dy_f32;
typedef double    dy_f64;
typedef dy_i32    dy_b32;
typedef uintptr_t dy_uptr;

#define DY_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define DY_KILOBYTES(value) ((dy_u64)(value) << 10)
#define DY_MEGABYTES(value) ((dy_u64)(value) << 20)
#define DY_GIGABYTES(value) ((dy_u64)(value) << 30)

// Source and destination must not overlap.
static inline void dy_memory_copy(void *destination, const void *source, dy_u64 size)
{
	if (size) memcpy(destination, source, (size_t)size);
}

static inline void dy_memory_zero(void *destination, dy_u64 size)
{
	if (size) memset(destination, 0, (size_t)size);
}

static inline void dy_memory_fill(void *destination, dy_u8 value, dy_u64 size)
{
	if (size) memset(destination, value, (size_t)size);
}

static inline dy_b32 dy_memory_equal(const void *left, const void *right, dy_u64 size)
{
	return size == 0 || memcmp(left, right, (size_t)size) == 0;
}

typedef struct dy_Arena
{
	dy_u64 capacity;
	dy_u64 committed;
	dy_u64 used;
	dy_u8 *data;
	const char *name;
}
dy_Arena;

typedef struct dy_Scratch
{
	dy_Arena *arena;
	dy_u64 restore_used;
}
dy_Scratch;

// A counted byte string. Data may contain zero bytes and is not necessarily
// null terminated.
typedef struct dy_String
{
	union
	{
		char *data;
		char *text;
	};
	dy_u64 size;
}
dy_String;

typedef struct dy_String_Array
{
	dy_String *items;
	dy_u32 count;
}
dy_String_Array;

typedef enum dy_Error
{
	DY_ERROR_NONE,
	DY_ERROR_UNKNOWN,
	DY_ERROR_INVALID_ARGUMENT,
	DY_ERROR_NOT_FOUND,
	DY_ERROR_ACCESS_DENIED,
	DY_ERROR_ALREADY_EXISTS,
	DY_ERROR_OUT_OF_MEMORY,
	DY_ERROR_NOT_SUPPORTED,
	DY_ERROR_BUFFER_TOO_SMALL,
	DY_ERROR_WOULD_BLOCK,
	DY_ERROR_END_OF_STREAM,
}
dy_Error;

typedef struct dy_Result
{
	dy_Error error;
	dy_u32 os_error;
}
dy_Result;

#define DY_WAIT_INFINITE ((dy_u32)-1)

#define DY_LIT(text) ((dy_String){ .data = (char *)(text), .size = sizeof(text) - 1 })

void *dy_virtual_reserve(dy_u64 size);
dy_b32 dy_virtual_commit(void *memory, dy_u64 size);
void dy_virtual_release(void *memory, dy_u64 size);

dy_Arena dy_arena_create(dy_u64 initial_reserve);
void dy_arena_set_name(dy_Arena *arena, const char *name);
void dy_arena_destroy(dy_Arena *arena);
void dy_arena_reset(dy_Arena *arena);
dy_u64 dy_arena_mark(dy_Arena *arena);
void dy_arena_restore(dy_Arena *arena, dy_u64 mark);
void *dy_arena_top(dy_Arena *arena);
void *dy_arena_reserve(dy_Arena *arena, dy_u64 size);
void *dy_arena_push(dy_Arena *arena, dy_u64 size, dy_u64 alignment);
void *dy_arena_push_zero(dy_Arena *arena, dy_u64 size, dy_u64 alignment);
void *dy_arena_push_copy(dy_Arena *arena, dy_u64 size, dy_u64 alignment, const void *data);
dy_String dy_arena_push_data(dy_Arena *arena, const void *data, dy_u64 size);
dy_String dy_arena_push_text(dy_Arena *arena, const char *text);
dy_String dy_arena_pushfv(dy_Arena *arena, const char *format, va_list arguments);
dy_String dy_arena_pushf(dy_Arena *arena, const char *format, ...);

dy_Scratch dy_begin_scratch(void);
dy_Scratch dy_begin_different_scratch(dy_Arena *conflict);
void dy_end_scratch(dy_Scratch scratch);
void dy_destroy_thread_scratch(void);

dy_String dy_string_from_data(const void *data, dy_u64 size);
dy_String dy_string_from_range(void *start, void *end);
dy_String dy_string_from_cstring(const char *text);
dy_String dy_string_slice(dy_String string, dy_u64 offset, dy_u64 size);
dy_b32 dy_string_equal(dy_String left, dy_String right);
dy_b32 dy_string_equal_insensitive(dy_String left, dy_String right);
dy_b32 dy_string_starts_with(dy_String string, dy_String prefix);
dy_b32 dy_string_ends_with(dy_String string, dy_String suffix);
dy_b32 dy_string_ends_with_insensitive(dy_String string, dy_String suffix);
dy_b32 dy_string_is(dy_String string, const char *text);
dy_b32 dy_string_contains(dy_String string, char character);
dy_String dy_string_trim_whitespace(dy_String string);
dy_b32 dy_string_split_first(dy_String string, char separator, dy_String *left, dy_String *right);
dy_String_Array dy_string_split(dy_Arena *arena, dy_String string, char separator);
dy_String_Array dy_string_split_lines(dy_Arena *arena, dy_String string);
dy_String_Array dy_string_split_block(dy_Arena *arena, dy_String string);
dy_u32 dy_string_count_lines(dy_String string);

dy_Result dy_error_message(dy_Arena *arena, dy_u32 os_error, dy_String *message);

#endif
