#pragma once
#include <string>
#include "Key.h"
#include "SDL.h"
#include "SDL_ttf.h"

#define DEFAULT_FONT_SIZE 26

// TODO: In future maybe have option to set font, for this to be possible we will need to look for the ttf files in the OS specific directories
class Label {
public:
	Label();
	Label(const std::string &text);

	TTF_Font *font;
	int fontSize;
	std::string text;
	Button button;
	SDL_Rect textRectangle;	
	bool hasFocus = false;

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
	void setSizes(int textX, int textY, int textW, int textH, int buttonX, int buttonY, int buttonW, int buttonH);

	// Free label resources
	void freeLabel();
};

class Textbox : public Label {
public:
	Textbox();
	Textbox(const std::string &text);

	// Returns true if the text should be rendered
	// Changes the enterPressed variable to true if enter was pressed
	bool processKeyEvent(const SDL_Event &event, bool *enterPressed, bool *shouldResizeTextbox);
	
	// Free textbox resources
	void freeTextbox();
};
