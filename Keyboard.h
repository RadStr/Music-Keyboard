#pragma once
#include "Main.h"

#include "Key.h"
#include "SDL.h"
#include "Textbox.h"
//#include <fstream>
#include <vector>

#include <math.h>

#include <thread>
#include <chrono>

#include "SDL_ttf.h"

//#include<functional>		// Needs to be added for gcc compilation


// The declaration of function for callback (calls method on the Keyboard)
void audio_callback(void *userData, Uint8 *bufferToBePlayed, int bytes);

class Keyboard {
public:
	// Initializes the keyboard and associates it with given renderer and window
	Keyboard(SDL_Window *window, SDL_Renderer *renderer);


	static constexpr size_t MAX_KEYS = 88; 			// Key count on piano ... If the user wants to have more keys, 
	// he has to change the source code, change this MAX_KEYS macro 
	// and also change the defaultInitControlKeys, because only the first 88 keys are initialized

	static constexpr size_t CALLBACK_SIZE = 256;		// Size of the callback buffer, which is filled with audio to be played

	int whiteKeyWidth = 28;
	int whiteKeyHeight = 400;
	int blackKeyWidth = 3 * whiteKeyWidth / 4;
	int blackKeyHeight = whiteKeyHeight / 2;
	int upperLeftCornerForKeysY = whiteKeyHeight;

	Label audioPlayingLabel;
	PlayFileTextbox playFileTextbox;
	RecordFilePathTextbox recordFilePathTextbox;
	Textbox *textboxWithFocus;
	ConfigFileTextbox configFileTextbox;
	DirectoryWithFilesTextbox directoryWithFilesTextbox;

	std::vector<Key> keys;
	std::vector<Uint8> vectorOfCallbackSize;
	SDL_AudioSpec audioSpec;
	// Pointers, needs to be freed
	SDL_AudioCVT *audioFromFileCVT;		// Stores the audio buffer from wav to be played

	SDL_Renderer *renderer;

	Key *keyPressedByMouse;		// Points to the key, so don't free it (Would free 1 key twice)
	SDL_Window *window;			// Don't free the window, the caller should do that
	// End of pointers

	int windowWidth;
	int windowHeight;
	Key recordKey;
	Uint32 recordStartTime;
	std::vector<Uint8> record;
	std::vector<TimestampAndID> recordedKeys;
	std::vector<Key *> currentlyPressedKeys;

	// Vector of vectors because of multiple channels ... vector[0] contains channel 0 of all just unpressed keys, etc.
	// currentlyUnpressedKeys.size() = this->audioSpec.channels 
	std::vector<std::vector<Sint32>> currentlyUnpressedKeys;			// The reasoning for this is to not have crackling when key is unpressed 
														// It happens, because the sound amplitude goes from some high value to 0 immediately and the
														// HW which plays the sound can't produce it correctly, so what we do is that we gradually lower the
														// amplitude of just unpressed key until the silence value is reached. (At least I think that is the reason).

	SDL_AudioDeviceID audioDevID;
	size_t whiteKeysCount;
	size_t blackKeysCount;
	bool isLastKeyBlack;
	Uint32 audioFromFileBufferIndex;
	SDL_Color keySetWindowTextColor;

	bool quit; 
	bool isRecording;
	bool shouldRedrawTextboxes;
	bool shouldRedrawKeys;
	bool shouldRedrawKeyLabels;

	std::vector<SDL_Texture *> textures;



//////////////////////////////////////////// Static methods
public:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Set methods from config file - In KeyboardSet.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sets the keysym modifiers of given key based on token split by modDelim
	// Returns true if the modifiers were valid, false otherwise
	static bool setModifiersForKeyFromConfigFile(Key *key, const std::string &token, char modDelim);

	// Sets the keysym scancode and sym of given key based on token
	// Returns true if the control keys were valid, false otherwise
	static bool setControlForKeyFromConfigFile(Key *key, const std::string &token);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Init methods - In KeyboardInit.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Fills the key audio buffer with default tone corresponding to that key
	static void initKeyWithDefaultAudioStatic(Key *key, const SDL_AudioSpec &audioSpecLocal);
#define initKeyWithDefaultAudioNonStatic(key) initKeyWithDefaultAudioStatic(key, this->audioSpec)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Convert methods - In KeyboardAudio.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Should be called with keyboard->audioSpec (this->audioSpec) as desiredSpec.
	// Doesn't free the buffer.
	static void convert(SDL_AudioCVT *cvt, SDL_AudioSpec *spec, SDL_AudioSpec *desiredSpec, Uint8 *buffer, Uint32 len);

	// This method takes the cvt->buf with length currentCVTBufferLen.
	// Converts it by using SDL_ConvertAudio and if there was convert overhead in memory.
	// Then copies the valid bytes from the buffer to new buffer and that new buffer assigns to the cvt->buf.
	// If there wasn't any overhead doesn't do anything.
	// If cvt buffer is nullptr or currentCVTBufferLen is 0 the function doesn't do anything.
	// Also frees the old content of the cvt->buf.
	static void convertAudioAndSaveMemory(SDL_AudioCVT *cvt, Uint32 currentCVTBufferLen);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Sound synthesis methods - In KeyboardAudio.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Fills the buffer in keyCVT with tone based on keyID, in format from given spec
	// So in the end the buffer will contain the tone in spec given as parameter.
	// And it will contain the numberOfSeconds of audio 
	static void generateTone(const SDL_AudioSpec &spec, int keyID, Uint32 numberOfSeconds, SDL_AudioCVT *keyCVT);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Create audio files methods - In KeyboardAudio.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Creates the .keys file which will have full path path.keys
	static void createKeyFile(const std::string &path, std::vector<TimestampAndID> &recordedKeys);
#define createKeyFileFromRecordedKeys(path) createKeyFile(path, this->recordedKeys)			// Define more specific function for current instance of keyboard

	// Creates the .wav file which will have full path path.wav
	static void createWavFile(const std::string &path, std::vector<Uint8> record, const SDL_AudioSpec &audioSpecWav);
#define createWavFileFromAudioBuffer(path) createWavFile(path, this->record, this->audioSpec)				// Define more specific function for current instance of keyboard

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Calculation help methods - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ID is index of key (indexing from 0)
	// Returns true if key with ID is white.
	static bool isWhiteKey(size_t ID);


protected:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Text/Font methods - In KeyboardDraw.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Returns number < 0 if the keyLabelPart doesn't fit, else returns currY where should be placed next keyLabel for that key
	static int testTextSize(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
		int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures);

	// widthTolerance tells how much space should be between the text and end of white key 
	// (so there will be spaces between key labels and it will be clear where one label ends and other starts)
	// This method ensures, that if it has smaller width than whiteKeyWidth, then it will be draw in the middle
	// Changes messageWidth 
	// Returns number which tells how much we should shift the x coordinate of the label, that it will be in the middle
	static int countKeyLabelSpaceAndWidth(int *messageWidth, int jumpX, int widthTolerance, int whiteKeyWidth);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Text/Font draw methods - In KeyboardDraw.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Draws the keyLabelPart with current font, doesn't care about width of text.
	static int drawKeyLabelPartWithoutWidthLimit(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
		int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures);

	// Draws the keyLabelPart with current font, cares about width of text.
	static int drawKeyLabelPartWithWidthLimit(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
		int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures);


//////////////////////////////////////////// Instance methods
public:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Endless loop methods - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// The main cycle is while loop, which just redraws the window when needed and checks for events. 
		// The loop is exited when either exception occurs or quit event was registered (User clicked on exit cross)
		int mainCycle();

		// Functions return int to give feedback if the function processsed correctly.
		// Is called in the main loop, processes the given event
		void checkEvents(const SDL_Event &event);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Audio methods - In KeyboardAudio.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Is called as callback function to fill the bufferToBePlayed, which will be played next.
		// (Respectively audio_callback function is the real callback function, but it calls this one) 
		// Function is called by the SDL library, every time the buffer which will be played needs to refilled. 
		void playPressedKeysCallback(Uint8 *bufferToBePlayed, int bytes);

		// Reduces the crackling by using the currentlyUnpressedKeys vector, which is done by gradually adjusting the sample values, 
		// instead of just putting silence in the buffer, which may produce crackling because of sudden change in sample values. 
		// For further understanding take look at the comments at currentlyUnpressedKeys or at source code.
		void reduceCrackling(Uint8 *bufferToBePlayed, int bytes);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Draw methods - In KeyboardDraw.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Draw methods don't actually draw (except drawWidnow), they set all the things that will be drawn next time, will be drawn by using SDL_RenderPresent
		void drawWindow();
		// Clears the renderer and sets background color
		void drawBackground();
		// Draws textboxes (the text and the box around)
		void drawTextboxes();
		// Draws the play keys and the record keys
		void drawKeys();
		// Draws the labels for all the keys
		void drawKeyLabels();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Methods working with config file - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// totalLinesInFile says how many lines (keys) were really in file - 1 (the first line should contain number of keys on keyboard, so we don't count that)
		int readConfigfile(size_t *totalLinesInFile);

		// Processes all the lines in config file after the first one and initializes al lthe keys based on the config file
		int processKeysInConfigFile(std::ifstream &s, std::string &line, size_t totalLineCountInConfig, size_t *totalLinesInFile);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Play methods - In KeyboardAudio.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Plays file based on extension, only .wav and .keys formats are currently supported
		void playFile(const std::string &path);

		// Plays the .keys file, window is not responding when the file is being played.
		void playKeyFile(const std::string &path);

		// Plays the .wav file, user can use everything while the file is being played (even record).
		void playWavFile(const std::string &path);

private:
		static bool SDL_KeyboardEventComparator(const SDL_KeyboardEvent& a, const SDL_KeyboardEvent& b);

		static void addForCyclesToEvents(std::vector<std::tuple<Uint32, Uint32, Uint32, std::vector<SDL_KeyboardEvent>>> forCycles, std::vector<SDL_KeyboardEvent> &events);
public:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Record methods - In KeyboardAudio.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Starts to record
		void startRecording(Uint32 timestamp);

		// Ends recording
		void endRecording();

		void addToRecordedKeys(Key *key, Uint32 timestamp, KeyEventType keyET);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Init methods - In KeyboardInit.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// If configFile == "" then it is same as calling defaultInit, else inits keyboard based on given configFile
		// totalLineInFile returns the total number of lines in config file without the first line (which tells number of keys in file)
		int initKeyboard(const std::string &configFile, size_t *totalLinesInFile);

		// Initializes all the keys and almost all properties
		// If initAudioBuffer == true then initialize the buffers of keys with the default tone
		int defaultInit(size_t totalKeys, bool initAudioBuffer);

		// Initialization of HW, initializes the audioSpec, opens the audio device and starts the callback calls
		void initAudioHW();

		// It is just default initialization of controls for all the keys
		constexpr void defaultInitControlKeys(size_t currKey);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Set methods - In KeyboardSet.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Sets blackKeysCount and whiteKeysCount properties
		void setBlackAndWhiteKeysCounts();

		// Sets renderer ... if it already exists, then don't do anything
		void setRenderer();

		// Goes through to directory and gradually set sounds to keys on keyboard from left to right.
		// That means iteratively goes through directory and if file with .wav extension is found, then set the first non-set key (going from left)
		void setKeySoundsFromDirectory();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Key set methods - In KeyboardSet.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Sets key location based on color of key (black or white)
		constexpr void setKeyLocation(int *currX, size_t index);

		// Sets variables such as whiteKeyHeight etc. based on the size of window
		constexpr void setKeySizes();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Resize methods - In KeyboardSet.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Resizes keyboard properties based on new width and height of window and sets corresponding properties
		void resizeWindow(SDL_WindowEvent windowEvent);

		// Resizes textboxes based on current width and height of window
		void resizeTextboxes();

		// Resizes keyboard keys (including record key) based on current width and height of window
		void resizeKeyboardKeys();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Free methods - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Release all resources of this instance, except window and renderer
		void freeKeyboard();

		// Release everything associated with keys
		void freeKeys();

		void freeTextures();

private:
		// Frees memory associated with the buffer and then the SDL_AudioCVT itself and sets it to 0
		static void freeSDL_AudioCVTptr(SDL_AudioCVT **cvt);

public:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Labels/Font methods - In KeyboardDraw.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Returns font such that the labels of all the play keys fit within the whiteKeyWidth - widthTolerance
	TTF_Font * findFontForPlayKeys(int widthTolerance);

	// Perform f function on all the text labels of key
	int performActionOnKeyLabels(const Key *key, TTF_Font *font, int widthTolerance, const bool draw,
		std::function<int(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
			int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures)> f);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Help methods - In KeyboardSet.cpp - countBlackKeys is defined as constexpr, so it needs to be defined in every file where it is used
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Counts number of black keys, if total number of keys on keyboard == keyCountLocal
	constexpr static size_t countBlackKeys(size_t keyCountLocal);		// Is constexpr so it has to be defined in Keyboard.cpp


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Pressing/unpressing keys methods - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Is called when play key is pressed, performs needed actions (adding to currentlyPressedKeys, etc. (for example if recording then add to recordedKeys))
	void keyPressAction(Key *key, Uint32 timestamp);

	// Is called when play key is unpressed, performs needed actions (adding to currentlyUnpressedKeys and removing from currentlyPressedKeys etc.)
	void keyUnpressAction(Key *key, Uint32 timestamp, size_t index);

	// Adds key to currentlyUnpressedKeys, looks what samples were played last and adds them.
	void addToUnpressedKeys(Key *key);

	// Checks if it was button pressed or unpressed event and if the key is associated with either record key or played key, based on that performs action
	// Returns pressed key (nullptr if none of the keyboard keys was pressed)
	Key * findKeyAndPerformAction(const SDL_KeyboardEvent &keyEvent);

private:
	// Checks if the event has the same mod (or valid subset for example if alt is mod then the either left or right alt can be pressed)
	static bool checkForMods(const Uint16 keyMod, const Uint16 keyEventMod);

public:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Mouse methods - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Checks what was pressed (textbox, key, nothing, etc.) and if it was left or right click, based on that perform action
	// Returns pressed key (in case of textbox or when nothing was pressed returns nullptr)
	Key * checkMouseClickAndPerformAction(const SDL_MouseButtonEvent &event);

	// Finds which play key was pressed (Is called only when we know that the the place where we clicked hit some key but we don't know which)
	// Returns the clicked key
	Key * findKeyOnPos(Sint32 x, Sint32 y);

	// Releases play key pressed (or does nothing if key wasn't pressed)
	void unpressedMouseButtonAction(const SDL_MouseButtonEvent &event);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Textbox action methods - In KeyboardMain.cpp
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Set foci of textboxes and textboxWithFocus and starts accepting text input by calling SDL_StartTextInput
	void textboxPressAction(Textbox &clickedTextbox, const SDL_MouseButtonEvent &event);
};