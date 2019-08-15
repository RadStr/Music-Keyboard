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
// TODO: In future maybe have option to set font, for this to be possible we will need to look for the ttf files in the OS specific directories

class LoggerClass {
private:
	LoggerClass();
	static std::ofstream LOGGER;

public:
	static std::ofstream& getLogger();
};

class GlobalConstants {
public:
	static constexpr SDL_Color WHITE = { 255, 255, 255 };
	static constexpr SDL_Color RED = { 255, 0, 0 };
	static constexpr SDL_Color BLACK = { 0, 0, 0 };
	static constexpr SDL_Color BACKGROUND_BLUE = { 0, 0, 100 };
};