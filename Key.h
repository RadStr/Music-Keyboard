#pragma once

#include <string>
#include "SDL.h"


enum KeyEventType {
	KEY_PRESSED = 0,			
	KEY_RELEASED = 1
};

class TimestampAndID {
public:
	// Constructor just copies constructor arguments to properties
	TimestampAndID(Uint32 timestamp, int ID, KeyEventType keyEventType);

	Uint32 timestamp;
	int ID;
	KeyEventType keyEventType;
};

class Button {
public:
	// Default Constructor for button
	Button();

	SDL_Rect rectangle;

	// Check if the rectangle representing this button was clicked
	bool checkMouseClick(const SDL_MouseButtonEvent &event);

	// Resizes the rectangle representing the button
	void resizeButton(int x, int y, int w, int h);
};

class Key : public Button {
public:
	// Default constructor for key
	Key();

	// Initializes the button size by given parameters
	Key(int x, int y, int w, int h);


	// Points to the audioConvert.buf and tells the first byte to be played in next call of callback function
	int startOfCurrAudio;
	// Contains the buffer with the audio to be played converted to the format of the keyboards (respectively audioDevice which the keyboards use for playing)
	SDL_AudioCVT audioConvert;	 

	
	// Key ID (indexing from 0 ... left key on keyboard has index 0)
	int ID;

	// == true if it was initialized by the default tone (If the audio buffer wasn't initialized from file)
	// == false otherwise
	bool isDefaultSound;
	
	// How many times is the key currently pressed (For situations when key is pressed by mouse and also keyboard, or when 1 key is on multiple keys)
	int pressCount;

	// The key which presses the key
	SDL_Keysym keysym;

	// Initialize the key variables
	void initKey();

	// Initialize the audioConvert.buf with the audio from audio file given in filename. The buffer is in the desiredAudioSpec
	void setAudioBufferWithFile(char *filename, SDL_AudioSpec *desiredAudioSpec);

	// Frees the resources associated with the key
	void freeKey();
};