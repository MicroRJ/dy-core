#include "dy.h"

#if defined(_WIN32)
#include "win32/dy_console.c"
#else
#error unsupported operating system
#endif
