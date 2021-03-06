#pragma once
#include <string>
#include "Key.h"
#include "SDL.h"
#include "SDL_ttf.h"


class Keyboard;		// Forward declaration


class Label {
public:
	Label();
	Label(const std::string &text);

	static constexpr size_t DEFAULT_FONT_SIZE = 26;

	TTF_Font *font;
	size_t fontSize;
	std::string text;
	Button button;
	SDL_Rect textRectangle;	
	bool hasFocus = false;
	SDL_Texture *oldTexture = nullptr;
	SDL_Texture *newTexture = nullptr;


	// Draws the text on the renderer with the color, respectively it just prepares for the drawing
	// It will be drawn when SDL_RenderPresent is called.
	void drawText(SDL_Renderer *renderer, SDL_Color color);

	// Same as draw text, but also "draws" the button (rectangle) of the text with color based on the focus of textbox,
	void drawTextWithBackground(SDL_Renderer *renderer, SDL_Color color, SDL_Color bgHasFocusColor, SDL_Color bgHasNotFocusColor);

	// Finds and sets the font in which the textWidth (Height) fits within the given limits
	// The textWidth and textHeight are set to corresponding values of given font.
	void findFittingFont(int *textWidth, int *textHeight, int widthLimit, int heightLimit);

	// Checks if the given text fits within the limits
	// The textWidth and textHeight are set to corresponding values of given font. 
	// Returns -1 if the text height is bigger than the heightLimit, -2 if width is bigger than the widthLimit
	// Else returns 0
	int testTextSize(const std::string &text, TTF_Font *font, int *textWidth, int *textHeight, int widthLimit, int heightLimit);

	// Finds font and sets the sizes of text and button
	void setSizesBasedOnButtonSize(int textX, int textY, int buttonX, int buttonY, int buttonW, int buttonH);

	// Sets the sizes of text and button
	constexpr void setSizes(int textX, int textY, int textW, int textH, int buttonX, int buttonY, int buttonW, int buttonH);

	constexpr void moveTexture();

	// Free label resources
	virtual void freeResources();
};

class Textbox : public Label {
public:
	using Label::Label;


	// Returns true if the text should be rendered
	// Changes the enterPressed variable to true if enter was pressed
	bool processKeyEvent(const SDL_Event &event, bool *enterPressed, bool *shouldResizeTextbox);


	virtual void processEnterPressedEvent(Keyboard &keyboard);
	virtual void drawTextbox(SDL_Renderer *renderer);
};


class ConfigFileTextbox : public Textbox {
public:
	using Textbox::Textbox;


	void processEnterPressedEvent(Keyboard &keyboard);
	void drawTextbox(SDL_Renderer *renderer);
};

class DirectoryWithFilesTextbox : public Textbox {
public:
	using Textbox::Textbox;


	void processEnterPressedEvent(Keyboard &keyboard);
	void drawTextbox(SDL_Renderer *renderer);
};

class RecordFilePathTextbox : public Textbox {
public:
	using Textbox::Textbox;


	void processEnterPressedEvent(Keyboard &keyboard);
	void drawTextbox(SDL_Renderer *renderer);
};

class PlayFileTextbox : public Textbox {
public:
	using Textbox::Textbox;


	void processEnterPressedEvent(Keyboard &keyboard);
	void drawTextbox(SDL_Renderer *renderer);
};
