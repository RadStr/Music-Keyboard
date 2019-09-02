#include "Keyboard.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Constructor
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Keyboard::Keyboard(SDL_Window *window, SDL_Renderer *renderer) {
	std::ofstream& logger = LoggerClass::getLogger();

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// 
///////		Init methods
//////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int Keyboard::initKeyboard(const std::string &configFilename, size_t *totalLinesInFile) {
	if (configFilename == "") {
		return Keyboard::defaultInit(MAX_KEYS, true);
	}

	return this->readConfigfile(totalLinesInFile);
}

int Keyboard::defaultInit(size_t totalKeys, bool initAudioBuffer) {
	size_t currKey = 0;
	int currX = 0;

	if (this->keys.size() != totalKeys) {
		freeKeys();
		this->keys.resize(totalKeys);

		for (size_t i = 0; i < this->keys.size(); i++) {
			keys[i].initKey();
		}
	}
	else {
		for (size_t i = 0; i < this->keys.size(); i++) {
			keys[i].freeKey();
		}
	}

	SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);
	this->setBlackAndWhiteKeysCounts();
	this->setKeySizes();
	setRenderer();
	this->recordKey.resizeButton(0, 0, this->whiteKeyWidth, 3 * this->blackKeyHeight / 4);
	this->recordKey.ID = 0;
	this->resizeTextboxes();

	size_t li = this->keys.size() - 1;		// Last index
	if (isWhiteKey(li)) {				// If the last key is black then the boundary is a bit different
		this->isLastKeyBlack = false;
	}
	else {
		this->isLastKeyBlack = true;
	}

	for (currKey = 0; currKey < this->keys.size(); currKey++) {
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
			throw std::runtime_error("Same formats aren't the same ... Shouldn't happen");
		}
		if (initAudioBuffer) {
			initKeyWithDefaultAudioNonStatic(&this->keys[currKey]);
		}
	}

	drawWindow();
	return 0;
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
	this->audioDevID = SDL_OpenAudioDevice(nullptr, 0, &this->audioSpec, &this->audioSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
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


void Keyboard::initKeyWithDefaultAudioStatic(Key *key, const SDL_AudioSpec &audioSpecLocal) {
	constexpr Uint32 numberOfSeconds = 10;
	if (key != nullptr) {
		key->freeKey();
		generateTone(audioSpecLocal, static_cast<int>(key->ID), numberOfSeconds, &key->audioConvert);
	}
}


/////////////////////////////////////////////////// Last method in this file:
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