// cppGenKodKeKlavesam.cpp : Tento soubor obsahuje funkci main. Provádění programu se tam zahajuje a ukončuje.
//

#include "pch.h"
#include <iostream>

#define MAX_KEYS 88
#define ALPHABET_LEN 26
#define NUM_LEN 10
#define ALFANUM_LEN (ALPHABET_LEN + NUM_LEN)
#define F_KEY_LEN 11
#define ALFANUM_AND_F_LEN (ALFANUM_LEN + F_KEY_LEN)
#define ALFANUM_AND_F_AND_NUMPAD_LEN (ALFANUM_LEN + F_KEY_LEN + NUM_LEN)			// PUT INTO BRACKETS !!!!!!!!

int main()
{
	char lowerCaseChar = 'a';
	char upperCaseChar = 'A';
	char num = '1';
	char numNP = '0';
	int currentMod = -1;
	char fkey = '1';
	char fkey2 = '0';
	bool isOneDigitFKey = true;
	for (int i = 0; i < MAX_KEYS; i++) {
		if (lowerCaseChar > 'z') {
			lowerCaseChar = 'a';
			upperCaseChar = 'A';
		}
		/*if (i % ALFANUM_AND_F_LEN_AND_NUMPAD == 0) {
			num = '1';
		}*/
		if (i % ALFANUM_AND_F_AND_NUMPAD_LEN == 0) {
			fkey = '1';
			fkey2 = '0';
			isOneDigitFKey = true;
			currentMod++;
		}

		std::cout << "case " << i << ":" << std::endl;
		if (currentMod == 0)
			std::cout << "this->keys[currKey].keysym.mod = KMOD_NUM;" << std::endl;
		else if (currentMod == 1)
			std::cout << "this->keys[currKey].keysym.mod = KMOD_NONE;" << std::endl;
		else
			std::cout << "this->keys[currKey].keysym.mod = KMOD_CAPS;" << std::endl;

		if (i % ALFANUM_AND_F_AND_NUMPAD_LEN < ALPHABET_LEN) {
			std::cout << "this->keys[currKey].keysym.scancode = SDL_SCANCODE_" << upperCaseChar << ";" << std::endl;
			std::cout << "this->keys[currKey].keysym.sym = SDLK_" << lowerCaseChar << ";" << std::endl;
			upperCaseChar++;
			lowerCaseChar++;
		}
		else if (i % ALFANUM_AND_F_AND_NUMPAD_LEN < ALFANUM_LEN) {
			std::cout << "this->keys[currKey].keysym.scancode = SDL_SCANCODE_" << num << ";" << std::endl;
			std::cout << "this->keys[currKey].keysym.sym = SDLK_" << num << ";" << std::endl;
			num++;
			if (num > '9') {
				num = '0';
			}
		}
		else if (i % ALFANUM_AND_F_AND_NUMPAD_LEN < ALFANUM_AND_F_LEN) {
			if (isOneDigitFKey) {
				std::cout << "this->keys[currKey].keysym.scancode = SDL_SCANCODE_F" << fkey << ";" << std::endl;
				std::cout << "this->keys[currKey].keysym.sym = SDLK_F" << fkey << ";" << std::endl;
				fkey++;
				if (fkey > '9') {
					fkey = '1';
					isOneDigitFKey = false;
				}
			}
			else {
				std::cout << "this->keys[currKey].keysym.scancode = SDL_SCANCODE_F" << fkey << fkey2 << ";" << std::endl;
				std::cout << "this->keys[currKey].keysym.sym = SDLK_F" << fkey << fkey2 << ";" << std::endl;
				fkey2++;
			}
		}
		else {
			std::cout << "this->keys[currKey].keysym.scancode = SDL_SCANCODE_KP_" << numNP << ";" << std::endl;
			std::cout << "this->keys[currKey].keysym.sym = SDLK_KP_" << numNP << ";" << std::endl;
			numNP++;
			if (numNP > '9') {
				numNP = '0';
			}
		}
		std::cout << "break;" << std::endl;


	}

	std::cout << "default:" << std::endl;
	std::cout << "throw new std::exception(\"Trying to process non-existing key\");" << std::endl;
}

// Spuštění programu: Ctrl+F5 nebo nabídka Ladit > Spustit bez ladění
// Ladění programu: F5 nebo nabídka Ladit > Spustit ladění

// Tipy pro zahájení práce:
//   1. K přidání nebo správě souborů použijte okno Průzkumník řešení.
//   2. Pro připojení ke správě zdrojového kódu použijte okno Team Explorer.
//   3. K zobrazení výstupu sestavení a dalších zpráv použijte okno Výstup.
//   4. K zobrazení chyb použijte okno Seznam chyb.
//   5. Pokud chcete vytvořit nové soubory kódu, přejděte na Projekt > Přidat novou položku. Pokud chcete přidat do projektu existující soubory kódu, přejděte na Projekt > Přidat existující položku.
//   6. Pokud budete chtít v budoucnu znovu otevřít tento projekt, přejděte na Soubor > Otevřít > Projekt a vyberte příslušný soubor .sln.
