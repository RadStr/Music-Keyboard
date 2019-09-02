#include "Keyboard.h"

#include <sstream>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Audio methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	if (vectorOfCallbackSize.size() < static_cast<size_t>(bytes)) {
		vectorOfCallbackSize.resize(bytes);
	}

	if (this->currentlyUnpressedKeys[0].size() > 0) {		// Each channel has the same number of samples, so we can just take [0] index
		// this->currentlyUnpressedKeys.size() == this->audioSpec.channels ... keep that in mind
		int byteSize = SDL_AUDIO_BITSIZE(this->audioSpec.format) / 8;
		std::vector<Sint32> vals = std::vector<Sint32>(this->audioSpec.channels);
		std::vector<Sint32> factors = std::vector<Sint32>(this->audioSpec.channels);
		std::vector<Sint32> mods = std::vector<Sint32>(this->audioSpec.channels);

		// The number we will decrement from the mods to get 0. It is +1 if the mod is positive, is -1 if negative. And we will also decrement it from the value
		// We are just removing the excess, which couldn't be covered by the factors
		std::vector<Sint32> modDecrementer = std::vector<Sint32>(this->audioSpec.channels);

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
						Uint8 *p = static_cast<Uint8 *>(&vectorOfCallbackSize[index]);
						*p = static_cast<Uint8>(vals[ch]);
						break;
					}
					case AUDIO_S8:
					{
						Sint8 *p = reinterpret_cast<Sint8 *>(&vectorOfCallbackSize[index]);
						*p = static_cast<Sint8>(vals[ch]);
						break;
					}
					case AUDIO_U16SYS:
					{
						Uint16 *p = reinterpret_cast<Uint16 *>(&vectorOfCallbackSize[index]);
						*p = static_cast<Uint16>(vals[ch]);
						break;
					}
					case AUDIO_S16SYS:
					{
						Sint16 *p = reinterpret_cast<Sint16 *>(&vectorOfCallbackSize[index]);
						*p = static_cast<Sint16>(vals[ch]);
						break;
					}
					case AUDIO_S32SYS:
					{
						Sint32 *p = reinterpret_cast<Sint32 *>(&vectorOfCallbackSize[index]);
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
					vectorOfCallbackSize[j] = this->audioSpec.silence;
				}
			}
			SDL_MixAudioFormat(bufferToBePlayed, vectorOfCallbackSize.data(), this->audioSpec.format, bytes, SDL_MIX_MAXVOLUME);
		}

		for (size_t i = 0; i < this->currentlyUnpressedKeys.size(); i++) {
			this->currentlyUnpressedKeys[i].clear();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Sound synthesis methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Keyboard::generateTone(const SDL_AudioSpec &spec, int keyID, Uint32 numberOfSeconds, SDL_AudioCVT *keyCVT) {
	SDL_AudioSpec sourceSpec;
	sourceSpec.channels = 1;
	sourceSpec.format = 0b0000000000001000; //0b0000_0000_0000_1000 Unsigned and 1 byte sample size little endian
	sourceSpec.freq = spec.freq;
	sourceSpec.samples = spec.samples;
	sourceSpec.silence = 127;
	sourceSpec.size = sourceSpec.samples;

	int ret = SDL_BuildAudioCVT(keyCVT, sourceSpec.format, sourceSpec.channels, sourceSpec.freq, spec.format, spec.channels, spec.freq);

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

	keyCVT->buf = buffer;
	keyCVT->len = sourceAudioByteSize;
	if (ret != 0) {
		convertAudioAndSaveMemory(keyCVT, bufferLen);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Convert methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

// Should be called with keyboard->audioSpec (this->audioSpec) as desiredSpec
void Keyboard::convert(SDL_AudioCVT *cvt, SDL_AudioSpec *spec, SDL_AudioSpec *desiredSpec, Uint8 *buffer, Uint32 len) {
	int ret = SDL_BuildAudioCVT(cvt, spec->format, spec->channels, spec->freq, desiredSpec->format, desiredSpec->channels, desiredSpec->freq);

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Play methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	// Else I don't do anything, it's better when the program doesn't crash when there is some bad extension by mistake
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
	this->audioPlayingLabel.text = "AUDIO OF LENGTH " + std::to_string(audioLenInSecs) + " SECONDS IS BEING PLAYED";
	this->audioPlayingLabel.hasFocus = true;
	this->resizeTextboxes();
	this->drawTextboxes();

	SDL_FreeWAV(buf);
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
						if (keyID >= static_cast<int>(this->keys.size()) || keyID < 0) {
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
	std::sort(events.begin(), events.end(), SDL_KeyboardEventComparator);			// TODO: Not sure if there should be SDL_KeyboardEventComparator or &SDL_KeyboardEventComparator
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Record methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

void Keyboard::addToRecordedKeys(Key *key, Uint32 timestamp, KeyEventType keyET) {
	if (isRecording) {
		timestamp -= this->recordStartTime;
		TimestampAndID tsid(timestamp, key->ID + 1, keyET);
		this->recordedKeys.push_back(tsid);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Create audio files methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
void Keyboard::createWavFile(const std::string &path, std::vector<Uint8> audioSamples, const SDL_AudioSpec &audioSpecWav) {
	std::ofstream f(path + ".wav", std::ios::binary);

	// Write the file headers
	f << "RIFF----WAVEfmt ";						// (chunk size to be filled in later)
	write_word(f, 16, 4);							// no extension data ... chunk size
	write_word(f, 1, 2);							// PCM format is under number 1 - integer samples
	write_word(f, audioSpecWav.channels, 2);	// number of channels (stereo file)
	write_word(f, audioSpecWav.freq, 4);		// samples per second (Hz)

	// Byte size of one second is calculated as (Sample Rate * BitsPerSample * Channels) / 8
	int byteSizeOfOneSecond = (audioSpecWav.freq *  SDL_AUDIO_BITSIZE(audioSpecWav.format) * audioSpecWav.channels) / 8;
	write_word(f, byteSizeOfOneSecond, 4);
	size_t frameLengthInBytes = SDL_AUDIO_BITSIZE(audioSpecWav.format) / 8 * audioSpecWav.channels;
	write_word(f, frameLengthInBytes, 2);  // data block size ... size of audio frame in bytes
	size_t sampleSizeInBits = SDL_AUDIO_BITSIZE(audioSpecWav.format);
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
		for (size_t j = 0; j < audioSpecWav.channels; j++) {
			size_t sampleIndex = i;
			for (size_t k = 0; k < sampleSizeInBytes; k++) {
				if (SDL_AUDIO_ISBIGENDIAN(audioSpecWav.format)) {		// Big endian, it is needed to write bytes in opposite direction
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