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
#include <filesystem>

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


Keyboard::Keyboard(SDL_Window *window, SDL_Renderer *renderer) {
	std::ofstream& logger = LoggerClass::getLogger();
	keys = nullptr;

	audioFromFileCVT = nullptr;
	audioFromFileBufferIndex = 0;
	recordStartTime = 0;

	SDL_StopTextInput();
	this->window = window;
	this->renderer = renderer;
	SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);


	quit = false;
	isRecording = false;
	shouldRedrawTextboxes = false;
	shouldRedrawKeys = false;
	shouldRedrawKeyLabels = false;

	keyPressedByMouse = nullptr;
	recordStartTime = 0;
	keySetWindowTextColor = GlobalConstants::WHITE;
	textboxWithFocus = nullptr;

	if (window == nullptr) {
		throw std::invalid_argument("Keyboard couldn't be initalized because window does not exist.");
	}

	this->configFileTextbox = ConfigFileTextbox("TEXTBOX WITH PATH TO THE CONFIG FILE");
	this->directoryWithFilesTextbox = DirectoryWithFilesTextbox("TEXTBOX WITH PATH TO THE DIRECTORY CONTAINING FILES");
	this->recordFilePathTextbox = RecordFilePathTextbox("TEXTBOX WITH PATH TO THE RECORDED FILE");
	this->playFileTextbox = PlayFileTextbox("TEXTBOX WITH PATH TO THE FILE TO BE PLAYED");
	this->audioPlayingLabel = Label("");
	logger << "resizeTextboxes()...: ";
	resizeTextboxes();
	logger << "resizeTextboxes() done, SDL error: " << SDL_GetError() << std::endl;
	logger << "Initializing HW..." << std::endl;
	initAudioHW();
	logger << "HW initialized, SDL error: " << SDL_GetError() << std::endl;
	logger << "Calling initKeyboard()...";
	size_t tmp = 0;					// Not Ideal
	initKeyboard("", &tmp);
	logger << "initKeyboard() ended, SDL error was: " << SDL_GetError() << std::endl;
}


void Keyboard::setRenderer() {
	if (this->renderer == nullptr) {
		this->renderer = SDL_CreateRenderer(this->window, -1, SDL_RENDERER_ACCELERATED);
	}
}


void Keyboard::initAudioHW() {
	std::ofstream& logger = LoggerClass::getLogger();
	logger << "Setting audioSpec...";
	this->audioSpec = SDL_AudioSpec();

	audioSpec.freq = 22050;				// number of samples per second
	// Some formats are horrible, for example on my computer  AUDIO_U16SYS causes so much crackling it's unlistenable. Even after the remove of reduceCrackling
	audioSpec.format = AUDIO_S16SYS;		// sample type (here: signed short i.e. 16 bit)		// TODO: Later make user choose the format						
	audioSpec.channels = 1;
	audioSpec.samples = CALLBACK_SIZE;		// buffer-size
	audioSpec.userdata = this;				// counter, keeping track of current sample number
	audioSpec.callback = audio_callback;	// function SDL calls periodically to refill the buffer
	logger << "audioSpec set successfully" << std::endl;
	logger << "SDL_Error: " << SDL_GetError() << std::endl;

	logger << "Opening audio device...";
	this->audioDevID = SDL_OpenAudioDevice(nullptr, 0, &this->audioSpec, &this->audioSpec, 0);
	logger << "Audio device opened" << std::endl;
	logger << "SDL_Error: " << SDL_GetError() << std::endl;

	for (size_t i = 0; i < this->audioSpec.channels; i++) {
		currentlyUnpressedKeys.push_back(std::vector<Sint32>());
	}

	logger << "Audio device can start playing audio soon...";
	SDL_PauseAudioDevice(this->audioDevID, 0);			// Start playing the audio
	logger << "Callback function will be now regularly called" << std::endl;
	logger << "SDL_Error: " << SDL_GetError() << std::endl;
}

// Sets blackKeysCount and whiteKeysCount properties
constexpr void Keyboard::setBlackAndWhiteKeysCounts() {
	this->blackKeysCount = countBlackKeys(this->keyCount);
	this->whiteKeysCount = static_cast<int>(this->keyCount) - this->blackKeysCount;
}

// Counts number of black keys, if total number of keys on keyboard == keyCountLocal
constexpr size_t Keyboard::countBlackKeys(size_t keyCountLocal) {
	size_t blackKeysCountLocal = 5 * (keyCountLocal / 12);		// For every 13 keys 5 of them are black
	size_t mod = keyCountLocal % 12;
	//if (mod > 11) can't happen ... it's already covered in the division
	if (mod > 9)
		blackKeysCountLocal += 4;
	else if (mod > 6)
		blackKeysCountLocal += 3;
	else if (mod > 4)
		blackKeysCountLocal += 2;
	else if (mod > 1)
		blackKeysCountLocal += 1;

	return blackKeysCountLocal;
}


void Keyboard::generateTone(const SDL_AudioSpec *spec, int keyID, Uint32 numberOfSeconds, SDL_AudioCVT *keyCVT) {
	SDL_AudioSpec sourceSpec;
	sourceSpec.channels = 1;
	sourceSpec.format = 0b0000000000001000; //0b0000_0000_0000_1000 Unsigned and 1 byte sample size little endian
	sourceSpec.freq = spec->freq;
	sourceSpec.samples = spec->samples;
	sourceSpec.silence = 127;
	sourceSpec.size = sourceSpec.samples;

	int ret = SDL_BuildAudioCVT(keyCVT, sourceSpec.format, sourceSpec.channels, sourceSpec.freq, spec->format, spec->channels, spec->freq);

	Uint32 sourceSpecbyteSize = SDL_AUDIO_BITSIZE(sourceSpec.format) / 8;
	Uint32 sourceAudioByteSize = numberOfSeconds * sourceSpec.channels * sourceSpecbyteSize * static_cast<Uint32>(sourceSpec.freq);
	Uint32 bufferLen = keyCVT->len_mult * sourceAudioByteSize;
	Uint8 *buffer = new Uint8[bufferLen];

	double freq = 440 * pow(2, (keyID - 49) / static_cast<double>(12));			// Frequency of the tone in Hertz (Hz)
	double period = static_cast<double>(sourceSpec.freq) / freq;					// sample rate / frequency of the tone
	size_t j = 0;
	for (size_t i = 0; i < sourceAudioByteSize; i++) {
		double angle = 2.0 * M_PI * j / period;
		Uint8 byteSample = static_cast<Uint8>((sin(angle) * ((1 << 5) - 1)) + ((1 << 7) - 1));		// Changed 7 to 5 (Slightly helps the mixing)
		buffer[i] = byteSample;
		j++;
	}

// TODO: Was just experimenting with sound synthesis
/*
	bool up = true;
	int ind = 0;
	for (int i = 0; i < sourceAudioByteSize; i++) {
		if (up) {
			buffer[i] += ind;
			if (buffer[i] > 255) {
				buffer[i] = 255;
			}

			ind++;
			if (ind > 20) {
				ind = 255;
				up = !up;
			}
		}
		else {
			buffer[i] += ind;
			if (buffer[i] < 0) {
				buffer[i] = 0;
			}

			ind--;
			if (ind < 0) {
				ind = 0;
				up = !up;
			}
		}
	}
*/

	keyCVT->buf = buffer;
	keyCVT->len = sourceAudioByteSize;
	if (ret != 0) {
		convertAudioAndSaveMemory(keyCVT, bufferLen);
	}
}

// The converting usually needs more space than needed, so we just copy the buffer content to smaller buffer
void Keyboard::convertAudioAndSaveMemory(SDL_AudioCVT *audioCVT, Uint32 currentCVTBufferLen) {
	if (currentCVTBufferLen == 0 || audioCVT->buf == nullptr) {
		return;
	}

	SDL_ConvertAudio(audioCVT);
	// The converting usually needs more space than needed, so we just copy the buffer content to smaller buffer
	if (audioCVT->len_cvt != static_cast<int>(currentCVTBufferLen)) {				
		Uint8 *convertedBuffer = new Uint8[audioCVT->len_cvt];
		int i;
		for (i = 0; i < audioCVT->len_cvt; i++) {
			convertedBuffer[i] = audioCVT->buf[i];
		}

		delete[] audioCVT->buf;
		audioCVT->buf = convertedBuffer;
	}
	// Else the lengths are the same, no need to the anything
}

void Keyboard::setKeySoundsFromDirectory() {
	if (keys != nullptr) {
		size_t i = 0;
		for (const auto &element : std::experimental::filesystem::directory_iterator(this->directoryWithFilesTextbox.text)) {
			std::string path = element.path().string();
			size_t extensionIndex = path.size() - 4;
			if (extensionIndex > 0 && path.substr(extensionIndex) == ".wav") {
				keys[i].setAudioBufferWithFile(&path[0], &this->audioSpec);
				i++;
			}
		}
	}
}


void Keyboard::checkEvents(const SDL_Event &event) {
	bool enterPressed = false;
	bool shouldResizeTextboxes = false;

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
		std::ofstream& logger = LoggerClass::getLogger();
		std::cout << "Key Pressed" << std::endl;
		std::cout << event.key.keysym.mod << std::endl;
		std::cout << event.key.keysym.scancode << std::endl;
		std::cout << event.key.timestamp << std::endl;
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


constexpr void Keyboard::setKeySizes() {
	this->upperLeftCornerForKeysY = windowHeight / 3;
	this->whiteKeyWidth = windowWidth / this->whiteKeysCount;
	this->whiteKeyHeight = windowHeight / 4;
	this->blackKeyWidth = 3 * this->whiteKeyWidth / 4;
	this->blackKeyHeight = this->whiteKeyHeight / 2;
}


void Keyboard::resizeWindow(SDL_WindowEvent e) {
	windowWidth = e.data1;
	windowHeight = e.data2;

	resizeKeyboardKeys();
	resizeTextboxes();
}


void Keyboard::resizeTextboxes() {
	int textX = 0;
	int textY;
	int textW;
	int textH;

	int buttonX = 0;
	int buttonY;
	int buttonW = windowWidth;
	int buttonH = blackKeyHeight / 4;

	this->configFileTextbox.findFittingFont(&textW, &textH, buttonW, buttonH);
	textY = this->windowHeight - buttonH + (buttonH - textH) / 2;
	buttonY = windowHeight - buttonH;
	this->configFileTextbox.setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);

	this->directoryWithFilesTextbox.findFittingFont(&textW, &textH, buttonW, buttonH);
	textY = this->windowHeight - 2 * buttonH + (buttonH - textH) / 2;
	buttonY = this->windowHeight - 2 * buttonH;
	this->directoryWithFilesTextbox.setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);

	this->recordFilePathTextbox.findFittingFont(&textW, &textH, buttonW, buttonH);
	textY = this->windowHeight - 3 * buttonH + (buttonH - textH) / 2;
	buttonY = this->windowHeight - 3 * buttonH;
	this->recordFilePathTextbox.setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);

	this->playFileTextbox.findFittingFont(&textW, &textH, buttonW, buttonH);
	textY = this->windowHeight - 4 * buttonH + (buttonH - textH) / 2;
	buttonY = this->windowHeight - 4 * buttonH;
	this->playFileTextbox.setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);

	buttonX = this->windowWidth / 3;
	buttonW = this->windowWidth / 3;
	textX = this->windowWidth / 3;
	this->audioPlayingLabel.findFittingFont(&textW, &textH,buttonW, buttonH);
	textY = this->upperLeftCornerForKeysY / 2 + (buttonH - textH) / 2;
	buttonY = this->upperLeftCornerForKeysY / 2;
	this->audioPlayingLabel.setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);
}


constexpr void Keyboard::resizeKeyboardKeys() {
	setKeySizes();
	int currX = 0;
	for (size_t i = 0; i < keyCount; i++) {
		setKeyLocation(&currX, i);
	}

	this->recordKey.resizeButton(0, 0, this->whiteKeyWidth, 3 * this->blackKeyHeight / 4);
}


int Keyboard::initKeyboard(const std::string &configFilename, size_t *totalLinesInFile) {
	if (configFilename == "") {		
		return Keyboard::defaultInit(MAX_KEYS, true);
	}

	return this->readConfigfile(totalLinesInFile);
}


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
			initKeyWithDefaultAudio(&this->keys[keyIndex]);
		}
		else {
			if (!std::getline(stream, line, '\n')) {			// If we reached the end of file, but there are still keys to initialize
				if (keyIndex >= 0) {
					initKeyWithDefaultAudio(&this->keys[keyIndex]);
				}
			}
			else {
				std::stringstream ss(line);
				currentToken = 0;
				if ((firstSpaceInd = line.find(' ')) == std::string::npos) {
					initKeyWithDefaultAudio(&this->keys[keyIndex]);
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
								initKeyWithDefaultAudio(&this->keys[keyIndex]);
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
								initKeyWithDefaultAudio(&this->keys[keyIndex]);
							}
							else {
								filename = token;
								char *c = &filename[0];
								if (! this->keys[keyIndex].setAudioBufferWithFile(c, &this->audioSpec)) {
									initKeyWithDefaultAudio(&this->keys[keyIndex]);
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


bool Keyboard::setModifiersForKeyFromConfigFile(Key *key, const std::string &token, char modDelim) {
	std::string modifierToken;
	key->keysym.mod = SDL_Keymod::KMOD_NONE;
	if (token != "NOMOD") {
		std::stringstream ss(token);
		while (std::getline(ss, modifierToken, modDelim)) {
			if (modifierToken == "CTRL") {
				key->keysym.mod |= (SDL_Keymod::KMOD_LCTRL | SDL_Keymod::KMOD_RCTRL);
			}
			else if (modifierToken == "NUM") {
				key->keysym.mod |= SDL_Keymod::KMOD_NUM;
			}
			else if (modifierToken == "CAPS") {
				key->keysym.mod |= SDL_Keymod::KMOD_CAPS;
			}
			else if (modifierToken == "ALT") {
				key->keysym.mod |= (SDL_Keymod::KMOD_LALT | SDL_Keymod::KMOD_RALT);
			}
			else if (modifierToken == "SHIFT") {
				key->keysym.mod |= (KMOD_LSHIFT | KMOD_RSHIFT);
			}
			else {
				std::ofstream& logger = LoggerClass::getLogger();
				logger << "Invalid modifier while processing config file" << std::endl;
				return false;
			}
		}
	}
	return true;
}


bool Keyboard::setControlForKeyFromConfigFile(Key *key, const std::string &token) {
	char c = token[0];
	if (token.size() > 1 && (c == 'F' || c == 'f')) {				// Process F keys (F1, F2, ...)
		int fNum;
		try {
			fNum = std::stoi(token.substr(1, std::string::npos));
		}
		catch (std::exception ex) {
			std::ofstream& logger = LoggerClass::getLogger();
			logger << "Invalid control keys while processing config file: F key number isn't supported or it can't be converted to number" << std::endl;
			return false;
		}
		if (fNum <= 0 || fNum >= 25) {
			std::ofstream& logger = LoggerClass::getLogger();
			logger << "Invalid control keys while processing config file: F key number isn't supported" << std::endl;
			return false;
		}

		SDL_Scancode sc = static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + fNum - 1);
		key->keysym.sym = SDL_SCANCODE_TO_KEYCODE(sc);
		key->keysym.scancode = sc;
	}
	else if (token == "BACKSPACE") {
		key->keysym.sym = SDLK_BACKSPACE;
		key->keysym.scancode = SDL_SCANCODE_BACKSPACE;
	}
	else if (token == "SPACE") {
		key->keysym.sym = SDLK_SPACE;
		key->keysym.scancode = SDL_SCANCODE_SPACE;
	}
	else if (token == "ENTER") {
		key->keysym.sym = SDLK_RETURN;
		key->keysym.scancode = SDL_SCANCODE_RETURN;
	}
	else if (token == "ENTERKP") {
		key->keysym.sym = SDLK_KP_ENTER;
		key->keysym.scancode = SDL_SCANCODE_KP_ENTER;
	}
	else if (token == "UP") {
		key->keysym.sym = SDLK_UP;
		key->keysym.scancode = SDL_SCANCODE_UP;
	}
	else if (token == "DOWN") {
		key->keysym.sym = SDLK_DOWN;
		key->keysym.scancode = SDL_SCANCODE_DOWN;
	}
	else if (token == "LEFT") {
		key->keysym.sym = SDLK_LEFT;
		key->keysym.scancode = SDL_SCANCODE_LEFT;
	}
	else if (token == "RIGHT") {
		key->keysym.sym = SDLK_RIGHT;
		key->keysym.scancode = SDL_SCANCODE_RIGHT;
	}
	else if (token == "INSERT") {
		key->keysym.sym = SDLK_INSERT;
		key->keysym.scancode = SDL_SCANCODE_INSERT;
	}
	else if (token == "HOME") {
		key->keysym.sym = SDLK_HOME;
		key->keysym.scancode = SDL_SCANCODE_HOME;
	}
	else if (token == "PAGEUP") {
		key->keysym.sym = SDLK_PAGEUP;
		key->keysym.scancode = SDL_SCANCODE_PAGEUP;
	}
	else if (token == "DELETE") {
		key->keysym.sym = SDLK_DELETE;
		key->keysym.scancode = SDL_SCANCODE_DELETE;
	}
	else if (token == "END") {
		key->keysym.sym = SDLK_END;
		key->keysym.scancode = SDL_SCANCODE_END;
	}
	else if (token == "PAGEDOWN") {
		key->keysym.sym = SDLK_PAGEDOWN;
		key->keysym.scancode = SDL_SCANCODE_PAGEDOWN;
	}
	else if (c == ',') {
		key->keysym.sym = SDLK_COMMA;
		key->keysym.scancode = SDL_SCANCODE_COMMA;
	}
	else if (c == '.') {
		key->keysym.sym = SDLK_PERIOD;
		key->keysym.scancode = SDL_SCANCODE_PERIOD;
	}
	else if (c == '/') {
		key->keysym.sym = SDLK_SLASH;
		key->keysym.scancode = SDL_SCANCODE_SLASH;
	}
	else if (c == ';') {
		key->keysym.sym = SDLK_SEMICOLON;
		key->keysym.scancode = SDL_SCANCODE_SEMICOLON;
	}
	else if (c == '\'') {
		key->keysym.sym = SDLK_QUOTE;
		key->keysym.scancode = SDL_SCANCODE_APOSTROPHE;
	}
	else if (c == '\\') {
		key->keysym.sym = SDLK_BACKSLASH;
		key->keysym.scancode = SDL_SCANCODE_BACKSLASH;
	}
	else if (c == '[') {
		key->keysym.sym = SDLK_LEFTBRACKET;
		key->keysym.scancode = SDL_SCANCODE_LEFTBRACKET;
	}
	else if (c == ']') {
		key->keysym.sym = SDLK_RIGHTBRACKET;
		key->keysym.scancode = SDL_SCANCODE_RIGHTBRACKET;
	}
	else if (c == '`') {
		key->keysym.sym = SDLK_BACKQUOTE;
		key->keysym.scancode = SDL_SCANCODE_GRAVE;
	}
	else if (c == '-') {
		key->keysym.sym = SDLK_MINUS;
		key->keysym.scancode = SDL_SCANCODE_MINUS;
	}
	else if (c == '=') {
		key->keysym.sym = SDLK_EQUALS;
		key->keysym.scancode = SDL_SCANCODE_EQUALS;
	}
	else if (c == '/') {
		key->keysym.sym = SDLK_KP_DIVIDE;
		key->keysym.scancode = SDL_SCANCODE_KP_DIVIDE;
	}
	else if (c == '*') {
		key->keysym.sym = SDLK_KP_MULTIPLY;
		key->keysym.scancode = SDL_SCANCODE_KP_MULTIPLY;
	}
	else if (c == '-') {
		key->keysym.sym = SDLK_KP_MINUS;
		key->keysym.scancode = SDL_SCANCODE_KP_MINUS;
	}
	else if (c == '+') {
		key->keysym.sym = SDLK_KP_PLUS;
		key->keysym.scancode = SDL_SCANCODE_KP_PLUS;
	}
	else if (c == '.') {
		key->keysym.sym = SDLK_KP_PERIOD;
		key->keysym.scancode = SDL_SCANCODE_KP_PERIOD;
	}
	else if (c >= 'a' && c <= 'z') {
		key->keysym.sym = static_cast<SDL_Keycode>(c);
		key->keysym.scancode = static_cast<SDL_Scancode>(static_cast<int>(SDL_SCANCODE_A) + static_cast<int>(c - 97));
	}
	else if (c >= 'A' && c <= 'Z') {
		key->keysym.sym = static_cast<SDL_Keycode>(c + 32);
		key->keysym.scancode = static_cast<SDL_Scancode>(static_cast<int>(SDL_SCANCODE_A) + static_cast<int>(c - 65));
	}
	else if (c >= '1' && c <= '9') {
		if (token.substr(1, 2) == "KP") {
			SDL_Scancode sc = static_cast<SDL_Scancode>(static_cast<int>(SDL_SCANCODE_KP_1) + static_cast<int>(c - 49));
			key->keysym.sym = SDL_SCANCODE_TO_KEYCODE(sc);
			key->keysym.scancode = sc;
		}
		else {
			key->keysym.sym = static_cast<SDL_Scancode>(c);
			key->keysym.scancode = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + static_cast<int>(c - 49));
		}
	}
	else if (c == '0') {
		if (token.substr(1, 2) == "KP") {
			key->keysym.sym = SDLK_KP_0;
			key->keysym.scancode = SDL_SCANCODE_KP_0;
		}
		else {
			key->keysym.sym = SDLK_0;
			key->keysym.scancode = SDL_SCANCODE_0;
		}
	}

	return true;
}


constexpr void Keyboard::setKeyLocation(int *currX, size_t index) {
	if (index % 12 == 4 || index % 12 == 6 || index % 12 == 9 || index % 12 == 11 || index % 12 == 1) {			// Black keys
		this->keys[index].resizeButton(*currX - this->blackKeyWidth / 2, this->upperLeftCornerForKeysY, this->blackKeyWidth, this->blackKeyHeight);
	}
	else {
		this->keys[index].resizeButton(*currX, this->upperLeftCornerForKeysY, this->whiteKeyWidth, this->whiteKeyHeight);
		*currX = *currX + this->whiteKeyWidth;
	}
}


int Keyboard::defaultInit(size_t totalKeys, bool initAudioBuffer) {
	size_t currKey = 0;	
	int currX = 0;

	if (this->keyCount != totalKeys || this->keys == nullptr) {
		if (this->keys != nullptr) {
			freeKeys();
		}

		this->keys = new Key[totalKeys];
		for (size_t i = 0; i < totalKeys; i++) {
			keys[i].initKey();
		}
	}
	else {
		for (size_t i = 0; i < totalKeys; i++) {
			keys[i].freeKey();
		}
	}
	this->keyCount = totalKeys;
	SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);
	this->setBlackAndWhiteKeysCounts();
	this->setKeySizes();
	setRenderer();
	this->recordKey.resizeButton(0, 0, this->whiteKeyWidth, 3 * this->blackKeyHeight / 4);
	this->recordKey.ID = 0;
	this->resizeTextboxes();

	size_t li = this->keyCount - 1;		// Last index
	if (isWhiteKey(li)) {				// If the last key is black then the boundary is a bit different
		this->isLastKeyBlack = false;
	}
	else {
		this->isLastKeyBlack = true;
	}

	for (currKey = 0; currKey < totalKeys; currKey++) {
		setKeyLocation(&currX, currKey);

		this->keys[currKey].isDefaultSound = initAudioBuffer;
		this->keys[currKey].ID = currKey;
		this->keys[currKey].pressCount = 0;
		defaultInitControlKeys(currKey);	

		// Frequency is the same as rate - samples per second
		int ret = SDL_BuildAudioCVT(&this->keys[currKey].audioConvert, this->audioSpec.format, this->audioSpec.channels, this->audioSpec.freq,
			this->audioSpec.format, this->audioSpec.channels, this->audioSpec.freq);		

		this->keys[currKey].audioConvert.buf = nullptr;

		if (ret != 0) {
			throw std::exception("Same formats aren't the same ... Shouldn't happen");
		}
		if (initAudioBuffer) {	
			initKeyWithDefaultAudio(&this->keys[currKey]);
		}
	}

	drawWindow();
	return 0;
}

void Keyboard::initKeyWithDefaultAudio(Key *key) {
	constexpr Uint32 numberOfSeconds = 10;
	if (key != nullptr) {
		key->freeKey();
		generateTone(&this->audioSpec, static_cast<int>(key->ID), numberOfSeconds, &key->audioConvert);
	}
}


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
		for (size_t i = 0; i < this->keyCount; i++) {
			key = &this->keys[i];
			// It is exactly the mod, or maybe it is using just one of the possible mods (for example 1 of the 2 shift keys)
			if (key->keysym.sym == keyEvent.keysym.sym && (key->keysym.mod == keyEvent.keysym.mod || checkForMods(key->keysym.mod, keyEvent.keysym.mod) )) {	
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
		((KMOD_CTRL & keyEventMod) != KMOD_NONE	 ||	mod1 == KMOD_NONE) &&
		((KMOD_SHIFT & keyEventMod) != KMOD_NONE || mod2 == KMOD_NONE) &&
		((KMOD_ALT & keyEventMod) != KMOD_NONE	 ||	mod3 == KMOD_NONE) &&
		(mod1 != 0 || mod2 != 0 || mod3 != 0));
}


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
		this->textboxWithFocus = &clickedTextbox;		// TODO: Not sure
	}
	else {
		this->textboxWithFocus = nullptr;
	}

	while (this->currentlyPressedKeys.size() > 0) {
		keyUnpressAction(this->currentlyPressedKeys[0], event.timestamp, 0);
	}

	SDL_StartTextInput();
}


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
		// TODO: podle me zbytecny kdyz to je hasFocus == true aspon u jednoho tak textboxWithFocus != nullptr
		//if (this->configFileTextbox->hasFocus || this->directoryWithFilesTextbox->hasFocus || this->playFileTextbox->hasFocus || this->recordFilePathTextbox->hasFocus) {
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
	if (event.x >= this->whiteKeysCount * this->keys[0].rectangle.w) {		// It is behind the last white key
		if (this->isLastKeyBlack) {											// If the black key is last then it has different borders
			if (event.x >= this->keys[keyCount - 1].rectangle.x + this->keys[keyCount - 1].rectangle.w || // It is behind the last black key
				event.y >= this->keys[keyCount - 1].rectangle.y + this->keys[keyCount - 1].rectangle.h) { // It is under the last black key
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
		throw std::exception("Mouse button pressed twice error without unpressing");					// Shouldn't happen
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
		index < keyCount - 1 &&											// We won't reach behind the keyboard
		y <= (this->blackKeyHeight + this->upperLeftCornerForKeysY) &&	// It is in the black key with y coordinate
		x >= (nextKeyStartX - this->blackKeyWidth / 2) &&				//							   x
		x <= (nextKeyStartX + this->blackKeyWidth / 2))					//                             x
	{
		return &this->keys[index + 1];		// It was the next black key
	}
	else if ((index != 0 || mod != 0) && mod != 2 && mod != 5 &&		// Before these white keys there isn't any black key 
		index <= keyCount &&											// It isn't after the keyboard
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
		throw std::exception("Button unpressed more times than it was pressed");					// Shouldn't happen
	}

	addToRecordedKeys(key, timestamp, KeyEventType::KEY_RELEASED);
}

void Keyboard::addToRecordedKeys(Key *key, Uint32 timestamp, KeyEventType keyET) {
	if (isRecording) {
		timestamp -= this->recordStartTime;
		TimestampAndID tsid(timestamp, key->ID + 1, keyET);
		this->recordedKeys.push_back(tsid);
	}
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


void Keyboard::playPressedKeysCallback(Uint8 *bufferToBePlayed, int bytes) {
#if DEBUG
	std::ofstream& logger = LoggerClass::getLogger();
	logger << "In playPressedKeysCallback()..." << std::endl;
#endif
	Key *key;
	if (this->currentlyPressedKeys.size() == 0) {
#if DEBUG
		logger << "Filling buffer with silence..." << std::endl;
#endif
		for (int i = 0; i < bytes; i++) {
			bufferToBePlayed[i] = this->audioSpec.silence;
		}
#if DEBUG
		logger << "Buffer filled" << std::endl;
#endif
		// TODO: What is really interesting is, that if I add this part of code, there is insane amount of crackling. Function
		// is done much faster than the variant without this part, and the else part is also much slower, so maybe it is because the buffer is filled too fast ?? 
		// I don't know if it is possible, that this is the reason of the crackling ???????
		//if (this->audioFromFileCVT == nullptr) {
		//	return;
		//}
	}
	else {
#if DEBUG
		logger << "Filling buffer with key sounds..." << std::endl;
#endif
		key = this->currentlyPressedKeys[0];
		int lastIndex = key->startOfCurrAudio + bytes;
		int i;

		if (key == nullptr || key->audioConvert.buf == nullptr) {
			for (i = 0; i < bytes; i++) {
				bufferToBePlayed[i] = this->audioSpec.silence;
			}
		}
		else {
			// If the audio buffer to be played needs more data than there is in the buffer of the key, then the rest will be silence
			if (lastIndex > key->audioConvert.len_cvt) {
				i = 0;
				for (; key->startOfCurrAudio < key->audioConvert.len_cvt; key->startOfCurrAudio++) {
					bufferToBePlayed[i] = key->audioConvert.buf[key->startOfCurrAudio];
					i++;
				}
				for (; i < bytes; i++) {
					bufferToBePlayed[i] = this->audioSpec.silence;
				}
			}
			else {
				for (i = 0; i < bytes; i++, key->startOfCurrAudio++) {
					bufferToBePlayed[i] = key->audioConvert.buf[key->startOfCurrAudio];
				}
			}
		}
		for (size_t j = 1; j < this->currentlyPressedKeys.size(); j++) {			// Using size_t j instead of i because of warning
			if (key != nullptr && key->audioConvert.buf != nullptr) {
				key = this->currentlyPressedKeys[j];
				lastIndex = key->startOfCurrAudio + bytes;
				int len;
				if (lastIndex > key->audioConvert.len_cvt) {
					len = (key->audioConvert.len_cvt - key->startOfCurrAudio);
					lastIndex = key->startOfCurrAudio + len;
				}
				else {
					len = bytes;
				}

				SDL_MixAudioFormat(bufferToBePlayed, &key->audioConvert.buf[key->startOfCurrAudio], this->audioSpec.format, len, SDL_MIX_MAXVOLUME);
				key->startOfCurrAudio = lastIndex;
			}
		}


/*
// TODO: Later make the default sound in such way, that it can be played infinitely, the simple way of just starting reading the buffer from start again doesn't work (there is skip)
		for (int i = 0; i < this->currentlyPressedKeys.size(); i++) {
			if (this->currentlyPressedKeys[i]->isDefaultSound) {		// If default we reset the index to start, so it can play continous tone even in the next call

			}
		}
*/
	}

	// If wav file is played
	if (this->audioFromFileCVT != nullptr) {
		int len;
		int lastIndex = audioFromFileBufferIndex + bytes;
		if (lastIndex > this->audioFromFileCVT->len_cvt) {
			len = (this->audioFromFileCVT->len_cvt - audioFromFileBufferIndex);
		}
		else {
			len = bytes;
		}

		SDL_MixAudioFormat(bufferToBePlayed, &this->audioFromFileCVT->buf[audioFromFileBufferIndex], this->audioSpec.format, len, SDL_MIX_MAXVOLUME);

		audioFromFileBufferIndex += len;
		if (len < bytes) {
			freeSDL_AudioCVTptr(&this->audioFromFileCVT);
			this->audioFromFileBufferIndex = 0;

			this->audioPlayingLabel.hasFocus = false;
			audioPlayingLabel.text = "";
			audioPlayingLabel.drawTextWithBackground(this->renderer, GlobalConstants::BACKGROUND_BLUE, GlobalConstants::BACKGROUND_BLUE, GlobalConstants::BACKGROUND_BLUE);
			SDL_RenderPresent(this->renderer);
		}
	}


	reduceCrackling(bufferToBePlayed, bytes);
	if (this->isRecording) {
		this->record.insert(this->record.end(), &bufferToBePlayed[0], &bufferToBePlayed[bytes]);
	}
}


void Keyboard::reduceCrackling(Uint8 *bufferToBePlayed, int bytes) {
	if (bufferOfCallbackSize == nullptr) {
		bufferOfCallbackSize = new Uint8[bytes];
	}

	if (this->currentlyUnpressedKeys[0].size() > 0) {
		// this->currentlyUnpressedKeys.size() == this->audioSpec.channels ... keep that in mind
		int byteSize = SDL_AUDIO_BITSIZE(this->audioSpec.format) / 8;
		Sint32 *vals = new Sint32[this->audioSpec.channels];			// Each channel has the same number of samples, so we can just take [0] index
		Sint32 *factors = new Sint32[this->audioSpec.channels];
		Sint32 *mods = new Sint32[this->audioSpec.channels];

		// The number we will decrement from the mods to get 0. It is +1 if the mod is positive, is -1 if negative. And we will also decrement it from the value
		// We are just removing the excess, which couldn't be covered by the factors
		Sint32 *modDecrementer = new Sint32[this->audioSpec.channels];		

		Sint32 sampleCount = bytes / (this->audioSpec.channels * byteSize);			// Number of samples (1 sample = 1 sample for 1 channel)
		Sint32 sampleMod = bytes % (this->audioSpec.channels * byteSize);				// Remaining bytes
		

		for (size_t i = 0; i < this->currentlyUnpressedKeys[0].size(); i++) {
			int index = 0;
			for (size_t j = 0; j < this->audioSpec.channels; j++) {
				vals[j] = this->currentlyUnpressedKeys[j][i];
				factors[j] = (vals[j] - this->audioSpec.silence) / sampleCount;
				mods[j] = (vals[j] - this->audioSpec.silence) % sampleCount;

				if (mods[j] > 0) {
					modDecrementer[j] = 1;
				}
				else {
					modDecrementer[j] = -1;
				}
			}

			Sint32 val;
			for (int j = 0; j < sampleCount; j++) {
				for (size_t ch = 0; ch < this->audioSpec.channels; ch++) {
					val = vals[ch];
					switch (this->audioSpec.format) {
					// These action work, because the Sint32 is little endian
					case AUDIO_U8:
					{			// I need to define the scope, so the jump to this address will have initialized local variable, else C2360 error
						Uint8 *p = static_cast<Uint8 *>(&bufferOfCallbackSize[index]);
						*p = static_cast<Uint8>(vals[ch]);
						break;
					}
					case AUDIO_S8:
					{
						Sint8 *p = reinterpret_cast<Sint8 *>(&bufferOfCallbackSize[index]);
						*p = static_cast<Sint8>(vals[ch]);
						break;
					}
					case AUDIO_U16SYS:
					{
						Uint16 *p = reinterpret_cast<Uint16 *>(&bufferOfCallbackSize[index]);
						*p = static_cast<Uint16>(vals[ch]);
						break;
					}
					case AUDIO_S16SYS:
					{
						Sint16 *p = reinterpret_cast<Sint16 *>(&bufferOfCallbackSize[index]);
						*p = static_cast<Sint16>(vals[ch]);
						break;
					}
					case AUDIO_S32SYS:
					{
						Sint32 *p = reinterpret_cast<Sint32 *>(&bufferOfCallbackSize[index]);
						*p = vals[ch];
						break;
					}
					case AUDIO_F32SYS:			// In this case we just skip it, it takes too much work to work with float
					default:
						break;
					}

 					index += byteSize;
					vals[ch] -= factors[ch];
		
					if (mods[ch] != 0) {
						vals[ch] -= modDecrementer[ch];
						mods[ch] -= modDecrementer[ch];
					}
				}
			}
			if (sampleMod > 0) {
				for (int j = bytes - sampleMod; j < bytes; j++) {
					bufferOfCallbackSize[j] = this->audioSpec.silence;
				}
			}
			SDL_MixAudioFormat(bufferToBePlayed, bufferOfCallbackSize, this->audioSpec.format, bytes, SDL_MIX_MAXVOLUME);
		}

		// Free arrays (since the arrays are really small, it would be better to have them allocated on stack, but arr[x] works only if x is constant expression)
		delete[] factors;
		delete[] vals;
		delete[] mods;
		delete[] modDecrementer;
		for (size_t i = 0; i < this->currentlyUnpressedKeys.size(); i++) {
			this->currentlyUnpressedKeys[i].clear();
		}
	}
}



void Keyboard::drawWindow() {
	drawBackground();
	drawTextboxes();
	drawKeys();
	drawKeyLabels();

	SDL_RenderPresent(renderer);
}


// Also clears the renderer
void Keyboard::drawBackground() {
	// background will be rendered in this color
	SDL_SetRenderDrawColor(renderer, 0, 0, 100, 255);
	// Clear window
	SDL_RenderClear(renderer);
}


void Keyboard::drawTextboxes() {
	playFileTextbox.drawTextbox(this->renderer);
	configFileTextbox.drawTextbox(this->renderer);
	directoryWithFilesTextbox.drawTextbox(this->renderer);
	recordFilePathTextbox.drawTextbox(this->renderer);

	audioPlayingLabel.drawTextWithBackground(this->renderer, GlobalConstants::RED, GlobalConstants::BACKGROUND_BLUE, GlobalConstants::BACKGROUND_BLUE);
}


void Keyboard::drawKeys() {
	if (isRecording) {
		SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);							// Red colour
		SDL_RenderFillRect(renderer, &this->recordKey.rectangle);
	}
	else {
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);						// White colour
		SDL_RenderFillRect(renderer, &this->recordKey.rectangle);
	}
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);						// Draw black key borders
	SDL_RenderDrawRect(renderer, &this->recordKey.rectangle);


	// When we first draw rectangle and then fill it then the borders won't be drawn
	// First draw the black keys (in this for cycle) of keyboard and then draw the white keys (in the 2nd for cycle) on top of them
	for (size_t i = 0; i < this->keyCount; i++) {
		if (isWhiteKey(i)) {						// White keys
			if (keys[i].pressCount == 0) {			// It isn't pressed
				SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);			// White color
				SDL_RenderFillRect(renderer, &this->keys[i].rectangle);
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);					// Black color
				SDL_RenderDrawRect(renderer, &this->keys[i].rectangle);
			}
			else {									// It is pressed key
				SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);				// Red color
				SDL_RenderFillRect(renderer, &this->keys[i].rectangle);
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);					// Black color
				SDL_RenderDrawRect(renderer, &this->keys[i].rectangle);
			}
		}
	}
	for (size_t i = 0; i < this->keyCount; i++) {		// Draw black keys
		if (!isWhiteKey(i)) {						// Black keys
			if (keys[i].pressCount == 0) {			// It isn't pressed key
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);						// Black color
				SDL_RenderFillRect(renderer, &this->keys[i].rectangle);
				SDL_RenderDrawRect(renderer, &this->keys[i].rectangle);
			}
			else {									// It is pressed key
				SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);					// Red color
				SDL_RenderFillRect(renderer, &this->keys[i].rectangle);
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);						// Black color
				SDL_RenderDrawRect(renderer, &this->keys[i].rectangle);
			}
		}
	}
}


void Keyboard::startRecording(Uint32 timestamp) {
	this->isRecording = true;
	this->recordStartTime = timestamp;
	this->record.clear();
	this->recordedKeys.clear();
}


void Keyboard::endRecording() {
	this->isRecording = false;
	createWavFileFromAudioBuffer(this->recordFilePathTextbox.text);
	createKeyFileFromRecordedKeys(this->recordFilePathTextbox.text);
	this->record.clear();
	this->recordedKeys.clear();
}


// This code about creating wav file is modified example of this http://www.cplusplus.com/forum/beginner/166954/
template <typename Word>
std::ostream& write_word(std::ostream& outs, Word value, unsigned size = sizeof(Word))
{
	for (; size; --size, value >>= 8)
		outs.put(static_cast<char>(value & 0xFF));
	return outs;
}

// Example for reading wav file http://www.cplusplus.com/forum/beginner/166954/
// Wav file format reference page http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
void Keyboard::createWavFile(const std::string &path, std::vector<Uint8> audioSamples) {
	std::ofstream f(path + ".wav", std::ios::binary);

	// Write the file headers
	f << "RIFF----WAVEfmt ";						// (chunk size to be filled in later)
	write_word(f, 16, 4);							// no extension data ... chunk size
	write_word(f, 1, 2);							// PCM format is under number 1 - integer samples
	write_word(f, this->audioSpec.channels, 2);	// number of channels (stereo file)
	write_word(f, this->audioSpec.freq, 4);		// samples per second (Hz)

	// Byte size of one second is calculated as (Sample Rate * BitsPerSample * Channels) / 8
	int byteSizeOfOneSecond = (this->audioSpec.freq *  SDL_AUDIO_BITSIZE(this->audioSpec.format) * this->audioSpec.channels) / 8;
	write_word(f, byteSizeOfOneSecond, 4);
	size_t frameLengthInBytes = SDL_AUDIO_BITSIZE(this->audioSpec.format) / 8 * this->audioSpec.channels;
	write_word(f, frameLengthInBytes, 2);  // data block size ... size of audio frame in bytes
	size_t sampleSizeInBits = SDL_AUDIO_BITSIZE(this->audioSpec.format);
	write_word(f, sampleSizeInBits, 2);  // number of bits per sample (use a multiple of 8)

	f << "data";
	bool needsPadding = false;
	// Write size of data in bytes
	if (audioSamples.size() % 2 == 1) {
		needsPadding = true;
		write_word(f, audioSamples.size() + 1, 4);			
	}
	else {
		write_word(f, audioSamples.size(), 4);
	}

	// Now write all samples to the file, in little endian format
	size_t sampleSizeInBytes = sampleSizeInBits / 8;
	size_t i = 0;
	while (i < audioSamples.size()) {
		for (size_t j = 0; j < this->audioSpec.channels; j++) {
			size_t sampleIndex = i;
			for (size_t k = 0; k < sampleSizeInBytes; k++) {
				if (SDL_AUDIO_ISBIGENDIAN(this->audioSpec.format)) {		// Big endian, it is needed to write bytes in opposite direction
					f << audioSamples[sampleIndex + sampleSizeInBytes - k - 1];
				}
				else {
					f << audioSamples[i];
				}

				i++;
			}
		}
	}

	if (needsPadding) {	// If odd number of bytes in chunk then add padding byte
		f << 0x00;
	}

	// (We'll need the final file size to fix the chunk sizes above)
	std::streamoff file_length = f.tellp();

	// Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
	f.seekp(0 + 4);
	write_word(f, file_length - 8, 4);
}


void Keyboard::createKeyFile(const std::string &path, std::vector<TimestampAndID> &keysForKeyFile) {
	if (keysForKeyFile.size() > 0) {
		std::ofstream f;
		std::string s = path + ".keys";
		f.open(s);
		for (size_t i = 0; i < keysForKeyFile.size() - 1; i++) {
			f << keysForKeyFile[i].timestamp << " " << keysForKeyFile[i].ID << " " << keysForKeyFile[i].keyEventType << std::endl;
		}
		f << keysForKeyFile[keysForKeyFile.size() - 1].timestamp << " " << keysForKeyFile[keysForKeyFile.size() - 1].ID << " " << keysForKeyFile[keysForKeyFile.size() - 1].keyEventType;
		f.close();
	}
}


void Keyboard::playFile(const std::string &path) {
	// Based on extension call the right method
	size_t index = path.find_last_of('.');
	if (index == std::string::npos) {
		return;
	}
	std::string extension = path.substr(index + 1, std::string::npos);
	if (extension == "wav") {
		playWavFile(path);
	}
	else if (extension == "keys") {
		playKeyFile(path);
	}
// TODO: I don't do anything, it's better when the program doesn't crash when there is some bad extension by mistake
/*
	else if (extension == "mp3") {				// TODO: The exceptions aren't ideal solution, but is ok for now.
		throw std::invalid_argument("Mp3 file format isn't supported. At least for now");
	}
	else {
		throw std::invalid_argument("Unknown file format.");
	}
*/
}

void Keyboard::playWavFile(const std::string &path) {
	SDL_AudioSpec *loadWAVResult;
	SDL_AudioSpec spec;
	Uint8 *buf = nullptr;
	Uint32 len;

	loadWAVResult = SDL_LoadWAV(&path[0], &spec, &buf, &len);
	if (loadWAVResult == nullptr) {
		SDL_FreeWAV(buf);
		return;
	}

	SDL_AudioCVT *cvt = new SDL_AudioCVT();
	convert(cvt, &spec, &this->audioSpec, buf, len);
	this->audioFromFileCVT = cvt;
	this->audioFromFileBufferIndex = 0;

	int sizeOfOneSec = this->audioSpec.freq * this->audioSpec.channels * SDL_AUDIO_BITSIZE(this->audioSpec.format) / 8;
	int audioLenInSecs = audioFromFileCVT->len_cvt / sizeOfOneSec;
	this->audioPlayingLabel.text = "AUDIO OF LENGTH " + std::to_string(audioLenInSecs) +" SECONDS IS BEING PLAYED";
	this->audioPlayingLabel.hasFocus = true;
	this->resizeTextboxes();
	this->drawTextboxes();

	SDL_FreeWAV(buf);
}

// Should be called with keyboard->audioSpec (this->audioSpec) as desiredSpec
void Keyboard::convert(SDL_AudioCVT *cvt, SDL_AudioSpec *spec, SDL_AudioSpec *desiredSpec, Uint8 *buffer, Uint32 len) {
	int ret = SDL_BuildAudioCVT(cvt, spec->format, spec->channels, spec->freq, desiredSpec->format,	desiredSpec->channels, desiredSpec->freq);

	cvt->len = len;
	Uint32 convertBufferLen = cvt->len * cvt->len_mult;
	cvt->buf = new Uint8[convertBufferLen];

	for (size_t i = 0; i < len; i++) {
		cvt->buf[i] = buffer[i];
	}
	for (size_t i = len; i < convertBufferLen; i++) {
		cvt->buf[i] = spec->silence;						
	}

	if (ret != 0) {
		convertAudioAndSaveMemory(cvt, convertBufferLen);
	}
	else {		// No conversion needed
		cvt->len_cvt = cvt->len;
	}

}


void Keyboard::playKeyFile(const std::string &path) {
	std::ofstream& logger = LoggerClass::getLogger();

	std::ifstream ifs;
	ifs.open(path);
	std::string line;
	std::string token;
	SDL_KeyboardEvent keyboardEvent;
	keyboardEvent.repeat = 0;
	Uint32 timestamp = 0;
	int keyID = 0;
	KeyEventType keyET;
	Uint32 uintET;
	char delim = ' ';
	size_t currToken = 0;
	std::vector<SDL_KeyboardEvent> events;

	bool isForCycleHeader = false;
	bool isForCycle = false;

	std::vector<std::tuple<Uint32, Uint32, Uint32, std::vector<SDL_KeyboardEvent>>> forCycles;
	Uint32 forCycleStartTime = 0;
	Uint32 rowCountInForCycle = 0;
	size_t forCycleCurrLine = 0;
	Uint32 forCycleCycleCount = 0;
	Uint32 forCycleWaitTime = 0;

	if (ifs.is_open()) {
		while (std::getline(ifs, line, '\n')) {			// For every line
			std::stringstream ss(line);
			currToken = 0;
			while (std::getline(ss, token, delim)) {	// Parse line
				if (currToken == 0) {
					if (token == "for") {
						isForCycleHeader = true;
					}
					else {
						isForCycleHeader = false;
					}
				}
				if (isForCycleHeader) {
					// TODO: Maybe later % 5, If I'd like to reuse the for cycles (call the multiple times with different parameters ... I would just have some label)
					if (currToken % 5 == 1) {
						try {
							forCycleStartTime = std::stoul(token, nullptr, 10);						
						}
						catch (std::exception e) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Invalid start time of for cycle in keys file" << std::endl;
							ifs.close();
							return;
						}
					}
					else if (currToken % 5 == 2) {
						rowCountInForCycle = std::stoul(token, nullptr, 10);
					}
					else if (currToken % 5 == 3) {
						try {
							forCycleCycleCount = std::stoul(token, nullptr, 10);
						}
						catch (std::exception e) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Invalid number of cycles of for cycle in keys file" << std::endl;
							ifs.close();
							return;
						}
					}
					else if (currToken % 5 == 4) {
						try {
							forCycleWaitTime = std::stoul(token, nullptr, 10);
							forCycles.push_back(std::tuple<Uint32, Uint32, Uint32, std::vector<SDL_KeyboardEvent>>(forCycleStartTime, 
								forCycleCycleCount, forCycleWaitTime, std::vector<SDL_KeyboardEvent>()));
						}
						catch (std::exception e) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Invalid number of cycles of for cycle in keys file" << std::endl;
							ifs.close();
							return;
						}
					}
				}
				else {
					if (currToken % 3 == 0) {
						try {
							timestamp = std::stoul(token, nullptr, 10);
						}
						catch (std::exception e) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Invalid timestamp in keys file" << std::endl;
							ifs.close();
							return;
						}
					}
					else if (currToken % 3 == 1) {
						try {
							keyID = std::stoi(token, nullptr, 10);
							keyID = keyID - 1;			// Because in the file are indexes from 1, but in this program we index from 0.
						}
						catch (std::exception e) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Couldn't parse key number in keys file" << std::endl;
							ifs.close();
							return;
						}
						if (keyID >= static_cast<int>(this->keyCount) || keyID < 0) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Invalid number of key in keys file: " << keyID << std::endl;
							ifs.close();
							return;
						}
					}
					else if (currToken % 3 == 2) {
						try {
							uintET = std::stoul(token, nullptr, 10);						
						}
						catch (std::exception e) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Couldn't parse event type" << std::endl;
							ifs.close();
							return;
						}
						if (uintET > 2) {
							logger << "ERROR WHILE PARSING .KEYS FILE: Invalid event" << std::endl;
							ifs.close();
							return;
						}

						keyET = static_cast<KeyEventType>(uintET);
						switch (keyET) {
						case KEY_PRESSED:
							keyboardEvent.state = SDL_PRESSED;
							keyboardEvent.type = SDL_KEYDOWN;
							break;
						case KEY_RELEASED:
							keyboardEvent.state = SDL_RELEASED;
							keyboardEvent.type = SDL_KEYUP;
							break;
						default:
							ifs.close();
							logger << "ERROR WHILE PARSING .KEYS FILE: Unknown event, shouldn't happen ... throwing exception" << std::endl;
							throw std::invalid_argument("Unknown event, shouldn't happen.");		// throw exception, because this is something wrong with program			
						}
					}
				}

				currToken++;
			}

			if (!isForCycleHeader) {			
				keyboardEvent.timestamp = timestamp;
				keyboardEvent.keysym = this->keys[keyID].keysym;
				if (isForCycle) {					
					if (forCycleCurrLine >= rowCountInForCycle) {
						isForCycle = false;
						events.push_back(keyboardEvent);
					}
					else {
						std::get<3>(forCycles.back()).push_back(keyboardEvent);
						forCycleCurrLine++;
					}					
				}
				else {
					events.push_back(keyboardEvent);
				}
			}
			else {
				isForCycle = true;
				isForCycleHeader = false;
				forCycleCurrLine = 0;
			}
		}
	}

	
	addForCyclesToEvents(forCycles, events);
	std::sort(events.begin(), events.end(), SDL_KeyboardEventComparator);			// TODO: Nejsem si jisty jestli tu ma byt SDL_KeyboardEventComparator nebo &SDL_KeyboardEventComparator
	logger << ".KEYS FILE: event count: " << events.size() << std::endl;
#if DEBUG 
	for (size_t i = 0; i < events.size(); i++) {
		logger << events[i].timestamp << "\t" << events[i].keysym.scancode << "\t" << events[i].state << std::endl;
	}
#endif
	if (events.size() > 0) {
		int audioLenInSecs = events.back().timestamp / 1000;
		this->audioPlayingLabel.text = "AUDIO OF LENGTH " + std::to_string(audioLenInSecs) + " SECONDS IS BEING PLAYED";
		this->audioPlayingLabel.hasFocus = true;
		this->resizeTextboxes();
		drawWindow();

		// Now play the key file, just fire the events at given timestamp
		clock_t time;
		SDL_Event eventFromUser;

		for (size_t i = 0; i < events.size(); i++) {
			events[i].timestamp = CLOCKS_PER_SEC * events[i].timestamp / 1000;		// Convert it to the time we get from clock()
		}

		time = clock();
		clock_t startTime = time;
		clock_t eventTime;
		for (size_t i = 0; i < events.size(); i++) {
// TODO: Version with passive waiting (but the version with active one is better since it also accepts input from user)
/*
					std::this_thread::sleep_for(std::chrono::milliseconds(events[i].timestamp - lastTimestamp));
					lastTimestamp = events[i].timestamp;
*/

			events[i].timestamp += startTime;
			eventTime = events[i].timestamp;
			// Version with active waiting
			while (time < eventTime) {
				if (SDL_PollEvent(&eventFromUser)) {
					checkEvents(eventFromUser);
					if (this->quit) {
						ifs.close();
						return;
					}
				}

				time = clock();
			}

			this->findKeyAndPerformAction(events[i]);
			drawKeys();
			SDL_RenderPresent(this->renderer);
		}

		this->audioPlayingLabel.hasFocus = false;
		audioPlayingLabel.text = "";
		audioPlayingLabel.drawTextWithBackground(this->renderer, GlobalConstants::RED, GlobalConstants::BACKGROUND_BLUE, GlobalConstants::BACKGROUND_BLUE);
		SDL_RenderPresent(this->renderer);
	}

	ifs.close();
}


bool Keyboard::SDL_KeyboardEventComparator(const SDL_KeyboardEvent& a, const SDL_KeyboardEvent& b) {
	return (a.timestamp < b.timestamp);
}


void Keyboard::addForCyclesToEvents(std::vector<std::tuple<Uint32, Uint32, Uint32, std::vector<SDL_KeyboardEvent>>> forCycles, std::vector<SDL_KeyboardEvent> &events) {
	Uint32 forCycleCycleCount;
	Uint32 currTime;
	Uint32 jumpTime;
	SDL_KeyboardEvent keyboardEvent;
	keyboardEvent.repeat = 0;
	for (std::vector<std::tuple<Uint32, Uint32, Uint32, std::vector<SDL_KeyboardEvent>>>::iterator iter = forCycles.begin(); iter != forCycles.end(); iter++) {
		currTime = std::get<0>(*iter);
		forCycleCycleCount = std::get<1>(*iter);
		jumpTime = std::get<2>(*iter);
#if DEBUG
		std::ofstream& logger = LoggerClass::getLogger();
		logger << "Iteration, for cycle of size:" << std::get<3>(*iter).size() << " Will be repeated: " << forCycleCycleCount << std::endl;
#endif

		for (size_t i = 0; i < forCycleCycleCount; i++, currTime += jumpTime) {
#if DEBUG
			logger << "Cycle: " << i << std::endl;
#endif
			for (std::vector<SDL_KeyboardEvent>::iterator iterator = std::get<3>(*iter).begin(); iterator != std::get<3>(*iter).end(); iterator++) {
				keyboardEvent.timestamp = currTime + iterator->timestamp;
				keyboardEvent.state = iterator->state;
				keyboardEvent.type = iterator->type;
#if DEBUG
				logger << "TIMESTAMP: " << keyboardEvent.timestamp << "\t";
				logger << "EVENT state: " << keyboardEvent.state << "\t";
#endif

				keyboardEvent.keysym = iterator->keysym;
				events.push_back(keyboardEvent);
			}
#if DEBUG
			logger << std::endl;
#endif
		}
	}
}


// Doesn't do anything with the textures argument
int Keyboard::testTextSize(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font, 
	int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures)
{
	int jumpY;
	int textWidth;
	TTF_SizeText(font, &keyLabelPart[0], &textWidth, &jumpY);
	int w;

	countKeyLabelSpaceAndWidth(&w, textWidth, widthTolerance, whiteKeyWidth);
	if (w == textWidth) {
		return 1;
	}
	else {
		return -1;
	}
}


// Returns currY for next key label part to be added, jump parameter is positive for white keys, negative for black ones
int Keyboard::drawKeyLabelPartWithoutWidthLimit(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
	int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures)
{
	SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, &keyLabelPart[0], color); 
	SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
	textures.push_back(message);		// Will be freed later

	SDL_Rect message_rect; //create a rect
	int jumpY;
	int textWidth;
	TTF_SizeText(font, &keyLabelPart[0], &textWidth, &jumpY);
	int space;

	space = countKeyLabelSpaceAndWidth(&message_rect.w, textWidth, widthTolerance, whiteKeyWidth);
	if (message_rect.w <= textWidth) {
		message_rect.w = textWidth;
		message_rect.x = key->rectangle.x;
	}
	else {
		message_rect.x = key->rectangle.x + space;
	}

	if (isWhiteKey(key->ID)) {
		message_rect.y = currY;
		currY += jumpY;
	}
	else {
		currY -= jumpY;
		message_rect.y = currY;
	}

	message_rect.h = jumpY;		// Controls the height of the rect

	SDL_RenderCopy(renderer, message, nullptr, &message_rect);

	// Free resources
	SDL_FreeSurface(surfaceMessage);
	return currY;
}


// Returns currY for next part to be added, jump parameter is positive for white keys, negative for black ones
int Keyboard::drawKeyLabelPartWithWidthLimit(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font, 
	int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures)
{
	SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, &keyLabelPart[0], color);
	SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
	textures.push_back(message);			// Will be freed later

	SDL_Rect message_rect; //create a rect
	int jumpY;
	int textWidth;
	TTF_SizeText(font, &keyLabelPart[0], &textWidth, &jumpY);
	int space;

	space = countKeyLabelSpaceAndWidth(&message_rect.w, textWidth, widthTolerance, whiteKeyWidth);
	message_rect.x = key->rectangle.x + space;

	if (isWhiteKey(key->ID)) {
		message_rect.y = currY;
		currY += jumpY;
	}
	else {
		currY -= jumpY;
		message_rect.y = currY;
	}

	message_rect.h = jumpY;		// Controls the height of the rect

	SDL_RenderCopy(renderer, message, nullptr, &message_rect);

	// Free resources
	SDL_FreeSurface(surfaceMessage);
	return currY;
}


int Keyboard::countKeyLabelSpaceAndWidth(int *messageWidth, int textWidth, int widthTolerance, int whiteKeyWidth) {
	int widthAvailable = whiteKeyWidth - widthTolerance;	
	int space;
	if (widthAvailable > textWidth) {
		*messageWidth = textWidth;
		space = (widthAvailable - textWidth) / 2;
	}
	else {
		*messageWidth = widthAvailable;
		space = 0;
	}

	return space;
}


TTF_Font * Keyboard::findFontForPlayKeys(int widthTolerance) {
	size_t j = 0;
	std::function<int(const Key*, int, const std::string&, const SDL_Color, TTF_Font*, int, SDL_Renderer*, int whiteKeyWidth, std::vector<SDL_Texture *> textures)> f = Keyboard::testTextSize;

	TTF_Font *font = nullptr;
	size_t fontSize;
	for (fontSize = Label::DEFAULT_FONT_SIZE; fontSize > 1; fontSize--) {
		font = TTF_OpenFont("arial.ttf", static_cast<int>(fontSize));
		for (; j < this->keyCount; j++) {
			if (performActionOnKeyLabels(&this->keys[j], font, widthTolerance, false, f) < 0) {
				break;
			}
		}
		if (j >= this->keyCount) {
			break;
		}
		TTF_CloseFont(font);
	}
	if (fontSize == 1) {
		font = TTF_OpenFont("arial.ttf", 2);
	}

	return font;
}


// http://lazyfoo.net/SDL_tutorials/lesson07/index.php
void Keyboard::drawKeyLabels() {
	//The font that's going to be used 
	TTF_Font *font = nullptr; 
	int widthTolerance = this->keys[0].rectangle.w / 10;
	std::string keyLabel;
	std::string name;

	std::function<int(const Key*, int, const std::string&, const SDL_Color, TTF_Font*, int, SDL_Renderer*, int whiteKeyWidth, std::vector<SDL_Texture *> textures)> f;
	f = Keyboard::drawKeyLabelPartWithoutWidthLimit;
	font = findFontForPlayKeys(widthTolerance);

	freeTextures();

	performActionOnKeyLabels(&this->recordKey, font, widthTolerance, true, f);
	f = Keyboard::drawKeyLabelPartWithWidthLimit;
	for (size_t i = 0; i < this->keyCount; i++) {
		performActionOnKeyLabels(&this->keys[i], font, widthTolerance, true, f);
	}

	TTF_CloseFont(font);
}


int Keyboard::performActionOnKeyLabels(const Key *key, TTF_Font *font, int widthTolerance, const bool draw,
			std::function<int (const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font, 
				int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures)> f)
{
	int currY = 0;
	std::string keyLabel = "";

	if (isWhiteKey(key->ID)) {
		currY = key->rectangle.y + key->rectangle.h; // controls the rect's y coordinate  
	}
	else {
		currY = key->rectangle.y;
	}


	if (key == &this->recordKey) {
		keyLabel = "RECORD KEY";
	}
	else {
		keyLabel = std::to_string(key->ID + 1);
	}

	// Draw (test if it fits) the index of key.
	currY = f(key, currY, keyLabel, GlobalConstants::RED, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
	if (!draw) {				// Since it is const parameter, compilator should optimize it out, so there should be no overhead
		if (currY < 0) {
			return currY;
		}
	}

	// https://www.libsdl.org/release/SDL-1.2.15/docs/html/guideinputkeyboard.html
	/* Check for the presence of each SDLMod value */
	/* This looks messy, but there really isn't    */
	/* a clearer way.                              */
	Uint16 mod = key->keysym.mod;
	if (mod & KMOD_NUM) {
		keyLabel = "NUM";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {				// Since it is const parameter, compilator should optimize it out, so there should be no overhead
			if (currY < 0) {
				return currY;
			}
		}
	}
	if (mod & KMOD_CAPS) {
		keyLabel = "CAP";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}

	// If it is ALT, check if it is only 1 of the possible keys or both, that's the reason why here are else if
	if (mod & KMOD_ALT) {
		keyLabel = "ALT";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}
	else if (mod & KMOD_LALT) {
		keyLabel = "LAL";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}
	else if (mod & KMOD_RALT) {
		keyLabel = "RAL";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}

	// If it is SHIFT, check if it is only 1 of the possible keys or both, that's the reason why here are else if
	if (mod & KMOD_SHIFT) {
		keyLabel = "SHI";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}
	else if (mod & KMOD_LSHIFT) {
		keyLabel = "LSH";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}
	else if (mod & KMOD_RSHIFT) {
		keyLabel = "RSH";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}

	// If it is CTRL, check if it is only 1 of the possible keys or both, that's the reason why here are else if
	if (mod & KMOD_CTRL) {
		keyLabel = "CTR";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}
	else if (mod & KMOD_LCTRL) {
		keyLabel = "LCT";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}
	else if (mod & KMOD_RCTRL) {
		keyLabel = "RCT";
		currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
		if (!draw) {
			if (currY < 0) {
				return currY;
			}
		}
	}

	keyLabel = SDL_GetKeyName(key->keysym.sym);
	if (keyLabel.size() > 2) {
		size_t index;
		if (keyLabel == "Return") {
			keyLabel = "Ret";
		}
		else if ((index = keyLabel.find("Keypad")) != std::string::npos) {
			keyLabel = "KP" + keyLabel.substr(keyLabel.size() - 1, 1);
		}
		else {
			keyLabel = keyLabel.substr(0, 2) + keyLabel.back();
		}
	}

	currY = f(key, currY, keyLabel, GlobalConstants::WHITE, font, widthTolerance, this->renderer, this->whiteKeyWidth, this->textures);
	if (!draw) {
		if (currY < 0) {
			return currY;
		}
	}

	return currY;
}



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


void Keyboard::freeTextures() {
	for (size_t i = 0; i < this->textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	this->textures.clear();
}


void Keyboard::freeKeys() {
	if (keys != nullptr) {
		for (size_t i = 0; i < keyCount; i++) {
			keys[i].freeKey();
		}

		delete[] keys;
		keys = nullptr;
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

	if (bufferOfCallbackSize != nullptr) {
		delete[] bufferOfCallbackSize;
		bufferOfCallbackSize = nullptr;
	}

	keyPressedByMouse = nullptr;
	textboxWithFocus = nullptr;

	freeSDL_AudioCVTptr(&audioFromFileCVT);

	freeTextures();
}

void Keyboard::freeSDL_AudioCVTptr(SDL_AudioCVT **cvt) {
	if (*cvt != nullptr) {
		delete[] (**cvt).buf;
		(**cvt).buf = nullptr;
		delete *cvt;
		*cvt = nullptr;
	}
}


constexpr void Keyboard::defaultInitControlKeys(size_t currKey) {
	this->recordKey.keysym.mod = KMOD_NUM;
	this->recordKey.keysym.scancode = SDL_SCANCODE_KP_MULTIPLY;
	this->recordKey.keysym.sym = SDLK_KP_MULTIPLY;

	switch (currKey) {
	case 0:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_A;
		this->keys[currKey].keysym.sym = SDLK_a;
		break;
	case 1:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_B;
		this->keys[currKey].keysym.sym = SDLK_b;
		break;
	case 2:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_C;
		this->keys[currKey].keysym.sym = SDLK_c;
		break;
	case 3:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_D;
		this->keys[currKey].keysym.sym = SDLK_d;
		break;
	case 4:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_E;
		this->keys[currKey].keysym.sym = SDLK_e;
		break;
	case 5:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F;
		this->keys[currKey].keysym.sym = SDLK_f;
		break;
	case 6:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_G;
		this->keys[currKey].keysym.sym = SDLK_g;
		break;
	case 7:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_H;
		this->keys[currKey].keysym.sym = SDLK_h;
		break;
	case 8:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_I;
		this->keys[currKey].keysym.sym = SDLK_i;
		break;
	case 9:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_J;
		this->keys[currKey].keysym.sym = SDLK_j;
		break;
	case 10:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_K;
		this->keys[currKey].keysym.sym = SDLK_k;
		break;
	case 11:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_L;
		this->keys[currKey].keysym.sym = SDLK_l;
		break;
	case 12:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_M;
		this->keys[currKey].keysym.sym = SDLK_m;
		break;
	case 13:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_N;
		this->keys[currKey].keysym.sym = SDLK_n;
		break;
	case 14:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_O;
		this->keys[currKey].keysym.sym = SDLK_o;
		break;
	case 15:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_P;
		this->keys[currKey].keysym.sym = SDLK_p;
		break;
	case 16:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_Q;
		this->keys[currKey].keysym.sym = SDLK_q;
		break;
	case 17:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_R;
		this->keys[currKey].keysym.sym = SDLK_r;
		break;
	case 18:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_S;
		this->keys[currKey].keysym.sym = SDLK_s;
		break;
	case 19:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_T;
		this->keys[currKey].keysym.sym = SDLK_t;
		break;
	case 20:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_U;
		this->keys[currKey].keysym.sym = SDLK_u;
		break;
	case 21:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_V;
		this->keys[currKey].keysym.sym = SDLK_v;
		break;
	case 22:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_W;
		this->keys[currKey].keysym.sym = SDLK_w;
		break;
	case 23:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_X;
		this->keys[currKey].keysym.sym = SDLK_x;
		break;
	case 24:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_Y;
		this->keys[currKey].keysym.sym = SDLK_y;
		break;
	case 25:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_Z;
		this->keys[currKey].keysym.sym = SDLK_z;
		break;
	case 26:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_1;
		this->keys[currKey].keysym.sym = SDLK_1;
		break;
	case 27:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_2;
		this->keys[currKey].keysym.sym = SDLK_2;
		break;
	case 28:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_3;
		this->keys[currKey].keysym.sym = SDLK_3;
		break;
	case 29:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_4;
		this->keys[currKey].keysym.sym = SDLK_4;
		break;
	case 30:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_5;
		this->keys[currKey].keysym.sym = SDLK_5;
		break;
	case 31:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_6;
		this->keys[currKey].keysym.sym = SDLK_6;
		break;
	case 32:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_7;
		this->keys[currKey].keysym.sym = SDLK_7;
		break;
	case 33:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_8;
		this->keys[currKey].keysym.sym = SDLK_8;
		break;
	case 34:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_9;
		this->keys[currKey].keysym.sym = SDLK_9;
		break;
	case 35:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_0;
		this->keys[currKey].keysym.sym = SDLK_0;
		break;
	case 36:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F1;
		this->keys[currKey].keysym.sym = SDLK_F1;
		break;
	case 37:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F2;
		this->keys[currKey].keysym.sym = SDLK_F2;
		break;
	case 38:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F3;
		this->keys[currKey].keysym.sym = SDLK_F3;
		break;
	case 39:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F4;
		this->keys[currKey].keysym.sym = SDLK_F4;
		break;
	case 40:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F5;
		this->keys[currKey].keysym.sym = SDLK_F5;
		break;
	case 41:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F6;
		this->keys[currKey].keysym.sym = SDLK_F6;
		break;
	case 42:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F7;
		this->keys[currKey].keysym.sym = SDLK_F7;
		break;
	case 43:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F8;
		this->keys[currKey].keysym.sym = SDLK_F8;
		break;
	case 44:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F9;
		this->keys[currKey].keysym.sym = SDLK_F9;
		break;
	case 45:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F10;
		this->keys[currKey].keysym.sym = SDLK_F10;
		break;
	case 46:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F11;
		this->keys[currKey].keysym.sym = SDLK_F11;
		break;
	case 47:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_0;
		this->keys[currKey].keysym.sym = SDLK_KP_0;
		break;
	case 48:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_1;
		this->keys[currKey].keysym.sym = SDLK_KP_1;
		break;
	case 49:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_2;
		this->keys[currKey].keysym.sym = SDLK_KP_2;
		break;
	case 50:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_3;
		this->keys[currKey].keysym.sym = SDLK_KP_3;
		break;
	case 51:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_4;
		this->keys[currKey].keysym.sym = SDLK_KP_4;
		break;
	case 52:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_5;
		this->keys[currKey].keysym.sym = SDLK_KP_5;
		break;
	case 53:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_6;
		this->keys[currKey].keysym.sym = SDLK_KP_6;
		break;
	case 54:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_7;
		this->keys[currKey].keysym.sym = SDLK_KP_7;
		break;
	case 55:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_8;
		this->keys[currKey].keysym.sym = SDLK_KP_8;
		break;
	case 56:
		this->keys[currKey].keysym.mod = KMOD_NUM;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_9;
		this->keys[currKey].keysym.sym = SDLK_KP_9;
		break;
	case 57:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_A;
		this->keys[currKey].keysym.sym = SDLK_a;
		break;
	case 58:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_B;
		this->keys[currKey].keysym.sym = SDLK_b;
		break;
	case 59:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_C;
		this->keys[currKey].keysym.sym = SDLK_c;
		break;
	case 60:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_D;
		this->keys[currKey].keysym.sym = SDLK_d;
		break;
	case 61:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_E;
		this->keys[currKey].keysym.sym = SDLK_e;
		break;
	case 62:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_F;
		this->keys[currKey].keysym.sym = SDLK_f;
		break;
	case 63:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_G;
		this->keys[currKey].keysym.sym = SDLK_g;
		break;
	case 64:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_H;
		this->keys[currKey].keysym.sym = SDLK_h;
		break;
	case 65:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_I;
		this->keys[currKey].keysym.sym = SDLK_i;
		break;
	case 66:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_J;
		this->keys[currKey].keysym.sym = SDLK_j;
		break;
	case 67:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_K;
		this->keys[currKey].keysym.sym = SDLK_k;
		break;
	case 68:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_L;
		this->keys[currKey].keysym.sym = SDLK_l;
		break;
	case 69:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_M;
		this->keys[currKey].keysym.sym = SDLK_m;
		break;
	case 70:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_N;
		this->keys[currKey].keysym.sym = SDLK_n;
		break;
	case 71:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_O;
		this->keys[currKey].keysym.sym = SDLK_o;
		break;
	case 72:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_P;
		this->keys[currKey].keysym.sym = SDLK_p;
		break;
	case 73:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_Q;
		this->keys[currKey].keysym.sym = SDLK_q;
		break;
	case 74:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_R;
		this->keys[currKey].keysym.sym = SDLK_r;
		break;
	case 75:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_S;
		this->keys[currKey].keysym.sym = SDLK_s;
		break;
	case 76:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_T;
		this->keys[currKey].keysym.sym = SDLK_t;
		break;
	case 77:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_U;
		this->keys[currKey].keysym.sym = SDLK_u;
		break;
	case 78:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_V;
		this->keys[currKey].keysym.sym = SDLK_v;
		break;
	case 79:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_W;
		this->keys[currKey].keysym.sym = SDLK_w;
		break;
	case 80:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_X;
		this->keys[currKey].keysym.sym = SDLK_x;
		break;
	case 81:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_Y;
		this->keys[currKey].keysym.sym = SDLK_y;
		break;
	case 82:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_Z;
		this->keys[currKey].keysym.sym = SDLK_z;
		break;
	case 83:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_1;
		this->keys[currKey].keysym.sym = SDLK_1;
		break;
	case 84:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_2;
		this->keys[currKey].keysym.sym = SDLK_2;
		break;
	case 85:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_3;
		this->keys[currKey].keysym.sym = SDLK_3;
		break;
	case 86:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_4;
		this->keys[currKey].keysym.sym = SDLK_4;
		break;
	case 87:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_5;
		this->keys[currKey].keysym.sym = SDLK_5;
		break;
	default:
		this->keys[currKey].keysym.mod = KMOD_NONE;
		this->keys[currKey].keysym.scancode = SDL_SCANCODE_5;
		this->keys[currKey].keysym.sym = SDLK_5;
		break;
	}
}