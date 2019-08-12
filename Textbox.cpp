#include "Textbox.h"


const size_t Label::DEFAULT_FONT_SIZE = 26;


Label::Label() {
	this->text = "";
	this->fontSize = DEFAULT_FONT_SIZE;
	this->font = TTF_OpenFont("arial.ttf", fontSize);
}

Label::Label(const std::string &text) {
	this->text = text;
	this->fontSize = DEFAULT_FONT_SIZE;
	this->font = TTF_OpenFont("arial.ttf", fontSize);
}

Textbox::Textbox() : Label() { }
Textbox::Textbox(const std::string &text) : Label(text) { }


constexpr void Label::moveTexture() {
	if (oldTexture != nullptr) {
		SDL_DestroyTexture(oldTexture);
	}
	oldTexture = newTexture;
	newTexture = nullptr;			// Set to nullptr, so after 2 calls both oldTexture and newTexture are == nullptr
}

void Label::freeLabel() {
	moveTexture();
	moveTexture();
	TTF_CloseFont(font);
	font = nullptr;
}

void Textbox::freeTextbox() {
	freeLabel();
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
	size_t fontSize;
	for (fontSize = DEFAULT_FONT_SIZE; fontSize > 1; fontSize--) {
		this->font = TTF_OpenFont("arial.ttf", fontSize);
		if (testTextSize(this->text, this->font, textWidth, textHeight, widthLimit, heightLimit) >= 0) {
			this->fontSize = fontSize;
			break;
		}
		TTF_CloseFont(this->font);
	}
	if (fontSize == 1) {
		this->fontSize = fontSize;
		this->font = TTF_OpenFont("arial.ttf", 1);
	}
}


// Returns minus numbers if text is too big, Sets textWidth and textHeight arguments
int Label::testTextSize(const std::string &text, TTF_Font *font, int *textWidth, int *textHeight, int widthLimit, int heightLimit) {
	TTF_SizeText(font, &text[0], textWidth, textHeight);
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