#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "dy_win32.h"

dy_u64 dy_counter(void)
{
	LARGE_INTEGER value;
	return QueryPerformanceCounter(&value) ? (dy_u64)value.QuadPart : 0;
}

dy_u64 dy_counter_frequency(void)
{
	LARGE_INTEGER value;
	return QueryPerformanceFrequency(&value) ? (dy_u64)value.QuadPart : 0;
}

dy_i64 dy_unix_time_ms(void)
{
	FILETIME time;
	GetSystemTimeAsFileTime(&time);
	return dy_win32_file_time_to_unix_ms(time);
}

void dy_sleep(dy_u64 milliseconds)
{
	while (milliseconds >= MAXDWORD)
	{
		Sleep(MAXDWORD - 1);
		milliseconds -= MAXDWORD - 1;
	}
	Sleep((DWORD)milliseconds);
}
