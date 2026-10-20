#include "dy_fs.h"

static dy_b32 dy_path_is_separator(char character)
{
	return character == '/' || character == '\\';
}

dy_b32 dy_path_builder_init(dy_Path_Builder *path, char *storage, dy_u64 capacity, dy_String root)
{
	if (path) *path = (dy_Path_Builder){0};
	if (!path || !storage || capacity == 0 || (!root.data && root.size) || root.size >= capacity) return 0;
	for (dy_u64 index = 0; index < root.size; ++index)
	{
		storage[index] = root.data[index] == '\\' ? '/' : root.data[index];
	}
	storage[root.size] = 0;
	*path = (dy_Path_Builder){ .data = storage, .size = root.size, .capacity = capacity };
	return 1;
}

dy_Path_Mark dy_path_mark(const dy_Path_Builder *path)
{
	return path ? path->size : 0;
}

dy_b32 dy_path_push(dy_Path_Builder *path, dy_String component)
{
	dy_b32 separator;
	if (!path || !path->data || path->capacity == 0 || path->size >= path->capacity || (!component.data && component.size)) return 0;
	separator = path->size && component.size && !dy_path_is_separator(path->data[path->size - 1]) && !dy_path_is_separator(component.data[0]);
	if (component.size > path->capacity - path->size - 1) return 0;
	if (separator && component.size == path->capacity - path->size - 1) return 0;
	if (separator) path->data[path->size++] = '/';
	for (dy_u64 index = 0; index < component.size; ++index)
	{
		path->data[path->size++] = component.data[index] == '\\' ? '/' : component.data[index];
	}
	path->data[path->size] = 0;
	return 1;
}

void dy_path_pop(dy_Path_Builder *path, dy_Path_Mark mark)
{
	if (!path || !path->data || mark > path->size) return;
	path->size = mark;
	path->data[mark] = 0;
}

static dy_u64 dy_path_root_size(dy_String path)
{
	dy_u64 root = 0;
	if (path.size >= 2 && path.data[1] == ':') return 2;
	if (path.size && dy_path_is_separator(path.data[0])) root = 1;
	if (path.size >= 2 && dy_path_is_separator(path.data[0]) && dy_path_is_separator(path.data[1]))
	{
		root = 2;
		for (dy_u32 component = 0; component < 2 && root < path.size; ++component)
		{
			while (root < path.size && !dy_path_is_separator(path.data[root])) ++root;
			while (root < path.size && dy_path_is_separator(path.data[root])) ++root;
		}
	}
	return root;
}

dy_Result dy_create_directories(dy_String path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	dy_Path_Builder builder;
	dy_u64 root;
	char *storage;
	if (!path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = dy_begin_scratch();
	storage = dy_arena_push(scratch.arena, path.size + 1, 1);
	if (!storage || !dy_path_builder_init(&builder, storage, path.size + 1, path))
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		dy_end_scratch(scratch);
		return result;
	}
	root = dy_path_root_size(dy_string_from_data(builder.data, builder.size));
	if (root >= builder.size)
	{
		dy_File_Info info;
		result = dy_file_info(dy_string_from_data(builder.data, builder.size), &info);
		if (!result.error && !info.is_directory) result.error = DY_ERROR_ALREADY_EXISTS;
		dy_end_scratch(scratch);
		return result;
	}
	for (dy_u64 index = root; index < builder.size; ++index)
	{
		if (!dy_path_is_separator(builder.data[index]) || index == root) continue;
		builder.data[index] = 0;
		result = dy_create_directory(dy_string_from_data(builder.data, index));
		builder.data[index] = '/';
		if (result.error)
		{
			dy_end_scratch(scratch);
			return result;
		}
	}
	result = dy_create_directory(dy_string_from_data(builder.data, builder.size));
	dy_end_scratch(scratch);
	return result;
}

static dy_Result dy_remove_tree_builder(dy_Path_Builder *path)
{
	dy_Result result = {0};
	dy_File_Info root;
	dy_Directory directory = {0};
	dy_Directory_Entry entry;
	dy_Directory_Status status;
	dy_String root_path = dy_string_from_data(path->data, path->size);
	result = dy_file_info(root_path, &root);
	if (result.error == DY_ERROR_NOT_FOUND) return (dy_Result){0};
	if (result.error) return result;
	if (!root.is_directory)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (root.is_symbolic_link) return dy_remove_directory(root_path);
	result = dy_find_first_file(path, &directory, &entry, &status);
	while (!result.error && status == DY_DIRECTORY_ENTRY)
	{
		dy_Path_Mark mark = dy_path_mark(path);
		if (!dy_path_push(path, entry.name)) result.error = DY_ERROR_BUFFER_TOO_SMALL;
		else if (entry.info.is_directory)
		{
			result = entry.info.is_symbolic_link ?
				dy_remove_directory(dy_string_from_data(path->data, path->size)) : dy_remove_tree_builder(path);
		}
		else result = dy_file_remove(dy_string_from_data(path->data, path->size));
		dy_path_pop(path, mark);
		if (!result.error) result = dy_find_next_file(&directory, &entry, &status);
	}
	{
		dy_Result close = dy_close_directory(&directory);
		if (!result.error) result = close;
	}
	if (!result.error) result = dy_remove_directory(root_path);
	return result;
}

dy_Result dy_remove_tree(dy_String path)
{
	dy_Result result = {0};
	dy_Scratch scratch;
	dy_Path_Builder builder;
	char *storage;
	if (!path.data || path.size == 0)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (path.size >= DY_PATH_CAPACITY)
	{
		result.error = DY_ERROR_BUFFER_TOO_SMALL;
		return result;
	}
	scratch = dy_begin_scratch();
	storage = dy_arena_push(scratch.arena, DY_PATH_CAPACITY, 1);
	if (!storage || !dy_path_builder_init(&builder, storage, DY_PATH_CAPACITY, path)) result.error = DY_ERROR_OUT_OF_MEMORY;
	else result = dy_remove_tree_builder(&builder);
	dy_end_scratch(scratch);
	return result;
}
