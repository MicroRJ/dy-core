#include "dy_memory.h"

#if defined(_WIN32)
#include "win32/dy_memory.c"
#elif defined(__linux__)
#include "linux/dy_memory.c"
#else
#error unsupported operating system
#endif
