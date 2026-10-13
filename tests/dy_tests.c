#include "dy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Thread_Test_Context {
	dy_Mutex mutex;
	dy_Condition condition;
	dy_u32 value;
} Thread_Test_Context;

static dy_u32 thread_test_main(void *context_pointer)
{
	Thread_Test_Context *context = context_pointer;
	dy_lock_mutex(&context->mutex);
	context->value = 42;
	dy_signal_condition(&context->condition);
	dy_unlock_mutex(&context->mutex);
	return 7;
}

int main(int argument_count, char **arguments)
{
	if (argument_count == 2 && strcmp(arguments[1], "--dy-stream-child") == 0)
	{
		char buffer[256];
		size_t size;
		while ((size = fread(buffer, 1, sizeof(buffer), stdin)) != 0)
		{
			if (fwrite(buffer, 1, size, stdout) != size) return 2;
		}
		return ferror(stdin) ? 1 : 0;
	}
	if (argument_count > 1 && strcmp(arguments[1], "--dy-argument-child") == 0)
	{
		if (argument_count != 8) return 1;
		if (strcmp(arguments[2], "plain") != 0) return 2;
		if (strcmp(arguments[3], "contains spaces") != 0) return 3;
		if (strcmp(arguments[4], "") != 0) return 4;
		if (strcmp(arguments[5], "quote\"inside") != 0) return 5;
		if (strcmp(arguments[6], "trailing\\") != 0) return 6;
		if (strcmp(arguments[7], "slashes\\\\\"quote") != 0) return 7;
		return 0;
	}

	dy_Arena arena = dy_arena_create(DY_KILOBYTES(64));
	assert(arena.data);
	assert(dy_arena_push(&arena, 1, 1));
	void *aligned = dy_arena_push(&arena, 1, 64);
	assert(aligned && (dy_uptr)aligned % 64 == 0);
	assert(!dy_arena_reserve(&arena, arena.capacity + 1));
	char embedded_zero[] = { 'a', 0, 'b' };
	assert(dy_string_contains(DY_LIT("abc"), 'b'));
	assert(!dy_string_contains(DY_LIT("abc"), 'x'));
	assert(dy_string_contains(dy_string_from_data(embedded_zero, sizeof(embedded_zero)), 0));
	assert(!dy_string_contains(DY_LIT("abc"), 0));
	void *memory = dy_virtual_reserve(DY_KILOBYTES(4));
	assert(memory && dy_virtual_commit(memory, DY_KILOBYTES(4)));
	memset(memory, 0x5a, DY_KILOBYTES(4));
	dy_virtual_release(memory, DY_KILOBYTES(4));

	assert(dy_counter_frequency() > 0);
	dy_u64 before = dy_counter();
	dy_sleep(1);
	assert(dy_counter() >= before);
	assert(dy_unix_time_ms() > 0);

	assert(dy_remove_tree(DY_LIT("build/dy_test")).error == DY_ERROR_NONE);
	assert(dy_create_directories(DY_LIT("build/dy_test/a/b")).error == DY_ERROR_NONE);
	dy_File file;
	dy_u64 transferred;
	dy_u64 size;
	const char expected[] = "dy";
	char actual[sizeof(expected)] = {0};
	assert(dy_access_file(DY_LIT("build/dy_test/a/source.tmp"), DY_FILE_CREATE_ALWAYS,
		DY_FILE_READ | DY_FILE_WRITE | DY_FILE_SHARE_READ, &file).error == DY_ERROR_NONE);
	assert(dy_file_is_valid(file));
	assert(dy_write_file(file, expected, sizeof(expected), &transferred).error == DY_ERROR_NONE);
	assert(transferred == sizeof(expected));
	assert(dy_get_file_size(file, &size).error == DY_ERROR_NONE && size == sizeof(expected));
	dy_u64 position;
	assert(dy_set_file_cursor(file, DY_SEEK_BEGIN, 0, &position).error == DY_ERROR_NONE && position == 0);
	assert(dy_read_file(file, actual, sizeof(actual), &transferred).error == DY_ERROR_NONE);
	assert(transferred == sizeof(actual) && memcmp(actual, expected, sizeof(expected)) == 0);
	assert(dy_close_file(file).error == DY_ERROR_NONE);

	dy_Stream *file_stream;
	assert(dy_file_stream_open(DY_LIT("build/dy_test/a/stream.tmp"), DY_FILE_CREATE_ALWAYS,
		DY_FILE_WRITE, &file_stream).error == DY_ERROR_NONE);
	assert(dy_stream_write(file_stream, expected, sizeof(expected),
		&transferred).error == DY_ERROR_NONE && transferred == sizeof(expected));
	assert(dy_stream_read(file_stream, actual, sizeof(actual), 0,
		&transferred).error == DY_ERROR_NOT_SUPPORTED);
	dy_stream_close(file_stream);
	assert(dy_file_stream_open(DY_LIT("build/dy_test/a/stream.tmp"), DY_FILE_OPEN_EXISTING,
		DY_FILE_READ | DY_FILE_SHARE_READ, &file_stream).error == DY_ERROR_NONE);
	memset(actual, 0, sizeof(actual));
	assert(dy_stream_read(file_stream, actual, sizeof(actual), DY_STREAM_READ_AVAILABLE,
		&transferred).error == DY_ERROR_NONE && transferred == sizeof(actual));
	assert(memcmp(actual, expected, sizeof(expected)) == 0);
	assert(dy_stream_read(file_stream, actual, sizeof(actual), 0,
		&transferred).error == DY_ERROR_END_OF_STREAM);
	dy_stream_close(file_stream);

	dy_Pipe pipe;
	assert(dy_pipe_create(&pipe).error == DY_ERROR_NONE);
	assert(dy_stream_read(pipe.reader, actual, sizeof(actual),
		DY_STREAM_READ_AVAILABLE, &transferred).error == DY_ERROR_WOULD_BLOCK);
	assert(dy_stream_read(pipe.writer, actual, sizeof(actual), 0,
		&transferred).error == DY_ERROR_NOT_SUPPORTED);
	assert(dy_stream_write(pipe.writer, expected, sizeof(expected),
		&transferred).error == DY_ERROR_NONE && transferred == sizeof(expected));
	memset(actual, 0, sizeof(actual));
	assert(dy_stream_read(pipe.reader, actual, sizeof(actual), DY_STREAM_READ_AVAILABLE,
		&transferred).error == DY_ERROR_NONE && transferred == sizeof(actual));
	assert(memcmp(actual, expected, sizeof(expected)) == 0);
	dy_stream_close(pipe.writer);
	pipe.writer = NULL;
	assert(dy_stream_read(pipe.reader, actual, sizeof(actual),
		DY_STREAM_READ_AVAILABLE, &transferred).error == DY_ERROR_END_OF_STREAM);
	dy_pipe_close(&pipe);

	dy_File_Info info;
	assert(dy_get_file_info(DY_LIT("build/dy_test/a/source.tmp"), &info).error == DY_ERROR_NONE);
	assert(info.size == sizeof(expected) && !info.is_directory && info.modified_unix_ms > 0);
	assert(dy_move_file(DY_LIT("build/dy_test/a/source.tmp"),
		DY_LIT("build/dy_test/a/moved.tmp"), 0).error == DY_ERROR_NONE);
	assert(dy_copy_file(DY_LIT("build/dy_test/a/moved.tmp"),
		DY_LIT("build/dy_test/a/copied.tmp"), 0).error == DY_ERROR_NONE);
	assert(dy_remove_file(DY_LIT("build/dy_test/a/copied.tmp")).error == DY_ERROR_NONE);

	char path_storage[1024];
	dy_Path_Builder path;
	dy_Directory directory;
	dy_Directory_Entry entry;
	dy_Directory_Status directory_status;
	dy_b32 found_moved = 0;
	assert(dy_path_builder_init(&path, path_storage, sizeof(path_storage), DY_LIT("build/dy_test/a")));
	assert(dy_find_first_file(&path, &directory, &entry, &directory_status).error == DY_ERROR_NONE);
	while (directory_status == DY_DIRECTORY_ENTRY) {
		if (dy_string_is(entry.name, "moved.tmp")) found_moved = 1;
		assert(dy_find_next_file(&directory, &entry, &directory_status).error == DY_ERROR_NONE);
	}
	assert(found_moved);
	assert(dy_close_directory(&directory).error == DY_ERROR_NONE);

	dy_String current_directory;
	dy_String absolute_path;
	dy_String executable_path;
	assert(dy_get_current_directory(&arena, &current_directory).error == DY_ERROR_NONE);
	assert(dy_set_current_directory(current_directory).error == DY_ERROR_NONE);
	assert(dy_get_absolute_path(&arena, DY_LIT("."), &absolute_path).error == DY_ERROR_NONE);
	assert(absolute_path.size > 0);
	assert(dy_get_executable_path(&arena, &executable_path).error == DY_ERROR_NONE);
	assert(executable_path.size > 0);
	assert(dy_executable_resolves(DY_LIT("cmd")));

	dy_Pipe process_input;
	dy_Pipe process_stdout;
	assert(dy_pipe_create(&process_input).error == DY_ERROR_NONE);
	assert(dy_pipe_create(&process_stdout).error == DY_ERROR_NONE);
	dy_String child_arguments[] = { DY_LIT("--dy-stream-child") };
	dy_Process stream_process;
	assert(dy_process_start((dy_Process_Start_Options){
		.executable = executable_path,
		.arguments = { child_arguments, DY_ARRAY_COUNT(child_arguments) },
		.standard_input = process_input.reader,
		.standard_output = process_stdout.writer,
		.hide_window = 1,
	}, &stream_process).error == DY_ERROR_NONE);
	dy_stream_close(process_input.reader);
	process_input.reader = NULL;
	dy_stream_close(process_stdout.writer);
	process_stdout.writer = NULL;
	const char stream_message[] = "streamed through child";
	dy_u64 sent = 0;
	while (sent < sizeof(stream_message))
	{
		assert(dy_stream_write(process_input.writer, stream_message + sent,
			sizeof(stream_message) - sent, &transferred).error == DY_ERROR_NONE);
		assert(transferred > 0);
		sent += transferred;
	}
	dy_stream_close(process_input.writer);
	process_input.writer = NULL;
	dy_Process_Status process_status;
	dy_u32 stream_exit_code;
	assert(dy_process_wait(stream_process, DY_WAIT_INFINITE, &process_status,
		&stream_exit_code).error == DY_ERROR_NONE);
	assert(process_status == DY_PROCESS_EXITED && stream_exit_code == 0);
	char stream_output[sizeof(stream_message)] = {0};
	dy_u64 received = 0;
	while (received < sizeof(stream_output))
	{
		assert(dy_stream_read(process_stdout.reader, stream_output + received,
			sizeof(stream_output) - received, 0, &transferred).error == DY_ERROR_NONE);
		assert(transferred > 0);
		received += transferred;
	}
	assert(memcmp(stream_output, stream_message, sizeof(stream_message)) == 0);
	assert(dy_stream_read(process_stdout.reader, actual, sizeof(actual),
		0, &transferred).error == DY_ERROR_END_OF_STREAM);
	dy_pipe_close(&process_input);
	dy_pipe_close(&process_stdout);
	assert(dy_process_close(&stream_process).error == DY_ERROR_NONE);

	dy_String argument_values[] = {
		DY_LIT("--dy-argument-child"),
		DY_LIT("plain"),
		DY_LIT("contains spaces"),
		DY_LIT(""),
		DY_LIT("quote\"inside"),
		DY_LIT("trailing\\"),
		DY_LIT("slashes\\\\\"quote"),
	};
	dy_Process argument_process;
	assert(dy_process_start((dy_Process_Start_Options){
		.executable = executable_path,
		.arguments = { argument_values, DY_ARRAY_COUNT(argument_values) },
		.hide_window = 1,
	}, &argument_process).error == DY_ERROR_NONE);
	dy_u32 argument_exit_code;
	assert(dy_process_wait(argument_process, DY_WAIT_INFINITE, &process_status,
		&argument_exit_code).error == DY_ERROR_NONE);
	assert(process_status == DY_PROCESS_EXITED && argument_exit_code == 0);
	assert(dy_process_close(&argument_process).error == DY_ERROR_NONE);

	assert(dy_set_env_field(DY_LIT("DY_TEST_VALUE"), DY_LIT("platapuss")).error == DY_ERROR_NONE);
	dy_String environment_value;
	assert(dy_get_env_field(&arena, DY_LIT("DY_TEST_VALUE"), &environment_value).error == DY_ERROR_NONE);
	assert(dy_string_is(environment_value, "platapuss"));
	dy_Env_Table environment;
	assert(dy_get_env_table(&arena, &environment).error == DY_ERROR_NONE && environment.count > 0);
	assert(dy_remove_env_field(DY_LIT("DY_TEST_VALUE")).error == DY_ERROR_NONE);

	dy_Process missing_process;
	dy_Result missing = dy_process_start((dy_Process_Start_Options){
		.executable = DY_LIT("dy-test-command-that-does-not-exist"),
	}, &missing_process);
	assert(missing.error == DY_ERROR_NOT_FOUND && missing.os_error != 0);
	dy_String error_message;
	assert(dy_error_message(&arena, missing.os_error, &error_message).error == DY_ERROR_NONE);
	assert(error_message.size > 0);

	Thread_Test_Context thread_context = {0};
	dy_Thread thread;
	dy_u32 thread_return_code;
	assert(dy_init_mutex(&thread_context.mutex).error == DY_ERROR_NONE);
	assert(dy_init_condition(&thread_context.condition).error == DY_ERROR_NONE);
	dy_lock_mutex(&thread_context.mutex);
	assert(dy_start_thread(thread_test_main, &thread_context, &thread).error == DY_ERROR_NONE);
	while (thread_context.value == 0) assert(dy_wait_condition(&thread_context.condition,
		&thread_context.mutex).error == DY_ERROR_NONE);
	dy_unlock_mutex(&thread_context.mutex);
	assert(dy_join_thread(thread, &thread_return_code).error == DY_ERROR_NONE);
	assert(thread_return_code == 7);
	assert(dy_close_thread(&thread).error == DY_ERROR_NONE);
	dy_destroy_condition(&thread_context.condition);
	dy_destroy_mutex(&thread_context.mutex);

	dy_u64 written;
	assert(dy_write_console(DY_STANDARD_OUTPUT, NULL, 0, &written).error == DY_ERROR_NONE && written == 0);
	(void)dy_stream_is_console(DY_STANDARD_OUTPUT);
	(void)dy_console_supports_colors(DY_STANDARD_OUTPUT);

	assert(dy_remove_tree(DY_LIT("build/dy_test")).error == DY_ERROR_NONE);
	dy_arena_destroy(&arena);
	return 0;
}
