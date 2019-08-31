#include "Textbox.h"

#include "Main.h"

Label::Label() {
	this->text = "";
	this->fontSize = DEFAULT_FONT_SIZE;
	this->font = TTF_OpenFont("arial.ttf", static_cast<int>(fontSize));
}

Label::Label(const std::string &text) {
	this->text = text;
	this->fontSize = DEFAULT_FONT_SIZE;
	this->font = TTF_OpenFont("arial.ttf", static_cast<int>(fontSize));
}


constexpr void Label::moveTexture() {
	if (oldTexture != nullptr) {
		SDL_DestroyTexture(oldTexture);
	}
	oldTexture = newTexture;
	newTexture = nullptr;			// Set to nullptr, so after 2 calls both oldTexture and newTexture are == nullptr
}

void Label::freeResources() {
	moveTexture();
	moveTexture();
	TTF_CloseFont(font);
	font = nullptr;
}


void Label::setSizesBasedOnButtonSize(int textX, int textY, int buttonX, int buttonY, int buttonW, int buttonH) {
	int textW; 
	int textH;
	this->findFittingFont(&textW, &textH, buttonW, buttonH);
	this->setSizes(textX, textY, textW, textH, buttonX, buttonY, buttonW, buttonH);
}

constexpr void Label::setSizes(int textX, int textY, int textW, int textH, int buttonX, int buttonY, int buttonW, int buttonH) {
	this->textRectangle.x = textX;
	this->textRectangle.y = textY;
	this->textRectangle.w = textW;
	this->textRectangle.h = textH;
	this->button.rectangle.x = buttonX;
	this->button.rectangle.y = buttonY;
	this->button.rectangle.w = buttonW;
	this->button.rectangle.h = buttonH;
}


void Label::findFittingFont(int *textWidth, int *textHeight, int widthLimit, int heightLimit) {
	// First free the old font
	if (this->font != nullptr) {
		TTF_CloseFont(this->font);
	}
	size_t fontSizeIter;
	for (fontSizeIter = DEFAULT_FONT_SIZE; fontSizeIter > 1; fontSizeIter--) {
		this->font = TTF_OpenFont("arial.ttf", static_cast<int>(fontSizeIter));
		if (testTextSize(this->text, this->font, textWidth, textHeight, widthLimit, heightLimit) >= 0) {
			this->fontSize = fontSizeIter;
			break;
		}
		TTF_CloseFont(this->font);
	}
	if (fontSizeIter == 1) {
		this->fontSize = fontSizeIter;
		this->font = TTF_OpenFont("arial.ttf", 1);
	}
}


// Returns minus numbers if text is too big, Sets textWidth and textHeight arguments
int Label::testTextSize(const std::string &textToTest, TTF_Font *textFont, int *textWidth, int *textHeight, int widthLimit, int heightLimit) {
	TTF_SizeText(textFont, &textToTest[0], textWidth, textHeight);
	if (*textHeight > heightLimit) {
		return -1;
	}
	else if (*textWidth > widthLimit) {
		return -2;
	}

	return 0;
}


void Label::drawTextWithBackground(SDL_Renderer *renderer, SDL_Color color, SDL_Color bgHasFocusColor, SDL_Color bgHasNotFocusColor) {
	if (this->hasFocus) {
		SDL_SetRenderDrawColor(renderer, bgHasFocusColor.r, bgHasFocusColor.g, bgHasFocusColor.b, bgHasFocusColor.a);
	}
	else {
		SDL_SetRenderDrawColor(renderer, bgHasNotFocusColor.r, bgHasNotFocusColor.g, bgHasNotFocusColor.b, bgHasNotFocusColor.a);
	}

	SDL_RenderFillRect(renderer, &this->button.rectangle);
	this->drawText(renderer, color);
}


void Label::drawText(SDL_Renderer *renderer, SDL_Color color) {
	if (this->text.size() != 0) {
		SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, &this->text[0], color);
		moveTexture();
		newTexture = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
		SDL_RenderCopy(renderer, newTexture, nullptr, &this->textRectangle);

		// Free resources
		SDL_FreeSurface(surfaceMessage);
	}
}


// Uses modified code from here http://lazyfoo.net/tutorials/SDL/32_text_input_and_clipboard_handling/index.php
bool Textbox::processKeyEvent(const SDL_Event &e, bool *enterPressed, bool *shouldResizeTextbox) {
	bool renderText = false;

	//Special key input
	if (e.type == SDL_KEYDOWN) {
		//Handle enter
		if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
			*enterPressed = true;
			renderText = true;
		}
		//Handle backspace 
		else if (e.key.keysym.sym == SDLK_BACKSPACE && this->text.size() > 0) {
			//lop off character 
			this->text.pop_back(); 
			renderText = true;
		}
		//Handle copy 
		else if (e.key.keysym.sym == SDLK_c && SDL_GetModState() & KMOD_CTRL) {
			SDL_SetClipboardText(this->text.c_str());
			return renderText;
		}
		//Handle paste 
		else if (e.key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL) {
			this->text = SDL_GetClipboardText();
			renderText = true;
		}
	}
	//Special text input event 
	else if (e.type == SDL_TEXTINPUT) {
		//Not copy or pasting 
		if (!((e.text.text[0] == 'c' || e.text.text[0] == 'C') && (e.text.text[0] == 'v' || e.text.text[0] == 'V') && SDL_GetModState() & KMOD_CTRL)) {
			//Append character 
			this->text += e.text.text;
			renderText = true;
		}
	}

	if (testTextSize(this->text, this->font, &this->textRectangle.w, &this->textRectangle.h, this->button.rectangle.w, this->button.rectangle.h) < 0) {
		this->findFittingFont(&this->textRectangle.w, &this->textRectangle.h, this->button.rectangle.w, this->button.rectangle.h);
		*shouldResizeTextbox = true;
	}
	else if (this->fontSize != DEFAULT_FONT_SIZE && this->textRectangle.h < this->button.rectangle.h - 3 && this->textRectangle.w < 5 * this->button.rectangle.w / 8) {
		this->findFittingFont(&this->textRectangle.w, &this->textRectangle.h, this->button.rectangle.w, this->button.rectangle.h);
		*shouldResizeTextbox = true;
	}

	return renderText;
}

void Textbox::processEnterPressedEvent(Keyboard &keyboard) {
	keyboard.textboxWithFocus = nullptr;
	SDL_StopTextInput();
};

void Textbox::drawTextbox(SDL_Renderer *renderer) { 
	this->drawTextWithBackground(renderer, GlobalConstants::WHITE, GlobalConstants::RED, GlobalConstants::BACKGROUND_BLUE); 
}


void ConfigFileTextbox::processEnterPressedEvent(Keyboard &keyboard) {
	size_t totalLines = 0;
	keyboard.readConfigfile(&totalLines);
	this->hasFocus = false;
	keyboard.drawWindow();

	Textbox::processEnterPressedEvent(keyboard);
}

void ConfigFileTextbox::drawTextbox(SDL_Renderer *renderer) {
	this->drawTextWithBackground(renderer, GlobalConstants::WHITE, GlobalConstants::RED, GlobalConstants::BACKGROUND_BLUE);
}


void DirectoryWithFilesTextbox::processEnterPressedEvent(Keyboard &keyboard) {
	keyboard.setKeySoundsFromDirectory();
	this->hasFocus = false;

	Textbox::processEnterPressedEvent(keyboard);
}

void DirectoryWithFilesTextbox::drawTextbox(SDL_Renderer *renderer) {
	this->drawTextWithBackground(renderer, GlobalConstants::WHITE, GlobalConstants::RED, GlobalConstants::BLACK);
}


void RecordFilePathTextbox::processEnterPressedEvent(Keyboard &keyboard) {
	this->hasFocus = false;

	Textbox::processEnterPressedEvent(keyboard);
}

void RecordFilePathTextbox::drawTextbox(SDL_Renderer *renderer) {
	this->drawTextWithBackground(renderer, GlobalConstants::WHITE, GlobalConstants::RED, GlobalConstants::BACKGROUND_BLUE);
}


void PlayFileTextbox::processEnterPressedEvent(Keyboard &keyboard) {
	this->hasFocus = false;
	keyboard.playFile(this->text);

	Textbox::processEnterPressedEvent(keyboard);
}

void PlayFileTextbox::drawTextbox(SDL_Renderer *renderer) {
	this->drawTextWithBackground(renderer, GlobalConstants::WHITE, GlobalConstants::RED, GlobalConstants::BLACK);
}