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
#define DY_PATH_CAPACITY 32768

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

typedef struct dy_Env_Field
{
	dy_String name;
	dy_String value;
}
dy_Env_Field;

typedef struct dy_Env_Table
{
	dy_Env_Field *items;
	dy_u32 count;
}
dy_Env_Table;

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

typedef struct dy_Stream dy_Stream;

typedef struct dy_Stream_Ops
{
	dy_Result (*read)(dy_Stream *stream, void *data, dy_u64 size, dy_u32 flags,
		dy_u64 *bytes_read);
	dy_Result (*write)(dy_Stream *stream, const void *data, dy_u64 size,
		dy_u64 *bytes_written);
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

typedef struct dy_Process
{
	dy_uptr handle;
}
dy_Process;

typedef struct dy_Process_Start_Options
{
	// The executable is also exposed to the child as argv[0]. The arguments
	// array contains only the values that follow argv[0].
	dy_String executable;
	dy_String_Array arguments;
	dy_String working_directory;
	dy_Stream *standard_input;
	dy_Stream *standard_output;
	dy_Stream *standard_error;
	dy_b32 hide_window;
}
dy_Process_Start_Options;

typedef enum dy_Process_Status
{
	DY_PROCESS_RUNNING,
	DY_PROCESS_EXITED,
}
dy_Process_Status;

typedef enum dy_Standard_Stream
{
	DY_STANDARD_INPUT,
	DY_STANDARD_OUTPUT,
	DY_STANDARD_ERROR,
}
dy_Standard_Stream;

typedef enum dy_Log_Level
{
	DY_LOG_DEBUG,
	DY_LOG_INFO,
	DY_LOG_WARNING,
	DY_LOG_ERROR,
}
dy_Log_Level;

typedef void dy_Log_Sink(void *context, dy_Log_Level level,
	dy_String subsystem, dy_String message);

#define DY_WAIT_INFINITE ((dy_u32)-1)

typedef dy_u32 dy_Thread_Function(void *context);

typedef struct dy_Thread
{
	dy_uptr handle;
}
dy_Thread;

typedef struct dy_Mutex
{
	dy_u64 storage[8];
}
dy_Mutex;

typedef struct dy_Condition
{
	dy_u64 storage[8];
}
dy_Condition;

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
char *dy_arena_push_data(dy_Arena *arena, const void *data, dy_u64 size);
char *dy_arena_push_text(dy_Arena *arena, const char *text);
char *dy_arena_push_char(dy_Arena *arena, char character);
void dy_arena_push_nchar(dy_Arena *arena, char character, dy_u64 count);
char *dy_arena_pushfv(dy_Arena *arena, const char *format, va_list arguments);
char *dy_arena_pushf(dy_Arena *arena, const char *format, ...);

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

dy_Result dy_access_file(dy_String path, dy_File_Intent intent, dy_File_Access access, dy_File *file);
dy_b32 dy_file_is_valid(dy_File file);
dy_Result dy_close_file(dy_File file);
dy_Result dy_get_file_info(dy_String path, dy_File_Info *info);
dy_Result dy_get_file_size(dy_File file, dy_u64 *size);
dy_Result dy_set_file_cursor(dy_File file, dy_Seek_Origin origin, dy_i64 distance, dy_u64 *position);
dy_Result dy_read_file(dy_File file, void *data, dy_u64 size, dy_u64 *bytes_read);
dy_Result dy_write_file(dy_File file, const void *data, dy_u64 size, dy_u64 *bytes_written);

dy_Result dy_stream_read(dy_Stream *stream, void *data, dy_u64 size, dy_u32 flags,
	dy_u64 *bytes_read);
dy_Result dy_stream_write(dy_Stream *stream, const void *data, dy_u64 size,
	dy_u64 *bytes_written);
// Closing destroys the stream. The caller must clear every pointer to it.
void dy_stream_close(dy_Stream *stream);

dy_Result dy_pipe_create(dy_Pipe *pipe);
void dy_pipe_close(dy_Pipe *pipe);
dy_Result dy_standard_stream_open(dy_Standard_Stream standard, dy_Stream **stream);
dy_Result dy_file_stream_open(dy_String path, dy_File_Intent intent,
	dy_File_Access access, dy_Stream **stream);

dy_b32 dy_path_builder_init(dy_Path_Builder *path, char *storage, dy_u64 capacity, dy_String root);
dy_Path_Mark dy_path_mark(const dy_Path_Builder *path);
dy_b32 dy_path_push(dy_Path_Builder *path, dy_String component);
void dy_path_pop(dy_Path_Builder *path, dy_Path_Mark mark);

dy_Result dy_get_executable_path(dy_Arena *arena, dy_String *path);
dy_Result dy_get_current_directory(dy_Arena *arena, dy_String *path);
dy_Result dy_set_current_directory(dy_String path);
dy_Result dy_get_absolute_path(dy_Arena *arena, dy_String path, dy_String *absolute);
dy_b32 dy_executable_resolves(dy_String name);
dy_Result dy_copy_file(dy_String source, dy_String destination, dy_b32 overwrite);
dy_Result dy_remove_file(dy_String path);
dy_Result dy_move_file(dy_String source, dy_String destination, dy_b32 overwrite);
dy_Result dy_create_directory(dy_String path);
dy_Result dy_create_directories(dy_String path);
dy_Result dy_remove_directory(dy_String path);
dy_Result dy_remove_tree(dy_String path);
dy_Result dy_find_first_file(dy_Path_Builder *path, dy_Directory *directory, dy_Directory_Entry *entry, dy_Directory_Status *status);
dy_Result dy_find_next_file(dy_Directory *directory, dy_Directory_Entry *entry, dy_Directory_Status *status);
dy_Result dy_close_directory(dy_Directory *directory);

dy_Result dy_get_env_field(dy_Arena *arena, dy_String name, dy_String *value);
dy_Result dy_get_env_table(dy_Arena *arena, dy_Env_Table *table);
dy_Result dy_set_env_field(dy_String name, dy_String value);
dy_Result dy_remove_env_field(dy_String name);

dy_b32 dy_process_is_valid(dy_Process process);
// Process start borrows all streams and duplicates their native handles for
// the child. The caller retains ownership and should close unused pipe ends.
dy_Result dy_process_start(dy_Process_Start_Options options, dy_Process *process);
dy_Result dy_process_poll(dy_Process process, dy_Process_Status *status, dy_u32 *exit_code);
dy_Result dy_process_wait(dy_Process process, dy_u32 milliseconds,
	dy_Process_Status *status, dy_u32 *exit_code);
dy_Result dy_process_kill(dy_Process process, dy_u32 exit_code);
// Closing releases bookkeeping resources and does not terminate the child.
dy_Result dy_process_close(dy_Process *process);
dy_u64 dy_current_process_id(void);
void dy_exit_process(dy_i32 exit_code);
void dy_debug_break(void);

dy_Result dy_start_thread(dy_Thread_Function *function, void *context, dy_Thread *thread);
dy_b32 dy_thread_is_valid(dy_Thread thread);
dy_Result dy_join_thread(dy_Thread thread, dy_u32 *return_code);
dy_Result dy_close_thread(dy_Thread *thread);
dy_u64 dy_current_thread_id(void);

dy_Result dy_init_mutex(dy_Mutex *mutex);
void dy_lock_mutex(dy_Mutex *mutex);
void dy_unlock_mutex(dy_Mutex *mutex);
void dy_destroy_mutex(dy_Mutex *mutex);

dy_Result dy_init_condition(dy_Condition *condition);
dy_Result dy_wait_condition(dy_Condition *condition, dy_Mutex *mutex);
void dy_signal_condition(dy_Condition *condition);
void dy_broadcast_condition(dy_Condition *condition);
void dy_destroy_condition(dy_Condition *condition);

dy_u64 dy_counter(void);
dy_u64 dy_counter_frequency(void);
dy_i64 dy_unix_time_ms(void);
void dy_sleep(dy_u64 milliseconds);

dy_b32 dy_stream_is_console(dy_Standard_Stream stream);
dy_b32 dy_console_supports_colors(dy_Standard_Stream stream);
dy_Result dy_enable_console_colors(dy_Standard_Stream stream);
dy_Result dy_write_console(dy_Standard_Stream stream, const void *data, dy_u64 size, dy_u64 *written);

// Setting a null sink restores the default standard-error sink. Configure the
// sink during startup; changing it concurrently with logging is unsupported.
void dy_log_set_sink(dy_Log_Sink *sink, void *context);
void dy_log(dy_Log_Level level, dy_String subsystem, dy_String message);
void dy_logf(dy_Log_Level level, dy_String subsystem, const char *format, ...);

dy_Result dy_error_message(dy_Arena *arena, dy_u32 os_error, dy_String *message);

#endif
