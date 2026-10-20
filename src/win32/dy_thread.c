#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "dy_thread.h"
#include "dy_win32.h"

typedef struct dy_Win32_Thread_Start
{
	dy_Thread_Function *function;
	void *context;
}
dy_Win32_Thread_Start;

_Static_assert(sizeof(dy_Mutex) >= sizeof(SRWLOCK), "dy_Mutex storage is too small");
_Static_assert(_Alignof(dy_Mutex) >= _Alignof(SRWLOCK), "dy_Mutex storage is under-aligned");
_Static_assert(sizeof(dy_Condition) >= sizeof(CONDITION_VARIABLE), "dy_Condition storage is too small");
_Static_assert(_Alignof(dy_Condition) >= _Alignof(CONDITION_VARIABLE), "dy_Condition storage is under-aligned");

static DWORD WINAPI dy_win32_thread_entry(void *context)
{
	dy_Win32_Thread_Start start = *(dy_Win32_Thread_Start *)context;
	HeapFree(GetProcessHeap(), 0, context);
	return start.function(start.context);
}

dy_Result dy_start_thread(dy_Thread_Function *function, void *context, dy_Thread *thread_result)
{
	dy_Result result = {0};
	dy_Win32_Thread_Start *start;
	HANDLE thread;
	if (thread_result) *thread_result = (dy_Thread){0};
	if (!function || !thread_result)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	start = HeapAlloc(GetProcessHeap(), 0, sizeof(*start));
	if (!start)
	{
		result.error = DY_ERROR_OUT_OF_MEMORY;
		result.os_error = ERROR_NOT_ENOUGH_MEMORY;
		return result;
	}
	start->function = function;
	start->context = context;
	thread = CreateThread(NULL, 0, dy_win32_thread_entry, start, 0, NULL);
	if (!thread)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		HeapFree(GetProcessHeap(), 0, start);
		return result;
	}
	thread_result->handle = (dy_uptr)thread;
	return result;
}

dy_b32 dy_thread_is_valid(dy_Thread thread)
{
	return thread.handle != 0;
}

dy_Result dy_join_thread(dy_Thread thread, dy_u32 *return_code)
{
	dy_Result result = {0};
	DWORD code;
	if (return_code) *return_code = 0;
	if (!return_code || !dy_thread_is_valid(thread))
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (WaitForSingleObject((HANDLE)thread.handle, INFINITE) != WAIT_OBJECT_0)
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	if (!GetExitCodeThread((HANDLE)thread.handle, &code))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
		return result;
	}
	*return_code = code;
	return result;
}

dy_Result dy_close_thread(dy_Thread *thread)
{
	dy_Result result = {0};
	if (!thread)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (thread->handle && !CloseHandle((HANDLE)thread->handle))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	*thread = (dy_Thread){0};
	return result;
}

dy_u64 dy_current_thread_id(void)
{
	return GetCurrentThreadId();
}

dy_Result dy_init_mutex(dy_Mutex *mutex)
{
	dy_Result result = {0};
	if (!mutex)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	*mutex = (dy_Mutex){0};
	InitializeSRWLock((SRWLOCK *)mutex->storage);
	return result;
}

void dy_lock_mutex(dy_Mutex *mutex)
{
	if (mutex) AcquireSRWLockExclusive((SRWLOCK *)mutex->storage);
}

void dy_unlock_mutex(dy_Mutex *mutex)
{
	if (mutex) ReleaseSRWLockExclusive((SRWLOCK *)mutex->storage);
}

void dy_destroy_mutex(dy_Mutex *mutex)
{
	if (mutex) *mutex = (dy_Mutex){0};
}

dy_Result dy_init_condition(dy_Condition *condition)
{
	dy_Result result = {0};
	if (!condition)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	*condition = (dy_Condition){0};
	InitializeConditionVariable((CONDITION_VARIABLE *)condition->storage);
	return result;
}

dy_Result dy_wait_condition(dy_Condition *condition, dy_Mutex *mutex)
{
	dy_Result result = {0};
	if (!condition || !mutex)
	{
		result.error = DY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!SleepConditionVariableSRW((CONDITION_VARIABLE *)condition->storage, (SRWLOCK *)mutex->storage, INFINITE, 0))
	{
		result.os_error = GetLastError();
		result.error = dy_win32_error(result.os_error);
	}
	return result;
}

void dy_signal_condition(dy_Condition *condition)
{
	if (condition) WakeConditionVariable((CONDITION_VARIABLE *)condition->storage);
}

void dy_broadcast_condition(dy_Condition *condition)
{
	if (condition) WakeAllConditionVariable((CONDITION_VARIABLE *)condition->storage);
}

void dy_destroy_condition(dy_Condition *condition)
{
	if (condition) *condition = (dy_Condition){0};
}
