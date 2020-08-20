#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <filesystem>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
//#include <SDL_gpu.h>
//#include <SFML/Network.hpp>
//#include <SFML/Window.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <functional>
#ifdef __ANDROID__
#include <android/log.h> //__android_log_print(ANDROID_LOG_VERBOSE, "GuiMaker", "Example number log: %d", number);
#include <jni.h>
#endif
#if defined(__ANDROID__) || defined(__APPLE__)
#include "vendor/PUGIXML/src/pugixml.hpp"
#else
#include <pugixml.hpp>
#endif

// NOTE: Remember to uncomment it on every release
//#define RELEASE

#if defined _MSC_VER && defined RELEASE
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

//240 x 240 (smart watch)
//240 x 320 (QVGA)
//360 x 640 (Galaxy S5)
//640 x 480 (480i - Smallest PC monitor)

int windowWidth = 240;
int windowHeight = 320;
SDL_Point mousePos;
SDL_Point realMousePos;
bool keys[SDL_NUM_SCANCODES];
bool buttons[SDL_BUTTON_X2 + 1];
std::string prefPath;

#define BG_COLOR 45, 46, 47, 0

void logOutputCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
	std::cout << message << std::endl;
}

int random(int min, int max)
{
	return min + rand() % ((max + 1) - min);
}

int SDL_QueryTextureF(SDL_Texture* texture, Uint32* format, int* access, float* w, float* h)
{
	int wi, hi;
	int result = SDL_QueryTexture(texture, format, access, &wi, &hi);
	*w = wi;
	*h = hi;
	return result;
}

void clearTexture(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Color color)
{
	SDL_Texture* currT = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, texture);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderClear(renderer);
	//SDL_RenderPresent(renderer); // TOOD: Does it causes flicker on android?
	SDL_SetRenderTarget(renderer, currT);
}

SDL_Texture* createClearedTexture(SDL_Renderer* renderer, Uint32 format, int access, int width, int height, SDL_Color color)
{
	SDL_Texture* texture = SDL_CreateTexture(renderer, format, access, width, height);
	clearTexture(renderer, texture, color);
	return texture;
}

SDL_bool SDL_PointInFRect(const SDL_Point* p, const SDL_FRect* r)
{
	return ((p->x >= r->x) && (p->x < (r->x + r->w)) &&
		(p->y >= r->y) && (p->y < (r->y + r->h))) ? SDL_TRUE : SDL_FALSE;
}

SDL_Texture* renderText(SDL_Texture* previousTexture, TTF_Font* font, SDL_Renderer* renderer, const std::string& text, SDL_Color color)
{
	if (previousTexture) {
		SDL_DestroyTexture(previousTexture);
	}
	SDL_Surface* surface;
	if (text.empty()) {
		surface = TTF_RenderUTF8_Blended(font, " ", color);
	}
	else {
		surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
	}
	if (surface) {
		SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_FreeSurface(surface);
		return texture;
	}
	else {
		return 0;
	}
}

struct Text {
	std::string text;
	SDL_Texture* t = 0;
	SDL_FRect dstR{};
	float wMultiplier = 1;
	float hMultiplier = 1;
	std::function<void()> onClickCallback;
	std::string fontStr;

	void handleEvent(SDL_Event event)
	{
		if (event.type == SDL_MOUSEBUTTONDOWN && SDL_PointInFRect(&mousePos, &dstR)) {
			if (onClickCallback) {
				onClickCallback();
			}
		}
	}

	void setText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color c = { 255,255,255 })
	{
		this->text = text;
		t = renderText(t, font, renderer, text, c);
	}

	void setText(SDL_Renderer* renderer, TTF_Font* font, int value, SDL_Color c = { 255,255,255 })
	{
		setText(renderer, font, std::to_string(value), c);
	}

	void adjustSize()
	{
		adjustSize(wMultiplier, hMultiplier);
	}

	void adjustSize(float wMultiplier, float hMultiplier)
	{
		float w, h;
		SDL_QueryTextureF(t, 0, 0, &w, &h);
		dstR.w = w * wMultiplier;
		dstR.h = h * hMultiplier;
	}

	void draw(SDL_Renderer* renderer)
	{
		SDL_RenderCopyF(renderer, t, 0, &dstR);
	}
};

struct Image {
	SDL_Texture* t = 0; // WARNING: Remember to destory old one on every change if it's the only pointer to it
	SDL_FRect dstR{};
	std::string srcStr;

	void draw(SDL_Renderer* renderer)
	{
		SDL_RenderCopyF(renderer, t, 0, &dstR);
	}
};

struct Button {
	SDL_FRect dstR{};
	std::function<void()> onClickCallback;

	void handleEvent(SDL_Event event)
	{
		if (event.type == SDL_MOUSEBUTTONDOWN && SDL_PointInFRect(&mousePos, &dstR)) {
			if (onClickCallback) {
				onClickCallback();
			}
		}
	}

	void draw(SDL_Renderer* renderer)
	{
		SDL_RenderFillRectF(renderer, &dstR);
	}
};

/*

- load
- handle events
- update
- draw
- save

*/

enum class Type {
	None,
	Text,
	Image,
	Button,
};

struct HotWidget {
	Type type = Type::None;
	int selected = -1;
	SDL_Point moveHitP{};
	int lastSelected = -1;
	bool resizing = false;
	SDL_Rect lastSelectedResizeR{};
	SDL_Point resizeHitP{};
};

// WARNING: Remember to run ResScanner before using Ui
std::string resList =
#include "resList.h"
;

struct Widgets {
	std::vector<Text> texts;
	std::vector<Image> images;
	SDL_Rect r;
};

struct Ui {
	enum class State {
		Main,
		ImagePicker,
		TextActionPicker,
		ChangeTextFont,
		UiPicker,
	};
	State state = State::Main;

	std::vector<std::string> resources;
	std::unordered_map<std::string, Image> textures;
	std::unordered_map<std::string, TTF_Font*> fonts;

	HotWidget hotW;

	int currWidgets = 0;
	std::vector<Widgets> widgets;
	SDL_Rect addUiR{};

	SDL_Rect addTextR{};
	int editedText = -1;
	int lastSelectedTextTicks = 0;
	int actionPickerTextIndex = -1;
	Text editTextTxt;
	Text changeFontTxt;
	std::vector<Text> fontTxts;

	SDL_Rect addImgR{};
	int editedImage = -1;
	int lastSelectedImageTicks = 0;

	SDL_Rect uiPickerR{};

	void set(int i, Type type, SDL_FRect dstR)
	{
		hotW.selected = i;
		hotW.lastSelected = i;
		hotW.lastSelectedResizeR.w = 16;
		hotW.lastSelectedResizeR.h = 16;
		hotW.lastSelectedResizeR.x = dstR.x + dstR.w;
		hotW.lastSelectedResizeR.y = dstR.y + dstR.h;
		hotW.moveHitP.x = mousePos.x - dstR.x;
		hotW.moveHitP.y = mousePos.y - dstR.y;
		hotW.type = type;
	}

	Ui(SDL_Renderer* renderer, TTF_Font* robotoF)
	{
		std::stringstream ss(resList);
		std::string line;
		while (std::getline(ss, line)) {
			resources.push_back(line);
		}
		for (std::string resource : resources) {
			int lastDot = resource.find_last_of(".");
			if (lastDot != std::string::npos) {
				std::string extension = resource.substr(lastDot);
				if (extension == ".png" || extension == ".jpg" || extension == ".bmp") {
					Image image;
					image.t = IMG_LoadTexture(renderer, resource.c_str());
					image.srcStr = resource;
					textures[resource] = image;
				}
				if (extension == ".ttf") {
					fonts[resource] = TTF_OpenFont(resource.c_str(), 72); // TODO: Allow to set font size?
					fontTxts.push_back(Text());
					fontTxts.back().setText(renderer, fonts[resource], resource);
					fontTxts.back().adjustSize(0.3, 0.3);
					fontTxts.back().dstR.y = fontTxts.size() <= 1 ? 0 : fontTxts[fontTxts.size() - 2].dstR.y + fontTxts[fontTxts.size() - 2].dstR.h + 2;
				}
			}
		}

		addUiR.w = 80;
		addUiR.h = 80;
		addUiR.x = 0;
		addUiR.y = 0;

		addTextR.w = 32;
		addTextR.h = 32;
		addTextR.x = 0;
		addTextR.y = 0;
		addImgR = addTextR;
		addImgR.y += addImgR.h;
		uiPickerR = addImgR;
		uiPickerR.y += uiPickerR.h;

		editTextTxt.setText(renderer, robotoF, "Edit text");
		editTextTxt.adjustSize(0.5, 0.5);
		editTextTxt.onClickCallback = [&] {
			editedText = actionPickerTextIndex;
			SDL_StartTextInput();
			std::cout << "editedText" << std::endl;
			state = State::Main;
		};
		changeFontTxt.setText(renderer, robotoF, "Change font");
		changeFontTxt.adjustSize(0.5, 0.5);
		changeFontTxt.dstR.y = editTextTxt.dstR.y + editTextTxt.dstR.h + 21;
		changeFontTxt.onClickCallback = [&] {
			state = State::ChangeTextFont;
		};
	}

	void save(std::string path)
	{
		pugi::xml_document doc;
		pugi::xml_node rootNode = doc.append_child("root");
		for (Widgets& w : widgets) {
			pugi::xml_node uiNode = rootNode.append_child("ui");
			pugi::xml_node textsNode = uiNode.append_child("texts");
			for (Text& text : w.texts) {
				pugi::xml_node textNode = textsNode.append_child("text");
				textNode.append_attribute("x").set_value(text.dstR.x);
				textNode.append_attribute("y").set_value(text.dstR.y);
				textNode.append_attribute("w").set_value(text.dstR.w);
				textNode.append_attribute("h").set_value(text.dstR.h);
				textNode.append_attribute("value").set_value(text.text.c_str());
				textNode.append_attribute("font").set_value(text.fontStr.c_str());
			}
			pugi::xml_node imagesNode = uiNode.append_child("images");
			for (Image& image : w.images) {
				pugi::xml_node imageNode = imagesNode.append_child("image");
				imageNode.append_attribute("x").set_value(image.dstR.x);
				imageNode.append_attribute("y").set_value(image.dstR.y);
				imageNode.append_attribute("w").set_value(image.dstR.w);
				imageNode.append_attribute("h").set_value(image.dstR.h);
				imageNode.append_attribute("src").set_value(image.srcStr.c_str());
			}
		}
		doc.save_file(path.c_str());
	}

	void load(std::string path, SDL_Renderer* renderer)
	{
#if 1
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		auto uiNodes = doc.child("root").children("ui");
		for (pugi::xml_node uiNode : uiNodes) {
			widgets.push_back(Widgets());
			auto textNodes = uiNode.child("texts").children("text");
			for (pugi::xml_node textNode : textNodes) {
				widgets.back().texts.push_back(Text());
				widgets.back().texts.back().fontStr = textNode.attribute("font").as_string();
				widgets.back().texts.back().setText(renderer, fonts[widgets.back().texts.back().fontStr], textNode.attribute("value").as_string());
				widgets.back().texts.back().dstR.w = textNode.attribute("w").as_float();
				widgets.back().texts.back().dstR.h = textNode.attribute("h").as_float();
				widgets.back().texts.back().dstR.x = textNode.attribute("x").as_float();
				widgets.back().texts.back().dstR.y = textNode.attribute("y").as_float();
			}
			auto imageNodes = uiNode.child("images").children("image");
			for (pugi::xml_node imageNode : imageNodes) {
				widgets.back().images.push_back(Image());
				// TODO: What if image src is empty???
				widgets.back().images.back().srcStr = imageNode.attribute("src").as_string();
				widgets.back().images.back().t = IMG_LoadTexture(renderer, widgets.back().images.back().srcStr.c_str());
				widgets.back().images.back().dstR.w = imageNode.attribute("w").as_float();
				widgets.back().images.back().dstR.h = imageNode.attribute("h").as_float();
				widgets.back().images.back().dstR.x = imageNode.attribute("x").as_float();
				widgets.back().images.back().dstR.y = imageNode.attribute("y").as_float();
			}
		}
		if (widgets.empty()) {
			widgets.push_back(Widgets());
		}
#endif
	}

	void handleEvents(SDL_Event event, SDL_Renderer* renderer, TTF_Font* robotoF)
	{
		auto& texts = widgets[currWidgets].texts;
		auto& images = widgets[currWidgets].images;
		if (state == State::Main) {
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				SDL_StopTextInput();
				if (hotW.lastSelected != -1 && SDL_PointInRect(&mousePos, &hotW.lastSelectedResizeR)) {
					hotW.resizing = true;
					hotW.resizeHitP = mousePos;
				}
				if (hotW.lastSelected != -1 &&
					(hotW.type == Type::Text && !SDL_PointInFRect(&mousePos, &texts[hotW.lastSelected].dstR) ||
						hotW.type == Type::Image && !SDL_PointInFRect(&mousePos, &images[hotW.lastSelected].dstR)) &&
					!hotW.resizing) {
					hotW.lastSelected = -1;
				}
				{
					int i = 0;
					for (Image& img : images) {
						if (SDL_PointInFRect(&mousePos, &img.dstR)) {
							if (hotW.type == Type::Image && hotW.lastSelected == i && SDL_GetTicks() - lastSelectedImageTicks <= 500) {
								editedImage = i;
								state = State::ImagePicker;
								std::cout << "editedImage" << std::endl;
							}
							lastSelectedImageTicks = SDL_GetTicks();
							set(i, Type::Image, img.dstR);
						}
						++i;
					}
				}
				{
					int i = 0;
					for (Text& txt : texts) {
						if (SDL_PointInFRect(&mousePos, &txt.dstR)) {
							if (hotW.type == Type::Text && hotW.lastSelected == i && SDL_GetTicks() - lastSelectedTextTicks <= 500) {
								state = State::TextActionPicker;
								actionPickerTextIndex = i;
							}
							lastSelectedTextTicks = SDL_GetTicks();
							set(i, Type::Text, txt.dstR);
						}
						++i;
					}
				}
				if (SDL_PointInRect(&mousePos, &addTextR)) {
					texts.push_back(Text());
					texts.back().setText(renderer, fonts["res\\Roboto-Regular.ttf"], "TEXT");
					texts.back().fontStr = "res\\Roboto-Regular.ttf";
					texts.back().wMultiplier = 0.5;
					texts.back().hMultiplier = 0.4;
					texts.back().adjustSize();
					texts.back().dstR.x = windowWidth / 2 - texts.back().dstR.w / 2;
					texts.back().dstR.y = 0;
				}
				if (SDL_PointInRect(&mousePos, &addImgR)) {
					images.push_back(Image());
					int w = 32, h = 32;
					images.back().t = createClearedTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h, { 255,255,255 });
					images.back().dstR.w = w;
					images.back().dstR.h = h;
					images.back().dstR.x = windowWidth / 2 - images.back().dstR.w / 2;
					images.back().dstR.y = 0;
				}
				if (SDL_PointInRect(&mousePos, &uiPickerR)) {
					state = State::UiPicker;
				}
			}
			if (event.type == SDL_MOUSEBUTTONUP) {
				hotW.selected = -1;
				hotW.resizing = false;
			}
			if (event.type == SDL_MOUSEMOTION) {
				if (hotW.selected != -1) {
					if (hotW.type == Type::Text) {
						{
							int i = 0;
							for (Text& txt : texts) {
								if (i == hotW.selected) {
									txt.dstR.x = mousePos.x - hotW.moveHitP.x;
									txt.dstR.y = mousePos.y - hotW.moveHitP.y;
									hotW.lastSelectedResizeR.x = txt.dstR.x + txt.dstR.w;
									hotW.lastSelectedResizeR.y = txt.dstR.y + txt.dstR.h;
								}
								++i;
							}
						}
					}
					else if (hotW.type == Type::Image) {
						{
							int i = 0;
							for (Image& img : images) {
								if (i == hotW.selected) {
									img.dstR.x = mousePos.x - hotW.moveHitP.x;
									img.dstR.y = mousePos.y - hotW.moveHitP.y;
									hotW.lastSelectedResizeR.x = img.dstR.x + img.dstR.w;
									hotW.lastSelectedResizeR.y = img.dstR.y + img.dstR.h;
								}
								++i;
							}
						}
					}
				}
				if (hotW.resizing) {
					if (hotW.type == Type::Text) {
						texts[hotW.lastSelected].wMultiplier += (mousePos.x - hotW.resizeHitP.x) * 0.007;
						texts[hotW.lastSelected].hMultiplier += (mousePos.y - hotW.resizeHitP.y) * 0.007;
						texts[hotW.lastSelected].adjustSize();
						hotW.resizeHitP = mousePos;
						hotW.lastSelectedResizeR.x = texts[hotW.lastSelected].dstR.x + texts[hotW.lastSelected].dstR.w;
						hotW.lastSelectedResizeR.y = texts[hotW.lastSelected].dstR.y + texts[hotW.lastSelected].dstR.h;
					}
					else if (hotW.type == Type::Image) {
						images[hotW.lastSelected].dstR.w += mousePos.x - hotW.resizeHitP.x;
						images[hotW.lastSelected].dstR.h += mousePos.y - hotW.resizeHitP.y;
						hotW.resizeHitP = mousePos;
						hotW.lastSelectedResizeR.x = images[hotW.lastSelected].dstR.x + images[hotW.lastSelected].dstR.w;
						hotW.lastSelectedResizeR.y = images[hotW.lastSelected].dstR.y + images[hotW.lastSelected].dstR.h;
					}
				}
			}
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
					{
						int i = 0;
						for (Text& txt : texts) {
							if (i == editedText && !txt.text.empty()) {
								txt.text.pop_back();
								txt.setText(renderer, fonts[txt.fontStr], txt.text);
								txt.adjustSize();
								hotW.lastSelectedResizeR.x = txt.dstR.x + txt.dstR.w;
								hotW.lastSelectedResizeR.y = txt.dstR.y + txt.dstR.h;
							}
							++i;
						}
					}
				}
			}
			if (event.type == SDL_TEXTINPUT) {
				{
					int i = 0;
					for (Text& txt : texts) {
						if (i == editedText) {
							txt.setText(renderer, fonts[txt.fontStr], txt.text + event.text.text);
							txt.adjustSize();
							hotW.lastSelectedResizeR.x = txt.dstR.x + txt.dstR.w;
							hotW.lastSelectedResizeR.y = txt.dstR.y + txt.dstR.h;
						}
						++i;
					}
				}
			}
		}
		else if (state == State::ImagePicker) {
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				for (auto texture : textures) {
					if (SDL_PointInFRect(&mousePos, &texture.second.dstR)) {
						// TODO: Fix memory leak?
						images[editedImage].t = texture.second.t;
						images[editedImage].srcStr = texture.first;
						state = State::Main;
					}
				}
			}
		}
		else if (state == State::TextActionPicker) {
			editTextTxt.handleEvent(event);
			changeFontTxt.handleEvent(event);
		}
		else if (state == State::ChangeTextFont) {
			for (Text& txt : fontTxts) {
				if (event.type == SDL_MOUSEBUTTONDOWN && SDL_PointInFRect(&mousePos, &txt.dstR)) {
					texts[actionPickerTextIndex].setText(renderer, fonts[txt.text], texts[actionPickerTextIndex].text);
					texts[actionPickerTextIndex].fontStr = txt.text;
					state = State::Main;
					break;
				}
			}
		}
		else if (state == State::UiPicker) {
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				{
					int i = 0;
					for (Widgets& w : widgets) {
						if (SDL_PointInRect(&mousePos, &w.r)) {
							currWidgets = i;
							state = State::Main;
							break;
						}
						++i;
					}
				}
				if (SDL_PointInRect(&mousePos, &addUiR)) {
					widgets.push_back(Widgets());
				}
			}
		}
	}

	void update()
	{
		if (state == State::Main) {

		}
		else if (state == State::ImagePicker) {

		}
	}

	void draw(SDL_Renderer* renderer)
	{
		auto& texts = widgets[currWidgets].texts;
		auto& images = widgets[currWidgets].images;
		if (state == State::Main) {
			SDL_SetRenderDrawColor(renderer, 56, 85, 111, 0);
			SDL_RenderFillRect(renderer, &addTextR);
			SDL_SetRenderDrawColor(renderer, 0, 63, 212, 0);
			SDL_RenderFillRect(renderer, &addImgR);
			SDL_SetRenderDrawColor(renderer, 0, 25, 43, 0);
			SDL_RenderFillRect(renderer, &uiPickerR);
			{
				int i = 0;
				for (Text& txt : texts) {
					txt.draw(renderer);
					if (hotW.type == Type::Text && i == hotW.lastSelected) {
						SDL_SetRenderDrawColor(renderer, 3, 132, 252, 0);
						SDL_RenderDrawRectF(renderer, &txt.dstR);
						SDL_SetRenderDrawColor(renderer, 212, 116, 0, 0);
						SDL_RenderFillRect(renderer, &hotW.lastSelectedResizeR);
					}
					++i;
				}
			}
			{
				int i = 0;
				for (Image& img : images) {
					img.draw(renderer);
					if (hotW.type == Type::Image && i == hotW.lastSelected) {
						SDL_SetRenderDrawColor(renderer, 3, 132, 252, 0);
						SDL_RenderDrawRectF(renderer, &img.dstR);
						SDL_SetRenderDrawColor(renderer, 212, 116, 0, 0);
						SDL_RenderFillRect(renderer, &hotW.lastSelectedResizeR);
					}
					++i;
				}
			}
		}
		else if (state == State::ImagePicker) {
			{
				int i = 0;
				for (auto texture : textures) {
#if 1
					texture.second.dstR.w = 40;
					texture.second.dstR.h = 40;
#else
					texture.second.dstR.w = 80;
					texture.second.dstR.h = 80;
#endif
					texture.second.dstR.x = i % (int)(windowWidth / texture.second.dstR.w) * texture.second.dstR.w;
					texture.second.dstR.y = i / (int)(windowWidth / texture.second.dstR.w) * texture.second.dstR.h;
					// TODO: add scroll
					SDL_RenderCopyF(renderer, texture.second.t, 0, &texture.second.dstR);
					textures[texture.first] = texture.second;
					++i;
				}
			}
		}
		else if (state == State::TextActionPicker) {
			editTextTxt.draw(renderer);
			changeFontTxt.draw(renderer);
		}
		else if (state == State::ChangeTextFont) {
			for (Text& txt : fontTxts) {
				txt.draw(renderer);
			}
		}
		else if (state == State::UiPicker) {
			std::vector<SDL_Texture*> uiTextures;
			{
				int i = 0;
				for (Widgets& w : widgets) {
					uiTextures.push_back(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight));
					SDL_SetRenderTarget(renderer, uiTextures.back());
					SDL_RenderClear(renderer);
					for (Text& txt : w.texts) {
						txt.draw(renderer);
					}
					for (Image& image : w.images) {
						image.draw(renderer);
					}
					w.r.w = 80;
					w.r.h = 80;
					w.r.x = (i + 1) % (int)(windowWidth / w.r.w) * w.r.w;
					w.r.y = (i + 1) / (int)(windowWidth / w.r.w) * w.r.h;
					++i;
				}
			}
			SDL_SetRenderTarget(renderer, 0);
			for (int i = 0; i < uiTextures.size(); ++i) {
				// TODO: add scroll
				SDL_SetRenderDrawColor(renderer, 125, 125, 125, 0);
				SDL_RenderCopy(renderer, uiTextures[i], 0, &widgets[i].r);
				SDL_RenderDrawRect(renderer, &widgets[i].r);
			}
			SDL_SetRenderDrawColor(renderer, 0, 125, 0, 0);
			SDL_RenderFillRect(renderer, &addUiR);
			// TODO: Cleanup
		}
	}
};

int main(int argc, char* argv[])
{
	std::srand(std::time(0));
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
	SDL_LogSetOutputFunction(logOutputCallback, 0);
	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();
	SDL_GetMouseState(&mousePos.x, &mousePos.y);
	prefPath = SDL_GetPrefPath("NextCodeApps", "GuiMaker");
	SDL_Window* window = SDL_CreateWindow("GuiMaker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	TTF_Font* robotoF = TTF_OpenFont("res/Roboto-Regular.ttf", 72);
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	SDL_RenderSetScale(renderer, w / (float)windowWidth, h / (float)windowHeight);
	bool running = true;
	Ui ui(renderer, robotoF);
	ui.load(prefPath + "data.xml", renderer);
	while (running) {
#if 1 // EVENT_HANDLING
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
				running = false;
			}
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
			}
			if (event.type == SDL_KEYDOWN) {
				keys[event.key.keysym.scancode] = true;
			}
			if (event.type == SDL_KEYUP) {
				keys[event.key.keysym.scancode] = false;
			}
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				buttons[event.button.button] = true;
			}
			if (event.type == SDL_MOUSEBUTTONUP) {
				buttons[event.button.button] = false;
			}
			if (event.type == SDL_MOUSEMOTION) {
				float scaleX, scaleY;
				SDL_RenderGetScale(renderer, &scaleX, &scaleY);
				mousePos.x = event.motion.x / scaleX;
				mousePos.y = event.motion.y / scaleY;
				realMousePos.x = event.motion.x;
				realMousePos.y = event.motion.y;
			}
			ui.handleEvents(event, renderer, robotoF);
#endif
#if 1 // UPDATING
			ui.update();
#endif
#if 1 // DRAWING
			SDL_SetRenderDrawColor(renderer, BG_COLOR);
			SDL_RenderClear(renderer);
			ui.draw(renderer);
			SDL_RenderPresent(renderer);
#endif
		}
	}
	ui.save(prefPath + "data.xml");
	return 0;
}