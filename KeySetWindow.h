#pragma once

#include "Keyboard.h"
// These 4 are already defined in Keyboard.h
#include "Key.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include "Textbox.h"


class KeySetWindow {
public:
	KeySetWindow(Key *key, SDL_Color color, Keyboard *keyboard);

	// Pointers, needs to be freed
	SDL_Window *window;
	SDL_Renderer *renderer;

	Keyboard *keyboard;			// Doesn't need to be freed
	Key *key;					// Doesn't need to be freed

	int windowWidth;
	int windowHeight;
	Textbox fileTextbox;
	Label keyLabel;
	SDL_Color color;
	bool needFileTextboxRedraw;
	bool needKeyLabelRedraw;
	bool isWindowEvent;


	// The main loop of this window, looks for events and redraws the window if needed
	// Ends and frees resources if window is closed
	bool mainCycle();

	// Checks type of event and performs corresponding action
	// Returns true if the main loop should be ended (Close event happened)
	// returnVal is set to true if the main window was closed, that means that we should close the whole application
	bool checkEvents(const SDL_Event &event, bool *returnVal);

	// Resizes the window, that means the coordinates and variables which depend on the window size will be set.
	void resizeWindow(int w, int h);

	// Resizes the window based on event (internally calls resizeWindow(int w, int h))
	void resizeWindow(const SDL_WindowEvent &event);

	// Sets the coordinates (of the button and text) of 2 textboxes in this window
	// The widths and heights are corresponding to the text, not to the button.
	void setWindowButtons(int fileTextboxWidth, int fileTextboxHeight, int keyLabelWidth, int keyLabelHeight);

	// Sets the coordinates of the keyLabel
	constexpr void setKeyLabel(int w, int h);

	// Sets the coordinates of the file textbox 
	constexpr void setFileTextbox(int w, int h);

	// Draws the window
	void drawWindow();

	// Prepares the textboxes for drawing (Doesn't draw them yet, SDL_RenderPresent needs to be called)
	void drawTextboxes();

	// Frees the resources of window (destroys the window)
	void freeResources();
};
