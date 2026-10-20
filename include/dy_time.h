#ifndef DY_TIME_H
#define DY_TIME_H

#include "dy.h"

dy_u64 dy_counter(void);
dy_u64 dy_counter_frequency(void);
dy_i64 dy_unix_time_ms(void);
void dy_sleep(dy_u64 milliseconds);

#endif
