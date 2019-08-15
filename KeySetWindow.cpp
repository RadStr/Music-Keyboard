#include "KeySetWindow.h"

KeySetWindow::KeySetWindow(Key *key, SDL_Color color, Keyboard *keyboard) {
	this->keyboard = keyboard;
	isWindowEvent = false;
	this->key = key;
	window = SDL_CreateWindow("Key set window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	this->renderer = SDL_CreateRenderer(this->window, -1, SDL_RENDERER_ACCELERATED);
	this->keyLabel.hasFocus = false;
	this->fileTextbox.hasFocus = false;
	this->needRedraw = false;
	this->color = color;
	SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);

	keyLabel.button.rectangle.x = 0;
	keyLabel.button.rectangle.y = 0;
	fileTextbox.button.rectangle.x = 0;

	keyLabel.text = "Press this button with mouse. Then press key to change the key control. ID of this key is " + std::to_string(key->ID + 1);;
	fileTextbox.text = "Click here on the bottom half of window and add file from which the sound should be taken.";

	resizeWindow(windowWidth, windowHeight);
}


bool KeySetWindow::mainCycle() {
	SDL_Event event;
	bool quit = false;
	drawWindow();
	bool returnVal = false;
	bool render;


	while (!quit) {
		isWindowEvent = false;
		needRedraw = false;
		render = false;

		if (SDL_PollEvent(&event)) {
			quit = checkEvents(event, &returnVal);
		}
		if (needRedraw) {
			setFileTextbox(this->fileTextbox.textRectangle.w, this->fileTextbox.textRectangle.h);
			setKeyLabel(this->keyLabel.textRectangle.w, this->keyLabel.textRectangle.h);
			this->drawTextboxes();
			render = true;
		}

		if (isWindowEvent) {
			// Redraw the keyboard window
			SDL_SetRenderDrawColor(this->keyboard->renderer, 0, 0, 100, 255);
			// Clear window
			SDL_RenderClear(this->keyboard->renderer);
			this->keyboard->drawWindow();
			this->keyboard->drawKeyLabels();
			SDL_RenderPresent(this->keyboard->renderer);
			render = true;
		}
		if (render) {
			SDL_RenderPresent(this->renderer);
		}
	}

	freeResources();
	return returnVal;
}


bool KeySetWindow::checkEvents(const SDL_Event &event, bool *returnVal) {
	Uint32 flags = SDL_GetWindowFlags(this->window);
	bool enterPressed = false;
	bool shouldResizeTextboxes = false;

	switch (event.type) {
	case SDL_TEXTINPUT:
		if ((flags & SDL_WINDOW_INPUT_FOCUS) != 0) {
			if (fileTextbox.hasFocus) {
				needRedraw = this->fileTextbox.processKeyEvent(event, &enterPressed, &shouldResizeTextboxes);
			}
		}
		break;
	case SDL_KEYDOWN:
		if ((flags & SDL_WINDOW_INPUT_FOCUS) != 0) {
			if (keyLabel.hasFocus) {
				this->key->keysym = event.key.keysym;
				keyLabel.text = "Key " + std::to_string(this->key->ID + 1) + " changed to " + (std::string)SDL_GetScancodeName(event.key.keysym.scancode) + " with current mods.";
				this->keyLabel.findFittingFont(&this->keyLabel.textRectangle.w, &this->keyLabel.textRectangle.h, this->windowWidth, this->windowHeight / 2);
				this->setKeyLabel(this->keyLabel.textRectangle.w, this->keyLabel.textRectangle.h);
				keyboard->drawKeyLabels();
				keyLabel.hasFocus = false;
				fileTextbox.hasFocus = false;

				needRedraw = true;
				isWindowEvent = true;
			}
			else if (fileTextbox.hasFocus) {
				needRedraw = this->fileTextbox.processKeyEvent(event, &enterPressed, &shouldResizeTextboxes);
			}
		}
		break;
	case SDL_KEYUP:
		break;
	case SDL_MOUSEBUTTONDOWN:
		break;
	case SDL_MOUSEBUTTONUP:
		if ((flags & SDL_WINDOW_MOUSE_FOCUS) != 0) {
			if (event.button.button == SDL_BUTTON_LEFT) {
				if (keyLabel.button.checkMouseClick(event.button))
				{
					keyLabel.hasFocus = !keyLabel.hasFocus;
					fileTextbox.hasFocus = false;
					SDL_StopTextInput();
				}
				else if (fileTextbox.button.checkMouseClick(event.button))
				{
					keyLabel.hasFocus = false;
					fileTextbox.hasFocus = !fileTextbox.hasFocus;
					if (fileTextbox.hasFocus) {
						SDL_StartTextInput();
					}
					else {
						SDL_StopTextInput();
					}
				}
				else {
					keyLabel.hasFocus = false;
					fileTextbox.hasFocus = false;
					SDL_StopTextInput();
				}
				needRedraw = true;
			}
		}
		break;
	case SDL_WINDOWEVENT:
		if ((flags & SDL_WINDOW_INPUT_FOCUS) != 0) {
			switch (event.window.event) {
			case SDL_WINDOWEVENT_MINIMIZED:
				break;
			case SDL_WINDOWEVENT_HIDDEN:
				break;
			case SDL_WINDOWEVENT_MOVED:
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				break;
			case SDL_WINDOWEVENT_RESIZED:
				resizeWindow(event.window);
				drawWindow();
				break;
			case SDL_WINDOWEVENT_CLOSE:
				if (event.window.windowID != SDL_GetWindowID(this->window)) {
					*returnVal = true;
				}
				return true;
			default:
				break;
			}
		}
		else {
			if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
				*returnVal = true;
				return true;
			}
			else {
				if (event.window.event == SDL_WINDOWEVENT_MOVED || event.window.event == SDL_WINDOWEVENT_RESIZED) {
					isWindowEvent = true;
					keyboard->checkEvents(event);
				}
			}
		}
		break;
	case SDL_QUIT:
		return true;
	default:
		break;
	}


	if (enterPressed) {
		this->key->setAudioBufferWithFile(&this->fileTextbox.text[0], this->keyboard->audioSpec);
		this->keyLabel.hasFocus = false;
		this->fileTextbox.hasFocus = false;
	}

	if (shouldResizeTextboxes) {
		setWindowButtons(this->fileTextbox.textRectangle.w, this->fileTextbox.textRectangle.h, this->keyLabel.textRectangle.w, this->keyLabel.textRectangle.h);
	}

	return false;
}


void KeySetWindow::resizeWindow(int w, int h) {
	this->windowWidth = w;
	this->windowHeight = h;

	int w1;
	int h1;
	int w2;
	int h2;
	this->fileTextbox.findFittingFont(&w1, &h1, this->windowWidth, this->windowHeight / 2);
	this->keyLabel.findFittingFont(&w2, &h2, this->windowWidth, this->windowHeight / 2);
	setWindowButtons(w1, h1, w2, h2);
}


void KeySetWindow::resizeWindow(const SDL_WindowEvent &event) {
	resizeWindow(event.data1, event.data2);
}


void KeySetWindow::setWindowButtons(int fileTextboxWidth, int fileTextboxHeight, int keyLabelWidth, int keyLabelHeight) {
	setFileTextbox(fileTextboxWidth, fileTextboxHeight);
	setKeyLabel(keyLabelWidth, keyLabelHeight);
}


constexpr void KeySetWindow::setKeyLabel(int w, int h) {
	keyLabel.textRectangle.x = (this->windowWidth - w) / 2;
	keyLabel.textRectangle.y = -this->windowHeight / 4 + (this->windowHeight - h) / 2;
	keyLabel.textRectangle.w = w;
	keyLabel.textRectangle.h = h;

	keyLabel.button.rectangle.w = this->windowWidth;
	keyLabel.button.rectangle.h = this->windowHeight / 2;
}


constexpr void KeySetWindow::setFileTextbox(int w, int h) {
	this->fileTextbox.textRectangle.x = (this->windowWidth - w) / 2;
	this->fileTextbox.textRectangle.y = this->windowHeight / 4 + (this->windowHeight - h) / 2;
	this->fileTextbox.textRectangle.w = w;
	this->fileTextbox.textRectangle.h = h;

	fileTextbox.button.rectangle.y = this->windowHeight / 2;
	fileTextbox.button.rectangle.w = this->windowWidth;
	fileTextbox.button.rectangle.h = this->windowHeight;
}


void KeySetWindow::drawWindow() {
	SDL_RenderClear(renderer);
	drawTextboxes();
	needRedraw = false;

	SDL_RenderPresent(renderer);
}


void KeySetWindow::drawTextboxes() {
	this->keyLabel.drawTextWithBackground(this->renderer, this->color, GlobalConstants::RED, GlobalConstants::BLACK);
	this->fileTextbox.drawTextWithBackground(this->renderer, this->color, GlobalConstants::RED, GlobalConstants::BACKGROUND_BLUE);
}


void KeySetWindow::freeResources() {
	// Destroy renderer
	SDL_DestroyRenderer(renderer);
	// Close and destroy the window
	SDL_DestroyWindow(window);
}