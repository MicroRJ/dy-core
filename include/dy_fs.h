#ifndef DY_FS_H
#define DY_FS_H

#include "dy_io.h"

#define DY_PATH_CAPACITY 32768

typedef struct dy_File
{
	dy_uptr value;
}
dy_File;

typedef enum dy_File_Access
{
	DY_FILE_READ         = 1 << 0,
	DY_FILE_WRITE        = 1 << 1,
	DY_FILE_EXECUTE      = 1 << 2,
	DY_FILE_SHARE_READ   = 1 << 3,
	DY_FILE_SHARE_WRITE  = 1 << 4,
	DY_FILE_SHARE_DELETE = 1 << 5,
	DY_FILE_NO_BUFFERING = 1 << 6,
}
dy_File_Access;

typedef enum dy_File_Intent
{
	DY_FILE_CREATE_ALWAYS,
	DY_FILE_CREATE_NEW,
	DY_FILE_OPEN_ALWAYS,
	DY_FILE_OPEN_EXISTING,
	DY_FILE_TRUNCATE_EXISTING,
}
dy_File_Intent;

typedef enum dy_Seek_Origin
{
	DY_SEEK_BEGIN,
	DY_SEEK_CURRENT,
	DY_SEEK_END,
}
dy_Seek_Origin;

typedef struct dy_File_Info
{
	dy_u64 size;
	dy_i64 created_unix_ms;
	dy_i64 accessed_unix_ms;
	dy_i64 modified_unix_ms;
	dy_b32 is_directory;
	dy_b32 is_symbolic_link;
}
dy_File_Info;

typedef struct dy_Path_Builder
{
	char *data;
	dy_u64 size;
	dy_u64 capacity;
}
dy_Path_Builder;

typedef dy_u64 dy_Path_Mark;

typedef struct dy_Directory
{
	dy_uptr value;
}
dy_Directory;

typedef struct dy_Directory_Entry
{
	// The name view remains valid until the next call or the directory is closed.
	dy_String name;
	dy_File_Info info;
}
dy_Directory_Entry;

typedef enum dy_Directory_Status
{
	DY_DIRECTORY_END,
	DY_DIRECTORY_ENTRY,
}
dy_Directory_Status;

dy_Result dy_file_open(dy_String path, dy_File_Intent intent, dy_File_Access access, dy_File *file);
dy_b32 dy_file_is_valid(dy_File file);
dy_Result dy_file_close(dy_File file);
dy_Result dy_file_info(dy_String path, dy_File_Info *info);
dy_Result dy_file_size(dy_File file, dy_u64 *size);
dy_Result dy_file_set_cursor(dy_File file, dy_Seek_Origin origin, dy_i64 distance, dy_u64 *position);
dy_Result dy_file_read(dy_File file, void *data, dy_u64 size, dy_u64 *bytes_read);
dy_Result dy_file_write(dy_File file, const void *data, dy_u64 size, dy_u64 *bytes_written);
dy_Result dy_file_flush(dy_File file);
dy_Result dy_file_stream_open(dy_String path, dy_File_Intent intent, dy_File_Access access, dy_Stream **stream);

dy_b32 dy_path_builder_init(dy_Path_Builder *path, char *storage, dy_u64 capacity, dy_String root);
dy_Path_Mark dy_path_mark(const dy_Path_Builder *path);
dy_b32 dy_path_push(dy_Path_Builder *path, dy_String component);
void dy_path_pop(dy_Path_Builder *path, dy_Path_Mark mark);

dy_Result dy_get_executable_path(dy_Arena *arena, dy_String *path);
dy_Result dy_get_current_directory(dy_Arena *arena, dy_String *path);
dy_Result dy_set_current_directory(dy_String path);
dy_Result dy_get_absolute_path(dy_Arena *arena, dy_String path, dy_String *absolute);
dy_b32 dy_executable_resolves(dy_String name);
dy_Result dy_file_copy(dy_String source, dy_String destination, dy_b32 overwrite);
dy_Result dy_file_remove(dy_String path);
dy_Result dy_file_move(dy_String source, dy_String destination, dy_b32 overwrite);
dy_Result dy_create_directory(dy_String path);
dy_Result dy_create_directories(dy_String path);
dy_Result dy_remove_directory(dy_String path);
dy_Result dy_remove_tree(dy_String path);
dy_Result dy_find_first_file(dy_Path_Builder *path, dy_Directory *directory, dy_Directory_Entry *entry, dy_Directory_Status *status);
dy_Result dy_find_next_file(dy_Directory *directory, dy_Directory_Entry *entry, dy_Directory_Status *status);
dy_Result dy_close_directory(dy_Directory *directory);

#endif
