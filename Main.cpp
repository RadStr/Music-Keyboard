#include "Main.h"

#include <string>
#include <iostream>

#include <math.h>
#include <SDL.h>
#include <SDL_audio.h>

#include <SDL_ttf.h>

std::ofstream LoggerClass::LOGGER;

std::ofstream& LoggerClass::getLogger() {
	if (!LOGGER.is_open()) {
		LOGGER.open("LOGGER.txt", std::ios::out | std::ios::app);
	}
	return LOGGER;
}


// Returns -1 if the TTF couldn't be initialized
int main(int argc, char *argv[])
{
	// Unused parameters, but are needed for SDL library to find main method
	static_cast<void>(argc);
	static_cast<void>(argv);


	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Init(SDL_INIT_EVERYTHING);
	std::ofstream& logger = LoggerClass::getLogger();
	logger << std::endl << "----------------------------------------------------------------------" << std::endl;

	logger << "Initializing window and renderer...";
	int err = SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE, &window, &renderer);
	if (err < 0) {
		logger << "Initialization of window and renderer failed" << std::endl;
		logger << "SDL error was: " << SDL_GetError() << std::endl;
		logger << "Ending program." << std::endl;
		return err;
	}
	logger << "Initialization of window and renderer completed sucessfully" << std::endl;

	logger << "Initializing TTF (calling TTF_Init)...";
	err = TTF_Init();
	if (err == -1) {
		logger << "Initialization of TTF (method TTF_Init) failed" << std::endl;
		logger << "SDL error was: " << SDL_GetError() << std::endl;
		logger << "Ending program." << std::endl;
		return -1;
	}
	logger << "Initialization of TTF (method TTF_Init) completed sucessfully." << std::endl;
	
	try {
		logger << "Calling constructor of Keyboard class..." << std::endl;
		Keyboard keyboard(window, renderer);
		logger << "Instance of Keyboard class created successfully" << std::endl;

		logger << "Entering main loop of program (calling mainCycle method)..." << std::endl;
		keyboard.mainCycle();
		logger << "Ended main loop successfully (Exited main loop of program without exception)" << std::endl;
	}
	catch (std::exception e) {
		logger << "Program failed: " << e.what() << std::endl;
		logger << "SDL error was: " << SDL_GetError() << std::endl;
		SDL_ClearError();
	}
	
	// Close and destroy the window
	logger << "SDL error was: " << SDL_GetError() << std::endl;
	SDL_ClearError();
	logger << "Closing and destroying window..." << std::endl;
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	window = nullptr;
	renderer = nullptr;

	// Clean up
	SDL_Quit();
	logger << "Ending program." << std::endl;
	logger << "SDL error was: " << SDL_GetError() << std::endl;
	logger.close();
	return 0;
}
