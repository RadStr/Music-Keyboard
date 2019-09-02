// The biggest problem this software has, is the limitaion of hardware, more specifically the anti-ghosting mechanism, which blocks combination of certain
// keys to be registered, because they may produce some next key which wasn't pressed. 
// For example on my keyboard when the keys A,S,W are pressed at the same moment, keyboard registers them all.
// But when keys I,J,K are pressed all the same moment,
// The keyboard ignores that they were pressed, that means keydown event isn't registered for any of the keys and therefore not a single sound will be played.

#include "Keyboard.h"
#include "KeySetWindow.h"

#include "time.h"
#include <ctime>

#include <sstream>
#include <iostream>

#include <algorithm>

#include <thread>

#define DEBUG 0


// The callback function internally called by SDL to get audio to be played 
void audio_callback(void *userData, Uint8 *bufferToBePlayed, int bytes) {
#if DEBUG
	std::ofstream& logger = LoggerClass::getLogger();
	logger << "In audio_callback()..." << std::endl;
#endif
	static_cast<Keyboard *>(userData)->playPressedKeysCallback(bufferToBePlayed, bytes);
#if DEBUG
	logger << "Exit audio_callback()..." << std::endl;
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Endless loop methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int Keyboard::mainCycle() {
	SDL_Event event;
	this->quit = false;
	std::ofstream& logger = LoggerClass::getLogger();

	logger << "Drawing window...";
	drawWindow();
	logger << "Window drawn successfully" << std::endl;
	bool render = false;

	logger << "\"Endless\" loop starting..." << std::endl;
	while (!this->quit) {
#if DEBUG
		logger << "Calling SDL_PollEvent...";
#endif
		if (SDL_PollEvent(&event)) {
#if DEBUG
			logger << "SDL_PollEvent ended." << std::endl;
#endif

			shouldRedrawKeys = false;
			shouldRedrawTextboxes = false;
			shouldRedrawKeyLabels = false;
			render = false;

#if DEBUG
			logger << "Calling checkEvents()..." << std::endl;
#endif
			checkEvents(event);
#if DEBUG
			logger << "checkEvents() ended." << std::endl;
			logger << "Checking what needs to be redrawed..." << std::endl;
#endif


			if (shouldRedrawKeys) {
#if DEBUG
				logger << "Calling drawKeys()...";
#endif
				drawKeys();
#if DEBUG
				logger << "drawKeys() ended." << std::endl;
#endif
				render = true;
			}
			if (shouldRedrawTextboxes) {
#if DEBUG
				logger << "Calling drawTextboxes()...";
#endif
				drawTextboxes();
#if DEBUG
				logger << "drawTextboxes() ended." << std::endl;
#endif
				render = true;
			}
			if (shouldRedrawKeyLabels) {
#if DEBUG
				logger << "Calling drawKeyLabels()...";
#endif
				drawKeyLabels();
#if DEBUG
				logger << "drawKeyLabels() ended." << std::endl;
#endif
				render = true;
			}
			if (render) {
#if DEBUG
				logger << "Calling SDL_RenderPresent()...";
#endif
				SDL_RenderPresent(this->renderer);
#if DEBUG
				logger << "SDL_RenderPresent ended.";
#endif
			}
		}
	}

	logger << "\"Endless\" loop ended" << std::endl;
	logger << "Freeing keyboard resources (calling freeKeyboard)...";
	this->freeKeyboard();
	logger << "Keyboard resources freed" << std::endl;
	return 0;
}


void Keyboard::checkEvents(const SDL_Event &event) {
	bool enterPressed = false;
	bool shouldResizeTextboxes = false;
#if DEBUG
	std::ofstream& logger = LoggerClass::getLogger();
#endif

	switch (event.type) {
	case SDL_TEXTINPUT:
		if (this->textboxWithFocus != nullptr) {
			this->shouldRedrawTextboxes = this->textboxWithFocus->processKeyEvent(event, &enterPressed, &shouldResizeTextboxes);
			if (enterPressed) {
			
				this->textboxWithFocus->processEnterPressedEvent(*this);
			}
		}
		break;
	case SDL_KEYDOWN:
#if DEBUG
		logger << "Key Pressed" << std::endl;
		logger << event.key.keysym.mod << std::endl;
		logger << event.key.keysym.scancode << std::endl;
		logger << event.key.timestamp << std::endl;
#endif
		if (this->textboxWithFocus != nullptr) {
			this->shouldRedrawTextboxes = this->textboxWithFocus->processKeyEvent(event, &enterPressed, &shouldResizeTextboxes);
			if (enterPressed) {
				this->textboxWithFocus->processEnterPressedEvent(*this);
			}
		}
		else {
			this->findKeyAndPerformAction(event.key);
		}
		break;
	case SDL_KEYUP:
		if (this->textboxWithFocus == nullptr) {
			this->findKeyAndPerformAction(event.key);
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		this->checkMouseClickAndPerformAction(event.button);			// May change this->quit if we ended the app from other window
		break;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT) {
			this->unpressedMouseButtonAction(event.button);
		}
		break;
	case SDL_WINDOWEVENT:
		switch (event.window.event) {
		case SDL_WINDOWEVENT_RESIZED:
			resizeWindow(event.window);
			// Redraw
			drawWindow();
			break;
		default:
			break;
		}
		break;
	case SDL_QUIT:
		this->quit = true;
	default:
		break;
	}

	if (shouldResizeTextboxes) {
		this->resizeTextboxes();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Pressing/unpressing keys methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Keyboard::keyPressAction(Key *key, Uint32 timestamp) {
	this->shouldRedrawKeys = true;

	if (key->pressCount == 0) {
		key->startOfCurrAudio = 0;
		currentlyPressedKeys.push_back(key);
	}
	key->pressCount++;

	addToRecordedKeys(key, timestamp, KeyEventType::KEY_PRESSED);
}

void Keyboard::keyUnpressAction(Key *key, Uint32 timestamp, size_t index) {
	this->shouldRedrawKeys = true;

	key->pressCount--;
	if (key->pressCount == 0) {
		addToUnpressedKeys(key);
		currentlyPressedKeys.erase(currentlyPressedKeys.begin() + index);
	}
	else if (key->pressCount < 0) {
		throw std::range_error("Button unpressed more times than it was pressed");					// Shouldn't happen
	}

	addToRecordedKeys(key, timestamp, KeyEventType::KEY_RELEASED);
}

void Keyboard::addToUnpressedKeys(Key *key) {
	void *sampleType;
	Uint16 format = this->audioSpec.format;
	int bufferIndex = key->startOfCurrAudio;
	for (size_t ch = 0; ch < this->audioSpec.channels; ch++, bufferIndex += SDL_AUDIO_BITSIZE(this->audioSpec.format) / 8) {
		sampleType = &key->audioConvert.buf[bufferIndex];

		switch (format) {
		case AUDIO_U8:
			this->currentlyUnpressedKeys[ch].push_back(*static_cast<Uint8 *>(sampleType));
			break;
		case AUDIO_S8:
			this->currentlyUnpressedKeys[ch].push_back(*static_cast<Sint8 *>(sampleType));
			break;
		case AUDIO_U16SYS:
			this->currentlyUnpressedKeys[ch].push_back(*static_cast<Uint16 *>(sampleType));
			break;
		case AUDIO_S16SYS:
			this->currentlyUnpressedKeys[ch].push_back(*static_cast<Sint16 *>(sampleType));
			break;
		case AUDIO_S32SYS:
			this->currentlyUnpressedKeys[ch].push_back(*static_cast<Sint32 *>(sampleType));
			break;
		case AUDIO_F32SYS:			// In this case we just skip it, it takes too much work to work with float
		default:
			break;
		}
	}
}


Key * Keyboard::findKeyAndPerformAction(const SDL_KeyboardEvent &keyEvent) {
	Key *key;
	if (keyEvent.repeat != 0) {				// Key repeat, don't need this event, I am already holding state in Key's property pressCount
		return nullptr;
	}

	if (keyEvent.type == SDL_KEYUP) {
		for (size_t i = 0; i < this->currentlyPressedKeys.size(); i++) {
			key = currentlyPressedKeys[i];

			if (keyEvent.keysym.sym == key->keysym.sym) {
				keyUnpressAction(key, keyEvent.timestamp, i);
				return key;
			}
		}
	}
	else {
		if (this->recordKey.keysym.sym == keyEvent.keysym.sym && this->recordKey.keysym.mod == keyEvent.keysym.mod) {
			this->shouldRedrawKeys = true;
			if (isRecording) {
				endRecording();
				return &recordKey;
			}
			else {
				startRecording(keyEvent.timestamp);
				return &recordKey;
			}
		}
		for (size_t i = 0; i < this->keys.size(); i++) {
			key = &this->keys[i];
			// It is exactly the mod, or maybe it is using just one of the possible mods (for example 1 of the 2 shift keys)
			if (key->keysym.sym == keyEvent.keysym.sym && (key->keysym.mod == keyEvent.keysym.mod || checkForMods(key->keysym.mod, keyEvent.keysym.mod))) {
				keyPressAction(key, keyEvent.timestamp);
				return key;
			}
		}
	}

	return nullptr;
}


bool Keyboard::checkForMods(const Uint16 keyMod, const Uint16 keyEventMod) {
	SDL_Keymod mod1;
	SDL_Keymod mod2;
	SDL_Keymod mod3;

	// Check for each modificator, where mod1, mod2, mod3 tells if it has modifier of that type, so in the end we just check if there was any modifier, 
	// if there wasn't then it should have been equal, so this method returns false, because we checked for equality elsewhere
	Uint16 km = KMOD_CTRL | KMOD_SHIFT | KMOD_SHIFT;
	km = ~km;
	km = km & keyMod;
	Uint16 keyModWithoutMod = km & keyMod;
	Uint16 keyEventModWithoutMod = km & keyEventMod;

	mod1 = static_cast<SDL_Keymod>(KMOD_CTRL & keyMod);
	mod2 = static_cast<SDL_Keymod>(KMOD_SHIFT & keyMod);
	mod3 = static_cast<SDL_Keymod>(KMOD_ALT & keyMod);

	return ((keyMod & keyEventMod) == keyEventMod && keyModWithoutMod == keyEventModWithoutMod &&
		((KMOD_CTRL & keyEventMod) != KMOD_NONE || mod1 == KMOD_NONE) &&
		((KMOD_SHIFT & keyEventMod) != KMOD_NONE || mod2 == KMOD_NONE) &&
		((KMOD_ALT & keyEventMod) != KMOD_NONE || mod3 == KMOD_NONE) &&
		(mod1 != 0 || mod2 != 0 || mod3 != 0));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Textbox action methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Keyboard::textboxPressAction(Textbox &clickedTextbox, const SDL_MouseButtonEvent &event) {
	this->shouldRedrawTextboxes = true;

	clickedTextbox.hasFocus = !clickedTextbox.hasFocus;
	bool newFocus = clickedTextbox.hasFocus;

	this->directoryWithFilesTextbox.hasFocus = false;
	this->configFileTextbox.hasFocus = false;
	this->recordFilePathTextbox.hasFocus = false;
	this->playFileTextbox.hasFocus = false;

	if (newFocus) {
		clickedTextbox.hasFocus = true;
		this->textboxWithFocus = &clickedTextbox;
	}
	else {
		this->textboxWithFocus = nullptr;
	}

	while (this->currentlyPressedKeys.size() > 0) {
		keyUnpressAction(this->currentlyPressedKeys[0], event.timestamp, 0);
	}

	SDL_StartTextInput();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Mouse methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Key * Keyboard::checkMouseClickAndPerformAction(const SDL_MouseButtonEvent &event) {
	// First check textboxes
	// The else branch is executed when no textbox was clicked.
	if (this->configFileTextbox.button.checkMouseClick(event)) {
		textboxPressAction(this->configFileTextbox, event);
		return nullptr;
	}
	else if (this->directoryWithFilesTextbox.button.checkMouseClick(event)) {
		textboxPressAction(this->directoryWithFilesTextbox, event);
		return nullptr;
	}
	else if (this->recordFilePathTextbox.button.checkMouseClick(event)) {
		textboxPressAction(this->recordFilePathTextbox, event);
		return nullptr;
	}
	else if (this->playFileTextbox.button.checkMouseClick(event)) {
		textboxPressAction(this->playFileTextbox, event);
		return nullptr;
	}
	else {
		if (this->textboxWithFocus != nullptr) {
			this->shouldRedrawTextboxes = true;
		}
		this->directoryWithFilesTextbox.hasFocus = false;
		this->configFileTextbox.hasFocus = false;
		this->recordFilePathTextbox.hasFocus = false;
		this->playFileTextbox.hasFocus = false;

		SDL_StopTextInput();
		this->textboxWithFocus = nullptr;
	}


	// First check if it is the record key, since it behaves differently
	if (this->recordKey.checkMouseClick(event)) {
		this->shouldRedrawKeys = true;

		if (isRecording) {
			endRecording();
			return &recordKey;
		}
		else {
			startRecording(event.timestamp);
			return &recordKey;
		}
	}

	// Check if it can be Key
	// this->keys[0] is always the white key
	// Check if it is in the rectangle where are all the play keys
	if (event.y >= this->keys[0].rectangle.h + this->upperLeftCornerForKeysY || event.x <= 0 || event.y <= this->upperLeftCornerForKeysY) {
		return nullptr;
	}
	if (event.x >= static_cast<int>(this->whiteKeysCount) * this->keys[0].rectangle.w) {		// It is behind the last white key
		if (this->isLastKeyBlack) {											// If the black key is last then it has different borders
			if (event.x >= this->keys[keys.size() - 1].rectangle.x + this->keys[keys.size() - 1].rectangle.w || // It is behind the last black key
				event.y >= this->keys[keys.size() - 1].rectangle.y + this->keys[keys.size() - 1].rectangle.h) { // It is under the last black key
				return nullptr;
			}
		}
		else {
			return nullptr;
		}
	}

	// Now it is some of the play keys for sure
	this->shouldRedrawKeys = true;
	// Now find the key
	Key *key = this->findKeyOnPos(event.x, event.y);
	keyPressedByMouse = key;
#if DEBUG
	std::cout << keyPressedByMouse->ID << "\tFreq: " << 440 * pow(2, (static_cast<int>(keyPressedByMouse->ID) - 49) / static_cast<double>(12)) << std::endl;
#endif
	if (event.button == SDL_BUTTON_LEFT) {
		keyPressAction(key, event.timestamp);
		return key;
	}
	else if (event.button == SDL_BUTTON_RIGHT) {
		KeySetWindow keySetWindow(key, keySetWindowTextColor, this);
		this->quit = keySetWindow.mainCycle();
		this->keyPressedByMouse = nullptr;
		shouldRedrawKeys = true;
		shouldRedrawTextboxes = true;
		shouldRedrawKeyLabels = true;
	}

	return key;
}


Key * Keyboard::findKeyOnPos(Sint32 x, Sint32 y) {
	if (keyPressedByMouse != nullptr) {
		throw std::range_error("Mouse button pressed twice error without unpressing");					// Shouldn't happen
	}

	size_t index = (x / this->whiteKeyWidth);			// index of the width key where the mouse pointed (if there weren't any black ones)
	size_t mod = index % 7;
	int keyStartX = static_cast<int>(index) * this->whiteKeyWidth;	// Calculate the x coordinate of the last white key
	int nextKeyStartX = keyStartX + this->whiteKeyWidth;	// Coordinate of the next white key
	size_t blackKeysCountLocal = 5 * (index / 7);   // For every 7 white keys there is 5 black ones
	if (mod >= 1) {							// Now just add the black keys based on the mod of white keys
		blackKeysCountLocal++;
		if (mod >= 3) {
			blackKeysCountLocal++;
			if (mod >= 4) {
				blackKeysCountLocal++;
				if (mod >= 6) {
					blackKeysCountLocal++;
				}
			}
		}
	}


	index += blackKeysCountLocal;

	// It can be seen on the keyboards (after the 0th key there is black key following, after then 1st there isn't, etc.)
	if (mod != 1 && mod != 4 &&											// After these white keys there isn't any black key	
		index < keys.size() - 1 &&											// We won't reach behind the keyboard
		y <= (this->blackKeyHeight + this->upperLeftCornerForKeysY) &&	// It is in the black key with y coordinate
		x >= (nextKeyStartX - this->blackKeyWidth / 2) &&				//							   x
		x <= (nextKeyStartX + this->blackKeyWidth / 2))					//                             x
	{
		return &this->keys[index + 1];		// It was the next black key
	}
	else if ((index != 0 || mod != 0) && mod != 2 && mod != 5 &&		// Before these white keys there isn't any black key 
		index <= keys.size() &&											// It isn't after the keyboard
		y <= (this->blackKeyHeight + this->upperLeftCornerForKeysY) &&	// It is in the black key with y coordinate
		x >= (keyStartX - this->blackKeyWidth / 2) && x <= (keyStartX + this->blackKeyWidth / 2))		// It is black key with x coordinate
	{
		return &this->keys[index - 1];		// It was the previous black key
	}
	else {
		return &this->keys[index];			// It was white key 
	}
}


void Keyboard::unpressedMouseButtonAction(const SDL_MouseButtonEvent &event) {
	if (this->keyPressedByMouse == nullptr) {
		return;
	}

	Key *key = this->keyPressedByMouse;
	this->keyPressedByMouse = nullptr;

	for (size_t i = 0; i < this->currentlyPressedKeys.size(); i++) {
		if (key == currentlyPressedKeys[i]) {
			keyUnpressAction(key, event.timestamp, i);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Methods working with config file 
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// First line is number of keys ( <= MAX_KEYS )		
// Then on each row is 1 key:
// Tries to read config file which is in the configFileTextbox
int Keyboard::readConfigfile(size_t *totalLinesInFile) {
	std::ifstream stream(this->configFileTextbox.text);

	if (stream.is_open()) {
		std::string line;
		Uint32 totalLineCountInConfig = 0;

		if (std::getline(stream, line)) {
			try {
				totalLineCountInConfig = std::stoul(line, nullptr, 10);
			}
			catch (std::exception e) {
				this->defaultInit(MAX_KEYS, true);
				return -1;
			}

			if (totalLineCountInConfig <= 0) {
				return this->defaultInit(MAX_KEYS, true);
			}
			else if (totalLineCountInConfig > MAX_KEYS) {
				totalLineCountInConfig = MAX_KEYS;
				return Keyboard::processKeysInConfigFile(stream, line, totalLineCountInConfig, totalLinesInFile);		// Init only the first MAX_KEYS keys
			}
		}
		else {				// Empty file
			return this->defaultInit(MAX_KEYS, true);
		}

		return Keyboard::processKeysInConfigFile(stream, line, totalLineCountInConfig, totalLinesInFile);		// Not to many files in config file
	}

	return 1;
}


// Returns 2 if expectedLineCountInConfig > real line count in file
// Returns 1 if expectedLineCountInConfig < real line count in file
// Returns 0 if expectedLineCountInConfig == real line count in file
// Returns -1 if some error happened (and logs it)
int Keyboard::processKeysInConfigFile(std::ifstream &stream, std::string &line, size_t expectedLineCountInConfig, size_t *totalLinesInFile) {
	bool hasRecordKey = false;
	defaultInit(expectedLineCountInConfig, false);
	std::string filename;
	std::string token;
	std::string modifierToken;
	char delim = ' ';
	char modDelim = '+';
	int keyNumber = -1;
	int lastKeyNumber = -1;
	size_t currentToken;
	size_t firstSpaceInd = 0;

	size_t currLine = 0;
	int keyIndex = 0;		// keyIndex, because of differences in indexing between key array and indexing in config file
	// This is pretty awful indexing, but it is my bad, choosed bad format of config file (respectively that record key will be 0th index)
	for (currLine = 0; keyIndex < static_cast<int>(expectedLineCountInConfig); currLine++, keyIndex++) {			
		if (hasRecordKey) {
			keyIndex = static_cast<int>(currLine) - 1;			// -1 because the record file is at index 0
		}
		else {
			keyIndex = static_cast<int>(currLine);
		}
		if (!stream.is_open()) {
			initKeyWithDefaultAudioNonStatic(&this->keys[keyIndex]);
		}
		else {
			if (!std::getline(stream, line, '\n')) {			// If we reached the end of file, but there are still keys to initialize
				if (keyIndex >= 0) {
					initKeyWithDefaultAudioNonStatic(&this->keys[keyIndex]);
				}
			}
			else {
				std::stringstream ss(line);
				currentToken = 0;
				if ((firstSpaceInd = line.find(' ')) == std::string::npos) {
					initKeyWithDefaultAudioNonStatic(&this->keys[keyIndex]);
					keyNumber = keyNumber + 1;
					continue;					// I could use else, but continue does the same thing and doesn't pollute the code with more { }
				}
				while (std::getline(ss, token, delim)) {
					currentToken++;
					if (currentToken == 1) {
						try {
							lastKeyNumber = keyNumber;
							keyNumber = std::stoi(token, nullptr, 10);
						}
						catch (std::exception ex) {
							std::ofstream& logger = LoggerClass::getLogger();
							logger << "Problem with config file: Invalid key number in config file" << std::endl;
							return -1;
						}
						if (keyNumber < 0) {
							std::ofstream& logger = LoggerClass::getLogger();
							logger << "Problem with config file: Invalid key number (only >= 0 are supported)" << std::endl;
							return -1;
						}
						else if (keyNumber <= lastKeyNumber) {
							std::ofstream& logger = LoggerClass::getLogger();
							logger << "Problem with config file: Numbers aren't in sequential order." << std::endl;
							return -1;
						}
						else if (keyNumber > static_cast<int>(expectedLineCountInConfig)) {
							std::ofstream& logger = LoggerClass::getLogger();
							logger << "Problem with config file: Too many keys in file." << std::endl;
							return -1;
						}
						// If the first key was record key
						if (currLine == 0 && keyNumber == 0) {	
							hasRecordKey = true;
						}
						else {
							// -1 because there is difference in the indexing (In the file indexing from 1, in programming from 0)
							for (; keyIndex < keyNumber - 1; currLine++, keyIndex++) {			
								initKeyWithDefaultAudioNonStatic(&this->keys[keyIndex]);
							}
						}
					}
					else if (currentToken == 2) {
						if (currLine == 0 && hasRecordKey) {
							if (!setModifiersForKeyFromConfigFile(&this->recordKey, token, modDelim)) {
								return -1;
							}
						}
						else {
							if (!setModifiersForKeyFromConfigFile(&this->keys[keyIndex], token, modDelim)) {
								return -1;
							}
						}
					}
					else if (currentToken == 3) {
						if (currLine == 0 && hasRecordKey) {
							if (!setControlForKeyFromConfigFile(&this->recordKey, token)) {
								return -1;
							}
						}
						else {
							if (!setControlForKeyFromConfigFile(&this->keys[keyIndex], token)) {
								return -1;
							}
						}
					}
					else if (currentToken == 4) {
						if ( !(currLine == 0 && hasRecordKey)) {
							std::string rest;			// If there are some spaces in the name, we add them
							while (std::getline(ss, rest, '\n')) {
								token += " " + rest;
							}
							if (token == "DEFAULT") {
								// Other properties of key were initialized in the defaultInit method 
								initKeyWithDefaultAudioNonStatic(&this->keys[keyIndex]);
							}
							else {
								filename = token;
								char *c = &filename[0];
								if (! this->keys[keyIndex].setAudioBufferWithFile(c, &this->audioSpec)) {
									initKeyWithDefaultAudioNonStatic(&this->keys[keyIndex]);
								}
								c = nullptr;
							}
						}
					}
				}
			}
		}
	}
	
	if (stream.is_open()) {
		while (std::getline(stream, line))
		{
			currLine++;					// How many lines are there after the last processed line in file 
		}
	}

	*totalLinesInFile = currLine;
	drawWindow();

	if (currLine > expectedLineCountInConfig) {
		return 2;
	}
	else if (currLine < expectedLineCountInConfig) {
		return 1;
	}
	else {
		return 0;					// Everything OK
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Calculation help methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Keyboard::isWhiteKey(size_t ID) {
	if (ID > MAX_KEYS) {
		throw std::invalid_argument("Invalid key number");
	}


	if (ID % 12 == 4 || ID % 12 == 6 || ID % 12 == 9 || ID % 12 == 11 || ID % 12 == 1) {
		return false;				// black key
	}
	else {
		return true;				// white key
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Free methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Keyboard::freeTextures() {
	for (size_t i = 0; i < this->textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	this->textures.clear();
}


void Keyboard::freeKeys() {
	for (size_t i = 0; i < keys.size(); i++) {
		keys[i].freeKey();
	}
}


void Keyboard::freeKeyboard() {
	SDL_CloseAudioDevice(audioDevID);
	freeKeys();

	audioPlayingLabel.freeResources();
	configFileTextbox.freeResources();
	directoryWithFilesTextbox.freeResources();
	playFileTextbox.freeResources();
	recordFilePathTextbox.freeResources();

	keyPressedByMouse = nullptr;
	textboxWithFocus = nullptr;

	freeSDL_AudioCVTptr(&audioFromFileCVT);

	freeTextures();
}


void Keyboard::freeSDL_AudioCVTptr(SDL_AudioCVT **cvt) {
	if (*cvt != nullptr) {
		delete[](**cvt).buf;
		(**cvt).buf = nullptr;
		delete *cvt;
		*cvt = nullptr;
	}
}