#include "Key.h"
#include "Keyboard.h"


TimestampAndID::TimestampAndID(Uint32 timestamp, int ID, KeyEventType keyEventType) {
	this->timestamp = timestamp;
	this->ID = ID;
	this->keyEventType = keyEventType;
}


Button::Button() = default;


Key::Key() : Button() {
	initKey();
}


Key::Key(int x, int y, int w, int h) {
	initKey();
	resizeButton(x, y, w, h);
}


void Key::initKey() {
	this->audioConvert.buf = NULL;
	this->startOfCurrAudio = 0;
}

void Button::resizeButton(int x, int y, int w, int h) {
	this->rectangle.x = x;
	this->rectangle.y = y;
	this->rectangle.w = w;
	this->rectangle.h = h;
}

bool Button::checkMouseClick(const SDL_MouseButtonEvent &event) {
	return (event.x >= this->rectangle.x && event.x <= this->rectangle.x + this->rectangle.w &&
		event.y >= this->rectangle.y && event.y <= this->rectangle.y + this->rectangle.h);
}


void Key::setAudioBufferWithFile(char *filename, SDL_AudioSpec *desiredAudioSpec) {
	this->isDefaultSound = false;
	Uint8 *audioBuffer = NULL;
	Uint32 audioBufferLen = 0;

	SDL_AudioSpec *spec = new SDL_AudioSpec();
	SDL_AudioSpec *specForFree = spec;

	spec = SDL_LoadWAV(filename, spec, &audioBuffer, &audioBufferLen);
	if (spec != NULL) {
		if (this->audioConvert.buf != NULL) {
			std::free(this->audioConvert.buf);
			this->audioConvert.buf = NULL;
		}
		Keyboard::convert(&this->audioConvert, spec, desiredAudioSpec, audioBuffer, audioBufferLen);

		SDL_FreeWAV(audioBuffer);
		audioBuffer = NULL;

		if (spec != NULL) {				
			if (spec != specForFree) {		// This case shouldn't happen, but just in case
				std::free(spec);
				std::free(specForFree);
			}
			else {
				std::free(spec);
			}
		}		
	}
	else {
		std::free(specForFree);
	}
}


void Key::freeKey() {
	if (this->audioConvert.buf != NULL) {
		std::free(this->audioConvert.buf);
		this->audioConvert.buf = NULL;
	}
}