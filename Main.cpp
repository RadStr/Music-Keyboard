#include "Main.h"

#include <string>
#include <iostream>

#include <math.h>
#include <SDL.h>
#include <SDL_audio.h>

#include <SDL_ttf.h>

std::ofstream GlobalVariables::LOGGER;
const SDL_Color GlobalVariables::WHITE = { 255, 255, 255 };
const SDL_Color GlobalVariables::BLACK = { 0, 0, 0 };
const SDL_Color GlobalVariables::RED = { 255, 0, 0 };
const SDL_Color GlobalVariables::BACKGROUND_BLUE = { 0, 0, 100 };

// Returns -1 if the TTF couldn't be initialized
int main(int argc, char *argv[])
{
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Init(SDL_INIT_EVERYTHING);
	GlobalVariables::LOGGER.open("LOGGER.txt", std::ios::out | std::ios::app);
	GlobalVariables::LOGGER << std::endl << "----------------------------------------------------------------------" << std::endl;
	GlobalVariables::LOGGER << "Initializing window and renderer...";

	int err = SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE, &window, &renderer);
	if (err < 0) {
		GlobalVariables::LOGGER << "Initialization of window and renderer failed" << std::endl;
		GlobalVariables::LOGGER << "SDL error was: " << SDL_GetError() << std::endl;
		GlobalVariables::LOGGER << "Ending program." << std::endl;
		return err;
	}
	GlobalVariables::LOGGER << "Initialization of window and renderer completed sucessfully" << std::endl;
	
	GlobalVariables::LOGGER << "Initializing TTF (calling TTF_Init)...";
	err = TTF_Init();
	if (err == -1) {
		GlobalVariables::LOGGER << "Initialization of TTF (method TTF_Init) failed" << std::endl;
		GlobalVariables::LOGGER << "SDL error was: " << SDL_GetError() << std::endl;
		GlobalVariables::LOGGER << "Ending program." << std::endl;
		return -1;
	}
	GlobalVariables::LOGGER << "Initialization of TTF (method TTF_Init) completed sucessfully." << std::endl;
	
	try {
		GlobalVariables::LOGGER << "Calling constructor of Keyboard class..." << std::endl;
		Keyboard keyboard(window, renderer);
		GlobalVariables::LOGGER << "Instance of Keyboard class created successfully" << std::endl;

		GlobalVariables::LOGGER << "Entering main loop of program (calling mainCycle method)..." << std::endl;
		keyboard.mainCycle();
		GlobalVariables::LOGGER << "Ended main loop successfully (Exited main loop of program without exception)" << std::endl;
	}
	catch (std::exception e) {
		GlobalVariables::LOGGER << "Program failed: " << e.what() << std::endl;
		GlobalVariables::LOGGER << "SDL error was: " << SDL_GetError() << std::endl;
		SDL_ClearError();
	}

	// Close and destroy the window
	GlobalVariables::LOGGER << "SDL error was: " << SDL_GetError() << std::endl;
	SDL_ClearError();
	GlobalVariables::LOGGER << "Closing and destroying window..." << std::endl;
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	window = nullptr;
	renderer = nullptr;

	// Clean up
	SDL_Quit();
	GlobalVariables::LOGGER << "Ending program." << std::endl;
	GlobalVariables::LOGGER << "SDL error was: " << SDL_GetError() << std::endl;
	GlobalVariables::LOGGER.close();
	return 0;
}
