#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

#include "dy_process.h"
#include "dy_win32.h"

static dy_b32 dy_win32_env_name_is_valid(dy_String name)
{
	if (!name.data || name.size == 0) return 0;
	for (dy_u64 index = 0; index < name.size; ++index)
	{
		if (name.data[index] == 0 || name.data[index] == '=') return 0;
	}
	return 1;
}

dy_Result dy_get_env_field(dy_Arena *arena, dy_String name, dy_String *value)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	dy_u64 mark;
	char *native_name;
	char *data;
	DWORD required;
	DWORD length;

	if (value) *value = (dy_String){0};
	if (!arena || !value || !dy_win32_env_name_is_valid(name))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_different_scratch(arena);
	native_name = dy_win32_text(scratch.arena, name);
	if (!native_name)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	SetLastError(ERROR_SUCCESS);
	required = GetEnvironmentVariableA(native_name, NULL, 0);
	if (!required)
	{
		result.os_error = GetLastError();
		if (result.os_error != ERROR_SUCCESS)
		{
			result.error = dy_win32_error(result.os_error);
			dy_end_scratch(scratch);
			return result;
		}
		required = 1;
	}
	mark = dy_arena_mark(arena);
	data = dy_arena_reserve(arena, required);
	if (!data)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	SetLastError(ERROR_SUCCESS);
	length = GetEnvironmentVariableA(native_name, data, required);
	if (length >= required || (length == 0 && GetLastError() != ERROR_SUCCESS))
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? dy_win32_error(result.os_error) : DY_ERROR_BUFFER_TOO_SMALL;
		dy_arena_restore(arena, mark);
		dy_end_scratch(scratch);
		return result;
	}
	arena->used += (dy_u64)length + 1;
	*value = dy_string_from_data(data, length);
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_get_env_table(dy_Arena *arena, dy_Env_Table *table)
{
	dy_Result result = {0};
	dy_u64 mark;
	LPCH block;
	LPCH cursor;
	dy_u64 count = 0;

	if (table) *table = (dy_Env_Table){0};
	if (!arena || !table)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	block = GetEnvironmentStringsA();
	if (!block)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	for (cursor = block; *cursor; cursor += strlen(cursor) + 1)
	{
		if (count == UINT32_MAX)
		{
			result.error = DY_ERROR_OUT_OF_MEMORY;
			FreeEnvironmentStringsA(block);
			return result;
		}
		++count;
	}

	mark = dy_arena_mark(arena);
	table->items = dy_arena_push_zero(arena, count * sizeof(*table->items), _Alignof(dy_Env_Field));
	if (!table->items && count)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		FreeEnvironmentStringsA(block);
		dy_arena_restore(arena, mark);
		return result;
	}

	for (cursor = block; *cursor; cursor += strlen(cursor) + 1)
	{
		dy_u64 size = (dy_u64)strlen(cursor);
		dy_u64 separator = cursor[0] == '=' ? 1 : 0;
		dy_Env_Field *field;
		while (separator < size && cursor[separator] != '=') ++separator;
		if (separator == size) continue;
		field = table->items + table->count;
		field->name = dy_arena_push_data(arena, cursor, separator);
		if (!field->name.data) goto out_of_memory;
		if (!dy_arena_push_zero(arena, 1, 1)) goto out_of_memory;
		field->value = dy_arena_push_data(arena, cursor + separator + 1, size - separator - 1);
		if (!field->value.data) goto out_of_memory;
		if (!dy_arena_push_zero(arena, 1, 1)) goto out_of_memory;
		++table->count;
	}
	FreeEnvironmentStringsA(block);
	return result;

out_of_memory:
	result.error = DY_ERROR_OUT_OF_MEMORY;
	FreeEnvironmentStringsA(block);
	dy_arena_restore(arena, mark);
	*table = (dy_Env_Table){0};
	return result;
}

dy_Result dy_set_env_field(dy_String name, dy_String value)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_name;
	char *native_value;
	if (!dy_win32_env_name_is_valid(name) || (!value.data && value.size))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_name = dy_win32_text(scratch.arena, name);
	native_value = dy_win32_text(scratch.arena, value);
	if (!native_name || !native_value)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!SetEnvironmentVariableA(native_name, native_value))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}

dy_Result dy_remove_env_field(dy_String name)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	char *native_name;
	if (!dy_win32_env_name_is_valid(name))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	native_name = dy_win32_text(scratch.arena, name);
	if (!native_name)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	if (!SetEnvironmentVariableA(native_name, NULL))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_ENVVAR_NOT_FOUND) result.os_error = 0;
		result.error = dy_win32_error(result.os_error);
	}
	dy_end_scratch(scratch);
	return result;
}
