#pragma once

#include "Keyboard.h"
#include <SDL.h>
#include <fstream>

#define DEBUG 0

#if _WIN32
#define PATH_SEPARATOR "\\"
#elif __APPLE__
#define PATH_SEPARATOR ":"
#else
#define PATH_SEPARATOR "/"
#endif

//https://superuser.com/questions/407804/where-are-the-physical-font-files-stored 

class GlobalVariables {
public:
	static std::ofstream LOGGER;
	static const SDL_Color WHITE;
	static const SDL_Color RED;
	static const SDL_Color BLACK;
	static const SDL_Color BACKGROUND_BLUE;
};