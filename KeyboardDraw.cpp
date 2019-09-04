#include "Keyboard.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Draw methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	for (size_t i = 0; i < this->keys.size(); i++) {
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
	for (size_t i = 0; i < this->keys.size(); i++) {		// Draw black keys
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
	for (size_t i = 0; i < this->keys.size(); i++) {
		performActionOnKeyLabels(&this->keys[i], font, widthTolerance, true, f);
	}

	TTF_CloseFont(font);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Text/Font methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TTF_Font * Keyboard::findFontForPlayKeys(int widthTolerance) {
	size_t j = 0;
	std::function<int(const Key*, int, const std::string&, const SDL_Color, TTF_Font*, int, SDL_Renderer*, int whiteKeyWidth, std::vector<SDL_Texture *> textures)> f = Keyboard::testTextSize;

	TTF_Font *font = nullptr;
	size_t fontSize;
	for (fontSize = Label::DEFAULT_FONT_SIZE; fontSize > 1; fontSize--) {
		font = TTF_OpenFont("arial.ttf", static_cast<int>(fontSize));
		for (; j < this->keys.size(); j++) {
			if (performActionOnKeyLabels(&this->keys[j], font, widthTolerance, false, f) < 0) {
				break;
			}
		}
		if (j >= this->keys.size()) {
			break;
		}
		TTF_CloseFont(font);
	}
	if (fontSize == 1) {
		font = TTF_OpenFont("arial.ttf", 2);
	}

	return font;
}

// Doesn't do anything with the textures argument
int Keyboard::testTextSize(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
	int widthTolerance, SDL_Renderer *renderer, int whiteKeyWidth, std::vector<SDL_Texture *> textures)
{
	static_cast<void>(renderer);
	static_cast<void>(color);
	static_cast<void>(currY);
	static_cast<void>(key);


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


int Keyboard::performActionOnKeyLabels(const Key *key, TTF_Font *font, int widthTolerance, const bool draw,
	std::function<int(const Key *key, int currY, const std::string &keyLabelPart, const SDL_Color color, TTF_Font *font,
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


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Text/Font draw methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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