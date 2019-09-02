#include "Keyboard.h"

#include <sstream>
#include <filesystem>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Help methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Set methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Keyboard::setRenderer() {
	if (this->renderer == nullptr) {
		this->renderer = SDL_CreateRenderer(this->window, -1, SDL_RENDERER_ACCELERATED);
	}
}


// Sets blackKeysCount and whiteKeysCount properties
void Keyboard::setBlackAndWhiteKeysCounts() {
	this->blackKeysCount = countBlackKeys(this->keys.size());
	this->whiteKeysCount = this->keys.size() - this->blackKeysCount;
}


void Keyboard::setKeySoundsFromDirectory() {
	size_t i = 0;
	for (const auto &element : std::experimental::filesystem::directory_iterator(this->directoryWithFilesTextbox.text)) {
		std::string path = element.path().string();
		size_t extensionIndex = path.size() - 4;
		if (extensionIndex > 0 && path.substr(extensionIndex) == ".wav") {
			if (i >= keys.size()) {
				break;
			}
			keys[i].setAudioBufferWithFile(&path[0], &this->audioSpec);
			i++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Key set methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
constexpr void Keyboard::setKeySizes() {
	this->upperLeftCornerForKeysY = windowHeight / 3;
	this->whiteKeyWidth = windowWidth / static_cast<int>(this->whiteKeysCount);
	this->whiteKeyHeight = windowHeight / 4;
	this->blackKeyWidth = 3 * this->whiteKeyWidth / 4;
	this->blackKeyHeight = this->whiteKeyHeight / 2;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Resize methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	this->audioPlayingLabel.findFittingFont(&textW, &textH, buttonW, buttonH);
	textY = this->upperLeftCornerForKeysY / 2 + (buttonH - textH) / 2;
	buttonY = this->upperLeftCornerForKeysY / 2;
	this->audioPlayingLabel.setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);
}


void Keyboard::resizeKeyboardKeys() {
	setKeySizes();
	int currX = 0;
	for (size_t i = 0; i < keys.size(); i++) {
		setKeyLocation(&currX, i);
	}

	this->recordKey.resizeButton(0, 0, this->whiteKeyWidth, 3 * this->blackKeyHeight / 4);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Set methods from config file
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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