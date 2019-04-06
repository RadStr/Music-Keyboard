#pragma once

#include "Keyboard.h"
#include <SDL.h>

#define SUPPORT_PARALLEL 0

#if _WIN32
#define PATH_SEPARATOR "\\"
#elif __APPLE__
#define PATH_SEPARATOR ":"
#else
#define PATH_SEPARATOR "/"
#endif

//https://superuser.com/questions/407804/where-are-the-physical-font-files-stored 