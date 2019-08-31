#include "Key.h"
#include "Keyboard.h"


TimestampAndID::TimestampAndID(Uint32 timestamp, size_t ID, KeyEventType keyEventType) {
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


constexpr void Key::initKey() {
	this->audioConvert.buf = nullptr;
	this->startOfCurrAudio = 0;
}


constexpr void Button::resizeButton(int x, int y, int w, int h) {
	this->rectangle.x = x;
	this->rectangle.y = y;
	this->rectangle.w = w;
	this->rectangle.h = h;
}

bool Button::checkMouseClick(const SDL_MouseButtonEvent &event) {
	return (event.x >= this->rectangle.x && event.x <= this->rectangle.x + this->rectangle.w &&
		event.y >= this->rectangle.y && event.y <= this->rectangle.y + this->rectangle.h);
}


bool Key::setAudioBufferWithFile(char *filename, SDL_AudioSpec *desiredAudioSpec) {
	Uint8 *audioBuffer = nullptr;
	Uint32 audioBufferLen = 0;

	SDL_AudioSpec spec = SDL_AudioSpec();
	SDL_AudioSpec *specPtr = &spec;

	specPtr = SDL_LoadWAV(filename, specPtr, &audioBuffer, &audioBufferLen);
	if (specPtr != nullptr) {
		this->isDefaultSound = false;
		delete[] this->audioConvert.buf;
		this->audioConvert.buf = nullptr;

		Keyboard::convert(&this->audioConvert, specPtr, desiredAudioSpec, audioBuffer, audioBufferLen);
		SDL_FreeWAV(audioBuffer);
		audioBuffer = nullptr;

		return true;
	}
	else {
		return false;
	}
}


void Key::freeKey() {
	delete[] this->audioConvert.buf;
	this->audioConvert.buf = nullptr;
}