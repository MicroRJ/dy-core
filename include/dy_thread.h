#ifndef DY_THREAD_H
#define DY_THREAD_H

#include "dy.h"

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

#endif
