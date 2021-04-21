/*
Copyright (C) 2017-2021
Martijn Braam, Clayton Craft <clayton@craftyguy.net>, et al.

This file is part of osk-sdl.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "draw_helpers.h"
#include "keyboard.h"
#include "tooltip.h"
#include "util.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>
#include <string>
#include <unistd.h>
#include <chrono>

Uint32 EVENT_RENDER;
char boxString[100] = "Enter Text";

//Game Controller 1 handler
SDL_Joystick* gGameController = NULL;

int main(int argc, char* argv[])
{
	std::vector<std::string> textinput;

	Config config;
	SDL_Event event;
	SDL_Window *display = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Haptic *haptic = nullptr;
	std::chrono::milliseconds repeat_delay { 25 }; // Keyboard key repeat rate in ms

	int w, h;

	bool typePass = false;
	bool showPass = false;

	static SDL_Event renderEvent {
		.type = EVENT_RENDER
	};

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

	if (argc >= 2) {
		if (strcmp(argv[1], "-p") == 0) {
			typePass = true;
		}
		else if (strcmp(argv[1], "-t") == 0) {
			typePass = false;
		}
		else if (strcmp(argv[1], "-v") == 0) {
			SDL_Log("osk-sdl v%s", VERSION);
			exit(0);
		}

		if (argc >= 3) {
			strcpy(boxString, argv[2]);
		}
	}

	SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "osk-sdl v%s", VERSION);

	atexit(SDL_Quit);
	Uint32 sdlFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC;

	if (SDL_Init(sdlFlags) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Init failed: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// check for joysticks
	if (SDL_NumJoysticks() < 1) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No joysticks connected: %s", SDL_GetError());
	}
	else {
		// load joystick
		gGameController = SDL_JoystickOpen( 0 );
		if (gGameController == NULL) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Unable to open game controller: %s", SDL_GetError());
		}
	}

	/*
	 * Set up display and renderer
	 * Use device resolution
	 */
	Uint32 windowFlags = SDL_WINDOW_FULLSCREEN;

	display = SDL_CreateWindow("OSK SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, windowFlags);

	if (display == nullptr) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create window/display: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/*
	  * Prefer using GLES, since it's better supported on mobile devices
	  * than full GL.
	  * NOTE: DirectFB's SW GLES implementation is broken, so don't try to
	  * use GLES w/ DirectFB
	  */
	int rendererIndex = find_gles_driver_index();
	renderer = SDL_CreateRenderer(display, rendererIndex, 0);

	if (renderer == nullptr) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// get width / height
	SDL_GetRendererOutputSize(renderer, &w, &h);

	int keyboardHeight = h / 3 * 2;
	if (h > w) {
		// Keyboard height is screen width / max number of keys per row * rows
		// Denominator below chosen to provide enough room for a 5 row layout without causing key height to
		// shrink too much
		keyboardHeight = w / 1.6;
	}

	int inputWidth = static_cast<int>(w * 0.9);
	int inputHeight = static_cast<int>(w * 0.1);

	if (SDL_SetRenderDrawColor(renderer, 255, 128, 0, SDL_ALPHA_OPAQUE) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not set background color: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (SDL_RenderFillRect(renderer, nullptr) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not fill background color: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Initialize haptic device
	if (SDL_WasInit(SDL_INIT_HAPTIC) != 0) {
		haptic = SDL_HapticOpen(0);
		if (haptic == nullptr) {
			SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Unable to open haptic device");
		} else if (SDL_HapticRumbleInit(haptic) != 0) {
			SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Unable to initialize haptic device");
			SDL_HapticClose(haptic);
			haptic = nullptr;
		} else {
			SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Initialized haptic device");
		}
	}

	// Initialize virtual keyboard
	Keyboard keyboard(0, 1, w, keyboardHeight, &config, haptic);
	if (keyboard.init(renderer)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to initialize keyboard!");
		exit(EXIT_FAILURE);
	}

	// Make SDL send text editing events for textboxes
	SDL_StartTextInput();

	SDL_Surface *wallpaper = make_wallpaper(&config, w, h);
	SDL_Texture *wallpaperTexture = SDL_CreateTextureFromSurface(renderer, wallpaper);

	int inputBoxRadius = std::strtol(config.inputBoxRadius.c_str(), nullptr, 10);
	if (inputBoxRadius >= BEZIER_RESOLUTION || inputBoxRadius > inputHeight / 1.5) {
		SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "inputbox-radius must be below %d and %f, it is %d", BEZIER_RESOLUTION,
			inputHeight / 1.5, inputBoxRadius);
		inputBoxRadius = 0;
	}

	Tooltip enterTextTooltip(TooltipType::info, inputWidth, inputHeight, inputBoxRadius, &config);
	if (enterTextTooltip.init(renderer, boxString)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to initialize enterTextTooltip!");
		exit(EXIT_FAILURE);
	}

	argb inputBoxColor = config.inputBoxBackground;

	SDL_Surface *inputBox = make_input_box(inputWidth, inputHeight, &inputBoxColor, inputBoxRadius);
	SDL_Texture *inputBoxTexture = SDL_CreateTextureFromSurface(renderer, inputBox);

	int topHalf = static_cast<int>(h - (keyboard.getHeight() * keyboard.getPosition()));
	SDL_Rect inputBoxRect = SDL_Rect {
		.x = w / 20,
		.y = static_cast<int>(topHalf / 3.5),
		.w = inputWidth,
		.h = inputHeight
	};

	if (inputBoxTexture == nullptr) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create input box texture: %s",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(renderer, &rendererInfo);

	// Start drawing keyboard when main loop starts
	SDL_PushEvent(&renderEvent);

	unsigned int cur_ticks = 0, Elapsed_Time;

	int locx = 47;
	int locy = 137;
	int keymargin = 47;

	// Process incoming SDL events
	bool done = false;
	while (!done) {	
		// Keep the time corresponding to the frame rendering beginning
		cur_ticks = SDL_GetTicks();

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_JOYHATMOTION) {
				// d-pad
				switch (event.jhat.value) {
					case 1: { // up
						if (locy >= 165) {
							locy -= 47;
						}
						else {
							locy = (h - 47);
						}
						break;
					}

					case 4: { // down
						if (locy < (h - 47)) {
							locy += 47;
						}
						else {
							locy = 137;
						}
						break;
					}

					case 8: { // left
						if (locx >= 90) {
							locx -= keymargin;
						}
						else {
							locx = (w - 47);
						}
						break;
					}

					case 2: { // right
						if (locx < (w - 47)) {
							locx += keymargin;
						}
						else {
							locx = keymargin;
						}
						break;
					}
				}

				//printf("x: %i  y: %i\n", locx, locy);

				keymargin = handleTapBegin(locx, locy, h, keyboard);
				SDL_PushEvent(&renderEvent);
			}

			if (event.type == SDL_JOYBUTTONDOWN) {
				switch (event.jbutton.button) {
					case 0: { // a
						if (handleTapEnd(locx, locy, h, keyboard, textinput)) {
							goto QUIT;
						}
						break;
					}

					case 1: { // b
						// space
						textinput.emplace_back(" ");
						break;
					}

					case 2: { // x
						setBackspace(textinput);
						break;
					}

					case 3: { // y
						// shift
						setCapslock(keyboard);
						break;
					}

					case 5: { // r1
						showPass = !showPass;
						break;
					}

					case 6: { // start
						goto QUIT;
						break;
					}

					case 7: { // select
						exit(0);
						break;
					}
				}

				SDL_PushEvent(&renderEvent);
			}

			// Render event handler
			if (event.type == EVENT_RENDER) {
				/* NOTE ON MULTI BUFFERING / RENDERING MULTIPLE TIMES:
				   We only re-schedule render events during animation, otherwise
				   we render once and then do nothing for a long while.

				   A single render may however never reach the screen, since
				   SDL_RenderCopy() page flips and with multi buffering that
				   may just fill the hidden backbuffer(s).

				   Therefore, we need to render multiple times if not during
				   animation to make sure it actually shows on screen during
				   lengthy pauses.

				   For software rendering (directfb backend), rendering twice
				   seems to be the sweet spot.

				   For accelerated rendering, we render 3 times to make sure
				   updates show on screen for drivers that use
				   triple buffering
				 */
				int render_times = 0;
				int max_render_times = (rendererInfo.flags & SDL_RENDERER_ACCELERATED) ? 3 : 2;
				while (render_times < max_render_times) {
					render_times++;
					SDL_RenderCopy(renderer, wallpaperTexture, nullptr, nullptr);

					topHalf = static_cast<int>(h - (keyboard.getHeight() * keyboard.getPosition()));
					inputBoxRect.y = static_cast<int>(topHalf / 3.5);

					SDL_RenderCopy(renderer, inputBoxTexture, nullptr, &inputBoxRect);

					if (textinput.size() <= 0) {
						enterTextTooltip.init(renderer, boxString);
						enterTextTooltip.draw(renderer, inputBoxRect.x, inputBoxRect.y);
					}
					else {
						if (!showPass && typePass) {
							draw_password_box_dots(renderer, &config, inputBoxRect, textinput.size());
						}

						if (!typePass || showPass) {
							std::string txt = strVector2str(textinput);
							enterTextTooltip.init(renderer, txt);
							enterTextTooltip.draw(renderer, inputBoxRect.x, inputBoxRect.y);
						}
					}

					// Draw keyboard last so that key previews don't get drawn over by e.g. the input box
					keyboard.draw(renderer, h);
					SDL_RenderPresent(renderer);
					if (keyboard.isInSlideAnimation()) {
						// No need to double-flip if we'll redraw more for animation
						// in a tiny moment anyway.
						break;
					}
				}

				// If any animations are running, continue to push render events to the
				// event queue
				if (keyboard.isInSlideAnimation()) {
					SDL_PushEvent(&renderEvent);
				}
			}			
		}

		// Wait enough time to get a 60Hz refresh rate
		Elapsed_Time = SDL_GetTicks() - cur_ticks;
		if (Elapsed_Time < 16) {
			SDL_Delay(16 - Elapsed_Time);
		}
	}

QUIT:
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);

	std::string pass = strVector2str(textinput);
	printf("%s", pass.c_str());
	fflush(stdout);

	return 0;
}
