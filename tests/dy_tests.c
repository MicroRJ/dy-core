#include "dy.h"

#include <assert.h>
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

int main(void)
{
	dy_Arena arena = dy_arena_create(DY_KILOBYTES(64));
	assert(arena.data);
	assert(dy_arena_push(&arena, 1, 1));
	void *aligned = dy_arena_push(&arena, 1, 64);
	assert(aligned && (dy_uptr)aligned % 64 == 0);
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

	assert(dy_set_env_field(DY_LIT("DY_TEST_VALUE"), DY_LIT("platapuss")).error == DY_ERROR_NONE);
	dy_String environment_value;
	assert(dy_get_env_field(&arena, DY_LIT("DY_TEST_VALUE"), &environment_value).error == DY_ERROR_NONE);
	assert(dy_string_is(environment_value, "platapuss"));
	dy_Env_Table environment;
	assert(dy_get_env_table(&arena, &environment).error == DY_ERROR_NONE && environment.count > 0);
	assert(dy_remove_env_field(DY_LIT("DY_TEST_VALUE")).error == DY_ERROR_NONE);

	dy_Process missing_process;
	dy_Result missing = dy_start_process(DY_LIT("dy-test-command-that-does-not-exist"),
		(dy_Process_Options){0}, &missing_process);
	assert(missing.error == DY_ERROR_NOT_FOUND && missing.os_error != 0);
	dy_String error_message;
	assert(dy_error_message(&arena, missing.os_error, &error_message).error == DY_ERROR_NONE);
	assert(error_message.size > 0);

	dy_Process process;
	assert(dy_start_process(DY_LIT("cmd.exe /d /c echo platapuss"), (dy_Process_Options){
		.capture_output = 1,
		.capture_error = 1,
		.hide_window = 1,
	}, &process).error == DY_ERROR_NONE);
	dy_b32 completed;
	dy_u32 exit_code;
	assert(dy_wait_process(process, UINT32_MAX, &completed, &exit_code).error == DY_ERROR_NONE);
	assert(completed && exit_code == 0);
	char process_output[64] = {0};
	dy_b32 end_of_stream;
	assert(dy_read_process(&process, DY_PROCESS_OUTPUT, process_output, sizeof(process_output),
		&transferred, &end_of_stream).error == DY_ERROR_NONE);
	assert(transferred >= sizeof("platapuss") - 1);
	assert(memcmp(process_output, "platapuss", sizeof("platapuss") - 1) == 0);
	assert(dy_close_process(&process).error == DY_ERROR_NONE);

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
