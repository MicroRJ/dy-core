#include "dy.h"
#include "dy_memory.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#define DY_THREAD_LOCAL __declspec(thread)
#else
#define DY_THREAD_LOCAL _Thread_local
#endif

#define DY_SCRATCH_ARENA_COUNT 4
#define DY_COMMIT_GRANULARITY DY_KILOBYTES(64)

static DY_THREAD_LOCAL dy_Arena dy_scratch_arenas[DY_SCRATCH_ARENA_COUNT];

dy_Arena dy_arena_create(dy_u64 initial_reserve)
{
	dy_Arena arena = {0};
	if (initial_reserve == 0) initial_reserve = DY_MEGABYTES(64);
	arena.data = dy_virtual_reserve(initial_reserve);
	if (arena.data) arena.capacity = initial_reserve;
	return arena;
}

void dy_arena_set_name(dy_Arena *arena, const char *name)
{
	if (arena) arena->name = name;
}

void dy_arena_destroy(dy_Arena *arena)
{
	if (!arena) return;
	dy_virtual_release(arena->data, arena->capacity);
	memset(arena, 0, sizeof(*arena));
}

void dy_arena_reset(dy_Arena *arena)
{
	if (arena) arena->used = 0;
}

dy_u64 dy_arena_mark(dy_Arena *arena)
{
	return arena ? arena->used : 0;
}

void dy_arena_restore(dy_Arena *arena, dy_u64 mark)
{
	assert(arena && mark <= arena->used);
	if (arena && mark <= arena->used) arena->used = mark;
}

void *dy_arena_top(dy_Arena *arena)
{
	return arena && arena->data ? arena->data + arena->used : NULL;
}

void *dy_arena_reserve(dy_Arena *arena, dy_u64 size)
{
	dy_u64 needed;
	dy_u64 committed;

	if (!arena || !arena->data || size > arena->capacity - arena->used)
	{
		assert(!"arena capacity exceeded");
		return NULL;
	}

	needed = arena->used + size;
	if (needed > arena->committed)
	{
		committed = (needed + DY_COMMIT_GRANULARITY - 1) & ~(DY_COMMIT_GRANULARITY - 1);
		if (committed > arena->capacity) committed = arena->capacity;
		if (!dy_virtual_commit(arena->data + arena->committed, committed - arena->committed))
		{
			assert(!"unable to commit arena memory");
			return NULL;
		}
		arena->committed = committed;
	}

	return arena->data + arena->used;
}

void dy_arena_align(dy_Arena *arena, dy_u64 alignment)
{
	uintptr_t top;
	dy_u64 padding;
	assert(arena && alignment && (alignment & (alignment - 1)) == 0);
	if (!arena || !alignment || (alignment & (alignment - 1)) != 0) return;
	top = (uintptr_t)dy_arena_top(arena);
	padding = (alignment - top % alignment) % alignment;
	dy_arena_push(arena, padding);
}

void *dy_arena_push(dy_Arena *arena, dy_u64 size)
{
	void *result = dy_arena_reserve(arena, size);
	if (result) arena->used += size;
	return result;
}

void *dy_arena_push_zero(dy_Arena *arena, dy_u64 size)
{
	void *result = dy_arena_push(arena, size);
	if (result && size) memset(result, 0, (size_t)size);
	return result;
}

void *dy_arena_push_copy(dy_Arena *arena, dy_u64 size, const void *data)
{
	void *result = dy_arena_push(arena, size);
	if (result && size) memcpy(result, data, (size_t)size);
	return result;
}

char *dy_arena_push_data(dy_Arena *arena, const void *data, dy_u64 size)
{
	return dy_arena_push_copy(arena, size, data);
}

char *dy_arena_push_text(dy_Arena *arena, const char *text)
{
	return dy_arena_push_data(arena, text, text ? (dy_u64)strlen(text) : 0);
}

char *dy_arena_push_char(dy_Arena *arena, char character)
{
	char *result = dy_arena_push(arena, 1);
	if (result) *result = character;
	return result;
}

void dy_arena_push_nchar(dy_Arena *arena, char character, dy_u64 count)
{
	void *result = dy_arena_push(arena, count);
	if (result && count) memset(result, character, (size_t)count);
}

char *dy_arena_pushfv(dy_Arena *arena, const char *format, va_list arguments)
{
	va_list copy;
	int length;
	char *result;

	va_copy(copy, arguments);
	length = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	assert(length >= 0);
	if (length < 0) return NULL;

	result = dy_arena_reserve(arena, (dy_u64)length + 1);
	if (!result) return NULL;
	if (vsnprintf(result, (size_t)length + 1, format, arguments) != length) return NULL;
	arena->used += (dy_u64)length;
	return result;
}

char *dy_arena_pushf(dy_Arena *arena, const char *format, ...)
{
	va_list arguments;
	char *result;
	va_start(arguments, format);
	result = dy_arena_pushfv(arena, format, arguments);
	va_end(arguments);
	return result;
}

dy_Scratch dy_begin_scratch(void)
{
	return dy_begin_different_scratch(NULL);
}

dy_Scratch dy_begin_different_scratch(dy_Arena *conflict)
{
	for (dy_u32 index = 0; index < DY_SCRATCH_ARENA_COUNT; ++index)
	{
		dy_Arena *arena = dy_scratch_arenas + index;
		if (arena == conflict) continue;
		if (!arena->data)
		{
			*arena = dy_arena_create(0);
			dy_arena_set_name(arena, "thread scratch");
		}
		assert(arena->data);
		return (dy_Scratch){ arena, arena->used };
	}
	assert(!"no non-conflicting scratch arena available");
	return (dy_Scratch){0};
}

void dy_end_scratch(dy_Scratch scratch)
{
	dy_arena_restore(scratch.arena, scratch.restore_used);
}

void dy_destroy_thread_scratch(void)
{
	for (dy_u32 index = 0; index < DY_SCRATCH_ARENA_COUNT; ++index)
	{
		dy_arena_destroy(dy_scratch_arenas + index);
	}
}

dy_String dy_string_from_data(void *data, dy_u64 size)
{
	return (dy_String){ .data = data, .size = size };
}

dy_String dy_string_from_range(void *start, void *end)
{
	assert((dy_u8 *)end >= (dy_u8 *)start);
	if ((dy_u8 *)end < (dy_u8 *)start) return (dy_String){0};
	return dy_string_from_data(start, (dy_u64)((dy_u8 *)end - (dy_u8 *)start));
}

dy_String dy_string_from_cstring(const char *text)
{
	return dy_string_from_data((char *)text, text ? (dy_u64)strlen(text) : 0);
}

dy_String dy_string_slice(dy_String string, dy_u64 offset, dy_u64 size)
{
	assert(offset <= string.size && size <= string.size - offset);
	if (offset > string.size || size > string.size - offset) return (dy_String){0};
	return dy_string_from_data(string.data + offset, size);
}

dy_b32 dy_string_equal(dy_String left, dy_String right)
{
	return left.size == right.size && (left.size == 0 || memcmp(left.data, right.data, (size_t)left.size) == 0);
}

static dy_u32 dy_ascii_lower(dy_u32 character)
{
	return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

dy_b32 dy_string_equal_insensitive(dy_String left, dy_String right)
{
	if (left.size != right.size) return 0;
	for (dy_u64 index = 0; index < left.size; ++index)
	{
		if (dy_ascii_lower((dy_u8)left.data[index]) != dy_ascii_lower((dy_u8)right.data[index])) return 0;
	}
	return 1;
}

dy_b32 dy_string_starts_with(dy_String string, dy_String prefix)
{
	return prefix.size <= string.size && memcmp(string.data, prefix.data, (size_t)prefix.size) == 0;
}

dy_b32 dy_string_ends_with(dy_String string, dy_String suffix)
{
	return suffix.size <= string.size && memcmp(string.data + string.size - suffix.size, suffix.data, (size_t)suffix.size) == 0;
}

dy_b32 dy_string_ends_with_insensitive(dy_String string, dy_String suffix)
{
	return suffix.size <= string.size && dy_string_equal_insensitive(dy_string_slice(string, string.size - suffix.size, suffix.size), suffix);
}

dy_b32 dy_string_is(dy_String string, const char *text)
{
	return dy_string_equal(string, dy_string_from_cstring(text));
}

static dy_b32 dy_ascii_is_whitespace(char character)
{
	return character == ' ' || character == '\t' || character == '\r' || character == '\n' || character == '\v' || character == '\f';
}

dy_String dy_string_trim_whitespace(dy_String string)
{
	dy_u64 start = 0;
	dy_u64 end = string.size;
	while (start < end && dy_ascii_is_whitespace(string.data[start])) ++start;
	while (end > start && dy_ascii_is_whitespace(string.data[end - 1])) --end;
	return dy_string_slice(string, start, end - start);
}

dy_b32 dy_string_split_first(dy_String string, char separator, dy_String *left, dy_String *right)
{
	if (!left || !right) return 0;
	for (dy_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == separator)
		{
			*left = dy_string_slice(string, 0, index);
			*right = dy_string_slice(string, index + 1, string.size - index - 1);
			return 1;
		}
	}
	*left = (dy_String){0};
	*right = (dy_String){0};
	return 0;
}

dy_String_Array dy_string_split(dy_Arena *arena, dy_String string, char separator)
{
	dy_String_Array result = {0};
	dy_u32 count = 1;
	dy_u64 start = 0;

	if (!arena) return result;
	for (dy_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == separator)
		{
			if (count == UINT32_MAX) return result;
			++count;
		}
	}

	dy_arena_align(arena, _Alignof(dy_String));
	result.items = dy_arena_push_zero(arena, sizeof(*result.items) * count);
	if (!result.items) return (dy_String_Array){0};

	for (dy_u64 index = 0; index <= string.size; ++index)
	{
		if (index == string.size || string.data[index] == separator)
		{
			result.items[result.count++] = dy_string_slice(string, start, index - start);
			start = index + 1;
		}
	}
	return result;
}

dy_String_Array dy_string_split_lines(dy_Arena *arena, dy_String string)
{
	dy_String_Array result = dy_string_split(arena, string, '\n');
	for (dy_u32 index = 0; index < result.count; ++index)
	{
		dy_String *line = result.items + index;
		if (line->size && line->data[line->size - 1] == '\r') --line->size;
	}
	return result;
}

dy_String_Array dy_string_split_block(dy_Arena *arena, dy_String string)
{
	dy_String_Array result = dy_string_split(arena, string, 0);
	while (result.count && result.items[result.count - 1].size == 0) --result.count;
	return result;
}

dy_u32 dy_string_count_lines(dy_String string)
{
	dy_u32 count = string.size ? 1 : 0;
	for (dy_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == '\n') ++count;
	}
	return count;
}
