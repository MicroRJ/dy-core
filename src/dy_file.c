#include "dy.h"

#if defined(_WIN32)
#include "win32/dy_file.c"
#else
#error unsupported operating system
#endif
