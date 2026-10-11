#include "dy.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>

void *dy_virtual_reserve(dy_u64 size)
{
	if (size > SIZE_MAX) return NULL;
	return VirtualAlloc(NULL, (SIZE_T)size, MEM_RESERVE, PAGE_NOACCESS);
}

dy_b32 dy_virtual_commit(void *memory, dy_u64 size)
{
	if (size > SIZE_MAX) return 0;
	return VirtualAlloc(memory, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

void dy_virtual_release(void *memory, dy_u64 size)
{
	(void)size;
	if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}
