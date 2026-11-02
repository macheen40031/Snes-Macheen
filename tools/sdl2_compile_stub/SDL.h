#pragma once
#include <cstdint>
using Uint8 = uint8_t; using Uint16 = uint16_t; using Uint32 = uint32_t; using Sint16 = int16_t;
struct SDL_Color { Uint8 r,g,b,a; };
struct SDL_Rect { int x,y,w,h; };
struct SDL_Renderer {}; struct SDL_Window {}; struct SDL_Texture {}; struct SDL_GameController {}; struct SDL_Joystick {};
using SDL_AudioDeviceID = Uint32;
struct SDL_AudioSpec { int freq; Uint16 format; Uint8 channels; Uint8 silence; Uint16 samples; Uint16 padding; Uint32 size; void (*callback)(void*, Uint8*, int); void* userdata; };
using SDL_JoystickID = int32_t;
enum SDL_GameControllerButton { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER };
enum SDL_GameControllerAxis { SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY };
struct SDL_Keysym { int sym; }; struct SDL_KeyboardEvent { int repeat; SDL_Keysym keysym; };
struct SDL_TextInputEvent { char text[32]; }; struct SDL_MouseButtonEvent { uint8_t button; int x; int y; }; struct SDL_TouchFingerEvent { float x; float y; }; struct SDL_UserEvent { Uint32 type; int code; void* data1; void* data2; }; struct SDL_DropEvent { Uint32 type; char* file; }; struct SDL_ControllerDeviceEvent { int which; }; struct SDL_ControllerButtonEvent { int which; uint8_t button; }; struct SDL_ControllerAxisEvent { int which; uint8_t axis; Sint16 value; };
union SDL_Event { uint32_t type; SDL_KeyboardEvent key; SDL_TextInputEvent text; SDL_ControllerDeviceEvent cdevice; SDL_ControllerButtonEvent cbutton; SDL_ControllerAxisEvent caxis; SDL_MouseButtonEvent button; SDL_TouchFingerEvent tfinger; SDL_UserEvent user; SDL_DropEvent drop; };
constexpr Uint32 SDL_INIT_VIDEO=1, SDL_INIT_GAMECONTROLLER=2, SDL_INIT_EVENTS=4, SDL_INIT_AUDIO=8;
constexpr int SDL_WINDOWPOS_CENTERED=0, SDL_WINDOW_RESIZABLE=1, SDL_WINDOW_ALLOW_HIGHDPI=2;
constexpr Uint32 SDL_RENDERER_ACCELERATED=1, SDL_RENDERER_PRESENTVSYNC=2, SDL_RENDERER_SOFTWARE=4;
constexpr int SDL_ENABLE=1, SDL_TEXTUREACCESS_STREAMING=1;
constexpr int SDL_BLENDMODE_NONE=0, SDL_BLENDMODE_BLEND=1;
constexpr Uint32 SDL_PIXELFORMAT_ARGB8888=0x16362004u;
constexpr Uint16 AUDIO_S16SYS=0x8010;
constexpr int SDL_AUDIO_ALLOW_FREQUENCY_CHANGE=1;
constexpr int SDLK_UP=1, SDLK_DOWN=2, SDLK_LEFT=3, SDLK_RIGHT=4, SDLK_RETURN=5, SDLK_SPACE=6, SDLK_ESCAPE=7, SDLK_BACKSPACE=8;
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"
#define SDL_HINT_TOUCH_MOUSE_EVENTS "SDL_TOUCH_MOUSE_EVENTS"
constexpr Uint32 SDL_QUIT=10, SDL_KEYDOWN=11, SDL_CONTROLLERDEVICEADDED=12, SDL_CONTROLLERDEVICEREMOVED=13, SDL_CONTROLLERBUTTONDOWN=14, SDL_CONTROLLERBUTTONUP=15, SDL_CONTROLLERAXISMOTION=16, SDL_MOUSEBUTTONDOWN=17, SDL_DROPFILE=18, SDL_FINGERDOWN=19, SDL_TEXTINPUT=20;
constexpr uint8_t SDL_BUTTON_LEFT=1;
int SDL_Init(Uint32); int SDL_InitSubSystem(Uint32); void SDL_Quit(); int SDL_SetHint(const char*,const char*);
void SDL_StartTextInput(); void SDL_StopTextInput();
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*,int,const SDL_AudioSpec*,SDL_AudioSpec*,int);
void SDL_CloseAudioDevice(SDL_AudioDeviceID); void SDL_PauseAudioDevice(SDL_AudioDeviceID,int);
void SDL_ClearQueuedAudio(SDL_AudioDeviceID); Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID);
int SDL_QueueAudio(SDL_AudioDeviceID,const void*,Uint32); const char* SDL_GetError();
SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,Uint32); void SDL_GetWindowSize(SDL_Window*,int*,int*); void SDL_DestroyWindow(SDL_Window*);
SDL_Renderer* SDL_CreateRenderer(SDL_Window*,int,Uint32); void SDL_DestroyRenderer(SDL_Renderer*);
SDL_Texture* SDL_CreateTexture(SDL_Renderer*,Uint32,int,int,int); void SDL_DestroyTexture(SDL_Texture*);
int SDL_UpdateTexture(SDL_Texture*,const SDL_Rect*,const void*,int); int SDL_RenderCopy(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*);
int SDL_RenderSetLogicalSize(SDL_Renderer*,int,int); int SDL_RenderSetScale(SDL_Renderer*,float,float); int SDL_GetRendererOutputSize(SDL_Renderer*,int*,int*); void SDL_RenderWindowToLogical(SDL_Renderer*,int,int,float*,float*); int SDL_GameControllerEventState(int);
int SDL_NumJoysticks(); int SDL_IsGameController(int); SDL_GameController* SDL_GameControllerOpen(int);
SDL_Joystick* SDL_GameControllerGetJoystick(SDL_GameController*); SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick*);
const char* SDL_GameControllerName(SDL_GameController*); void SDL_GameControllerClose(SDL_GameController*);
Uint32 SDL_RegisterEvents(int); int SDL_PushEvent(SDL_Event*); int SDL_PollEvent(SDL_Event*); void SDL_free(void*); void SDL_Delay(Uint32);
int SDL_SetRenderDrawBlendMode(SDL_Renderer*,int);
int SDL_SetRenderDrawColor(SDL_Renderer*,Uint8,Uint8,Uint8,Uint8); int SDL_RenderClear(SDL_Renderer*);
int SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect*); int SDL_RenderDrawRect(SDL_Renderer*,const SDL_Rect*);
int SDL_RenderDrawLine(SDL_Renderer*,int,int,int,int); void SDL_RenderPresent(SDL_Renderer*);

// SDL2 surface and image-loading API used by the artwork loader.
struct SDL_Surface;
struct SDL_RWops;

SDL_RWops* SDL_RWFromFile(const char*, const char*);
SDL_Surface* SDL_LoadBMP_RW(SDL_RWops*, int);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*);
void SDL_FreeSurface(SDL_Surface*);
