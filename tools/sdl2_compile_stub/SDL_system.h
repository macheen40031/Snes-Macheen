#pragma once
#include "SDL.h"
void* SDL_AndroidGetJNIEnv(void);
void* SDL_AndroidGetActivity(void);
const char* SDL_AndroidGetInternalStoragePath(void);
