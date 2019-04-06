#include "Main.h"

#include <string>
#include <iostream>

#include <math.h>
#include <SDL.h>
#include <SDL_audio.h>

#include <SDL_ttf.h>

const int AMPLITUDE = 28000;
const int SAMPLE_RATE = 22050;


// Returns -1 if the TTF couldn't be initialized
int main(int argc, char *argv[])
{
	SDL_Window *window;
	SDL_Renderer *renderer;

	SDL_Init(SDL_INIT_EVERYTHING);

	int err = SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE, &window, &renderer);
	if (err < 0) {
		return err;
	}

	err = TTF_Init();
	if (err == -1) {
		return -1;
	}

#if SUPPORT_PARALLEL
	unsigned int threadsSupported = std::thread::hardware_concurrency();
	if (threadsSupported > 1) {
		KeyboardParallel keyboard(window, renderer);
		keyboard.mainCycleParallel();
	}
	else {
		Keyboard keyboard(window, renderer);
		keyboard.mainCycle();
	}
#else
	Keyboard keyboard(window, renderer);
	keyboard.mainCycle();
#endif

	// Close and destroy the window
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	window = NULL;
	renderer = NULL;

	// Clean up
	SDL_Quit();
	return 0;

}
