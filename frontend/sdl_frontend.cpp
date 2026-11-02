#include <SDL.h>
#ifdef __ANDROID__
#include <SDL_system.h>
#endif

#include "controller_mapper.hpp"
#include "frontend_model.hpp"
#include "emulator_session.hpp"
#include "libretro_session.hpp"
#include "frame_pacer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#ifdef __ANDROID__
#include <jni.h>
#endif

namespace {
constexpr int kWidth = 1000;
constexpr int kHeight = 620;
constexpr SDL_Color kBackground{12, 17, 23, 255};

constexpr SDL_Color kPanelLight{66, 77, 87, 255};
constexpr SDL_Color kPanelDark{8, 12, 17, 255};
constexpr SDL_Color kText{226, 235, 242, 255};
constexpr SDL_Color kMuted{142, 158, 171, 255};
constexpr SDL_Color kBlue{74, 174, 255, 255};

constexpr SDL_Color kRed{238, 54, 64, 255};
constexpr SDL_Color kRedDark{93, 22, 30, 255};
constexpr SDL_Color kSteel{48, 57, 66, 255};
constexpr SDL_Color kSteelHighlight{105, 118, 130, 255};
constexpr SDL_Color kGreen{70, 216, 138, 255};

struct Glyph { char character; std::array<uint8_t, 7> rows; };
constexpr Glyph kFont[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'-',{0,0,0,31,0,0,0}}, {'.',{0,0,0,0,0,12,12}},
    {':',{0,12,12,0,12,12,0}}, {'/',{1,2,2,4,8,8,16}},
    {'+',{0,4,4,31,4,4,0}}, {'%',{17,2,4,8,16,17,0}}
};

void color(SDL_Renderer* renderer, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

void fill(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color c) {
    color(renderer, c);
    SDL_RenderFillRect(renderer, &rect);
}

void outline(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color c, int thickness = 1) {
    color(renderer, c);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect edge{rect.x + i, rect.y + i, rect.w - (2 * i), rect.h - (2 * i)};
        if (edge.w > 0 && edge.h > 0) SDL_RenderDrawRect(renderer, &edge);
    }
}

void drawText(SDL_Renderer* renderer, int x, int y, const std::string& text,
              int scale, SDL_Color c) {
    color(renderer, c);
    int cursor = x;
    for (unsigned char raw : text) {
        const char ch = static_cast<char>(std::toupper(raw));
        if (ch == ' ') { cursor += 4 * scale; continue; }
        const Glyph* glyph = nullptr;
        for (const auto& candidate : kFont) {
            if (candidate.character == ch) { glyph = &candidate; break; }
        }
        if (glyph != nullptr) {
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if ((glyph->rows[static_cast<std::size_t>(row)] & (1u << (4 - col))) != 0) {
                        SDL_Rect pixel{cursor + col * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(renderer, &pixel);
                    }
                }
            }
        }
        cursor += 6 * scale;
    }
}

void drawBolt(SDL_Renderer* renderer, int x, int y) {
    color(renderer, SDL_Color{4, 8, 12, 255});
    SDL_Rect shadow{x - 5, y - 5, 11, 11};
    SDL_RenderFillRect(renderer, &shadow);
    outline(renderer, SDL_Rect{x - 5, y - 5, 10, 10}, kPanelLight, 2);
    color(renderer, SDL_Color{10, 17, 24, 255});
    SDL_RenderDrawLine(renderer, x - 2, y, x + 2, y);
    SDL_RenderDrawLine(renderer, x, y - 2, x, y + 2);
}

void drawPanel(SDL_Renderer* renderer, SDL_Rect rect) {
    // Keep the chassis translucent: the console artwork remains visible beneath
    // the controls instead of being buried under three opaque rectangles.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill(renderer, rect, SDL_Color{13, 20, 27, 154});
    for (int y = rect.y + 6; y < rect.y + rect.h - 6; y += 8) {
        color(renderer, (y / 8) % 2 == 0 ? SDL_Color{176, 190, 201, 20}
                                             : SDL_Color{0, 0, 0, 24});
        SDL_RenderDrawLine(renderer, rect.x + 7, y, rect.x + rect.w - 8, y);
    }
    // Double-cut steel edge, dark inset and machined highlight.
    outline(renderer, rect, SDL_Color{151, 169, 183, 220}, 2);
    SDL_Rect bevel{rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6};
    outline(renderer, bevel, SDL_Color{5, 9, 13, 230}, 2);
    SDL_Rect inner{rect.x + 7, rect.y + 7, rect.w - 14, rect.h - 14};
    outline(renderer, inner, SDL_Color{104, 124, 139, 145}, 1);
    fill(renderer, SDL_Rect{rect.x + 20, rect.y + 4, rect.w - 40, 2},
         SDL_Color{194, 207, 216, 155});
    drawBolt(renderer, rect.x + 13, rect.y + 13);
    drawBolt(renderer, rect.x + rect.w - 13, rect.y + 13);
    drawBolt(renderer, rect.x + 13, rect.y + rect.h - 13);
    drawBolt(renderer, rect.x + rect.w - 13, rect.y + rect.h - 13);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void drawButton(SDL_Renderer* renderer, SDL_Rect rect, const std::string& label,
                bool selected) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill(renderer, rect, selected ? SDL_Color{82, 25, 34, 218}
                                  : SDL_Color{5, 12, 19, 166});
    // A narrow top highlight and lower shadow create a raised steel switch.
    fill(renderer, SDL_Rect{rect.x + 4, rect.y + 3, rect.w - 8, 2},
         SDL_Color{177, 191, 202, static_cast<uint8_t>(selected ? 150 : 85)});
    fill(renderer, SDL_Rect{rect.x + 4, rect.y + rect.h - 5, rect.w - 8, 2},
         SDL_Color{0, 0, 0, 155});
    outline(renderer, rect, selected ? kRed : kSteelHighlight, selected ? 2 : 1);
    outline(renderer, SDL_Rect{rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6},
            selected ? kRedDark : kPanelDark, 1);
    if (selected) {
        fill(renderer, SDL_Rect{rect.x + 7, rect.y + 7, 3, rect.h - 14}, kRed);
        fill(renderer, SDL_Rect{rect.x + rect.w - 10, rect.y + 7, 3, rect.h - 14}, kRed);
        drawBolt(renderer, rect.x + rect.w - 16, rect.y + rect.h / 2);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    drawText(renderer, rect.x + 18, rect.y + 12, label, 2, kText);
}

std::string controllerName(SDL_GameController* controller) {
    if (controller == nullptr) return "NOT CONNECTED";
    const char* name = SDL_GameControllerName(controller);
    return name != nullptr ? name : "CONTROLLER CONNECTED";
}

std::size_t activeMenuIndex(FrontendScreen screen) {
    switch (screen) {
    case FrontendScreen::GameLibrary: return 0;
    case FrontendScreen::Settings: return 2;
    case FrontendScreen::Controllers: return 3;
    case FrontendScreen::Saves: return 4;
    case FrontendScreen::About: return 5;
    default: return 0;
    }
}

void renderUi(SDL_Renderer* renderer, const FrontendModel& model,
              const std::array<SDL_GameController*, 2>& controllers,
              const EmulatorSession& emulator, const LibretroSession& snes9x,
              bool usingSnes9x, SDL_Texture* gameTexture, SDL_Texture* coreTexture,
              const std::string& inputDiagnostic,
              std::size_t saveActionSelection,
              SDL_Texture* skinTexture) {
    color(renderer, kBackground);
    SDL_RenderClear(renderer);
    if (skinTexture != nullptr)
        SDL_RenderCopy(renderer, skinTexture, nullptr, nullptr);
    drawPanel(renderer, SDL_Rect{12, 10, kWidth - 24, 90});
    drawText(renderer, 38, 30, "SNES-MACHEEN", 4, kText);
    drawText(renderer, 42, 72, "SUPER NINTENDO EMULATOR", 1, kBlue);
    drawText(renderer, 760, 44, "V1.24 ANDROID BUILD", 1, kGreen);

    drawPanel(renderer, SDL_Rect{12, 112, 310, 492});
    drawText(renderer, 34, 132, "MAIN MENU", 2, kRed);
    const auto& items = model.mainMenuItems();
    const std::size_t activeIndex = model.screen() == FrontendScreen::MainMenu
        ? model.selectedIndex() : activeMenuIndex(model.screen());
    for (std::size_t i = 0; i < items.size(); ++i) {
        drawButton(renderer, SDL_Rect{30, 164 + static_cast<int>(i) * 52, 274, 43},
                   items[i], i == activeIndex);
    }
    if (model.screen() != FrontendScreen::MainMenu)
        drawButton(renderer, SDL_Rect{30, 548, 274, 40}, "BACK TO MENU", false);

    drawPanel(renderer, SDL_Rect{334, 112, 654, 492});
    if (model.screen() == FrontendScreen::GameLibrary) {
        drawText(renderer, 360, 136, "GAME LIBRARY", 3, kRed);
        const auto& games = model.games();
        if (games.empty()) {
            drawText(renderer, 360, 206, "NO ROMS FOUND", 2, kMuted);
            drawText(renderer, 360, 238, "PLACE LEGAL ROM DUMPS IN YOUR ROM FOLDER", 1, kMuted);
        } else {
            const std::size_t start = model.selectedIndex() > 7 ? model.selectedIndex() - 7 : 0;
            const std::size_t stop = std::min(games.size(), start + 9);
            for (std::size_t i = start; i < stop; ++i) {
                const int y = 190 + static_cast<int>(i - start) * 42;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                fill(renderer, SDL_Rect{356, y, 610, 34},
                     i == model.selectedIndex() ? SDL_Color{18, 93, 137, 205} : SDL_Color{8, 14, 20, 145});
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                outline(renderer, SDL_Rect{356, y, 610, 34},
                        i == model.selectedIndex() ? kBlue : kPanelDark, 1);
                drawText(renderer, 368, y + 10, games[i].title, 1, kText);
                drawText(renderer, 830, y + 10,
                         std::to_string(games[i].fileSize / 1024u) + " KB", 1, kMuted);
            }
        }
        drawText(renderer, 360, 566, "ENTER SELECT   UP/DOWN BROWSE   ESC BACK", 1, kMuted);
    } else if (model.screen() == FrontendScreen::Controllers) {
        drawText(renderer, 360, 136, "CONTROLLER SETUP", 3, kRed);
        drawText(renderer, 360, 204, "PLAYER 1", 2, kText);
        drawText(renderer, 360, 238, controllerName(controllers[0]), 1,
                 controllers[0] ? kGreen : kMuted);
        drawText(renderer, 360, 294, "PLAYER 2", 2, kText);
        drawText(renderer, 360, 328, controllerName(controllers[1]), 1,
                 controllers[1] ? kGreen : kMuted);
        drawText(renderer, 360, 406, "SDL GAME CONTROLLER EVENTS", 1, kMuted);
        drawText(renderer, 360, 430, "DS4 PAIRING IS DONE IN ANDROID BLUETOOTH SETTINGS", 1, kMuted);
    } else if (model.screen() == FrontendScreen::Settings) {
        drawText(renderer, 360, 136, "SETTINGS", 3, kRed);
        drawText(renderer, 360, 214, "VIDEO / DISPLAY", 2, kText);
        drawText(renderer, 360, 254, "AUDIO", 2, kText);
        drawText(renderer, 360, 294, "EMULATION", 2, kText);
        drawText(renderer, 360, 334, "CONTROLS", 2, kText);
        drawText(renderer, 360, 406, "SETTING CONTROLS WILL BE CONNECTED IN A LATER BUILD", 1, kMuted);
    } else if (model.screen() == FrontendScreen::Saves) {
        drawText(renderer, 360, 136, "SAVES / STATES", 3, kRed);
        drawText(renderer, 360, 178, "SELECT A SAVE-STATE OPERATION", 1, kMuted);
        drawButton(renderer, SDL_Rect{360, 206, 440, 46}, "SAVE SLOT 1", saveActionSelection == 0);
        drawButton(renderer, SDL_Rect{360, 262, 440, 46}, "LOAD SLOT 1", saveActionSelection == 1);
        drawButton(renderer, SDL_Rect{360, 318, 440, 46}, "SAVE SLOT 2", saveActionSelection == 2);
        drawButton(renderer, SDL_Rect{360, 374, 440, 46}, "LOAD SLOT 2", saveActionSelection == 3);
        drawText(renderer, 360, 446, "SLOT FILES ARE STORED BESIDE THE ROM", 1, kMuted);
        drawText(renderer, 360, 478, usingSnes9x && snes9x.loaded()
            ? snes9x.status() : "LOAD A GAME FIRST TO USE SAVE STATES", 1,
            usingSnes9x && snes9x.loaded() ? kGreen : kMuted);
    } else if (model.screen() == FrontendScreen::About) {
        drawText(renderer, 360, 136, "ABOUT SNES-MACHEEN", 3, kRed);
        drawText(renderer, 360, 220, "ORIGINAL C++ 65C816 / SNES EMULATOR PROJECT", 1, kText);
        drawText(renderer, 360, 254, "INDUSTRIAL METAL UI CONCEPT", 1, kMuted);
    } else if (model.screen() == FrontendScreen::Playing) {
        drawText(renderer, 360, 130, "NOW PLAYING", 2, kRed);
        if ((usingSnes9x && coreTexture != nullptr) || (!usingSnes9x && gameTexture != nullptr)) {
            const SDL_Rect viewport{446, 164, 430, 376};
            fill(renderer, SDL_Rect{438, 156, 446, 392}, kPanelDark);
            outline(renderer, SDL_Rect{438, 156, 446, 392}, kPanelLight, 2);
            SDL_RenderCopy(renderer, usingSnes9x ? coreTexture : gameTexture, nullptr, &viewport);
        } else {
            drawText(renderer, 360, 220, "VIDEO TEXTURE UNAVAILABLE", 2, kMuted);
        }
        drawText(renderer, 360, 558, "ESC / CONTROLLER B: RETURN TO LIBRARY", 1, kMuted);
        const std::string gameStatus = usingSnes9x ? snes9x.status() : emulator.status();
        drawText(renderer, 360, 578, gameStatus, 1,
                 !usingSnes9x && emulator.state() == EmulatorSession::State::Faulted ? SDL_Color{255, 150, 110, 255} : kMuted);
    } else {
        drawText(renderer, 360, 136, "SYSTEM READY", 3, kRed);
        drawText(renderer, 360, 220, "SELECT GAME LIBRARY TO BROWSE ROM FILES", 1, kText);
        drawText(renderer, 360, 254, "CONTROLLER INPUT FOUNDATION AVAILABLE", 1, kMuted);
    }

    if (model.screen() != FrontendScreen::Playing) {
        fill(renderer, SDL_Rect{334, 548, 654, 42}, SDL_Color{12, 18, 24, 255});
        drawText(renderer, 350, 564, model.statusMessage(), 1, kMuted);
    }
    color(renderer, kPanelLight);
    SDL_RenderDrawLine(renderer, 24, 610, 976, 610);
    drawText(renderer, 28, 612, "TOUCH MENU ITEMS OR USE ARROWS / ENTER / ESC", 1, kMuted);
    drawText(renderer, 350, 584, inputDiagnostic, 1, kGreen);
    SDL_RenderPresent(renderer);
}

bool mapControllerButton(SDL_GameControllerButton button, GamepadControl& control) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A: control = GamepadControl::South; return true;
    case SDL_CONTROLLER_BUTTON_B: control = GamepadControl::East; return true;
    case SDL_CONTROLLER_BUTTON_X: control = GamepadControl::West; return true;
    case SDL_CONTROLLER_BUTTON_Y: control = GamepadControl::North; return true;
    case SDL_CONTROLLER_BUTTON_BACK: control = GamepadControl::Back; return true;
    case SDL_CONTROLLER_BUTTON_START: control = GamepadControl::Start; return true;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: control = GamepadControl::DpadUp; return true;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: control = GamepadControl::DpadDown; return true;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: control = GamepadControl::DpadLeft; return true;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: control = GamepadControl::DpadRight; return true;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: control = GamepadControl::LeftShoulder; return true;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: control = GamepadControl::RightShoulder; return true;
    default: return false;
    }
}

struct ControllerSlot { SDL_GameController* handle = nullptr; SDL_JoystickID instance = -1; };

#ifdef __ANDROID__
bool callAndroidPickerMethod(const char* methodName) {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) return false;
    jclass activityClass = env->GetObjectClass(activity);
    if (activityClass == nullptr) {
        env->DeleteLocalRef(activity);
        return false;
    }
    jmethodID method = env->GetStaticMethodID(activityClass, methodName, "()V");
    if (method == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
        return false;
    }
    env->CallStaticVoidMethod(activityClass, method);
    const bool success = !env->ExceptionCheck();
    if (!success) env->ExceptionClear();
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
    return success;
}
#endif
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
        return 2;

    // Audio is optional: a missing Android audio device must not prevent the UI/game from launching.
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_Window* window = SDL_CreateWindow("Snes-Macheen", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, kWidth, kHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) { SDL_Quit(); return 3; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) { SDL_DestroyWindow(window); SDL_Quit(); return 4; }
    SDL_RenderSetLogicalSize(renderer, kWidth, kHeight);
    SDL_Texture* skinTexture = nullptr;
    SDL_Surface* skinSurface = SDL_LoadBMP_RW(
        SDL_RWFromFile("ui/industrial_console.bmp", "rb"), 1);
    if (skinSurface != nullptr) {
        skinTexture = SDL_CreateTextureFromSurface(renderer, skinSurface);
        SDL_FreeSurface(skinSurface);
    }
    // If artwork loading fails, retain the existing renderer background.
    SDL_GameControllerEventState(SDL_ENABLE);

    FrontendModel model;
    ControllerMapper mapper;
    EmulatorSession emulator;
    LibretroSession snes9x;
    bool usingSnes9x = false;
    SDL_Texture* coreTexture = nullptr;
    SDL_Texture* gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, static_cast<int>(PPU::SCREEN_WIDTH),
        static_cast<int>(PPU::SCREEN_HEIGHT));
    std::filesystem::path romDirectory;
#ifdef __ANDROID__
    const char* internalStorage = SDL_AndroidGetInternalStoragePath();
    romDirectory = (internalStorage != nullptr && *internalStorage != '\0')
        ? std::filesystem::path(internalStorage) / "roms"
        : std::filesystem::current_path() / "roms";
#else
    romDirectory = std::filesystem::current_path() / "ROMs";
#endif
    std::error_code directoryError;
    std::filesystem::create_directories(romDirectory, directoryError);
    if (!directoryError) model.scanGameDirectory(romDirectory);
    else model.setStatusMessage("Unable to prepare the private ROM folder.");
    if (argc > 1) {
        const std::filesystem::path requestedDirectory(argv[1]);
        if (std::filesystem::is_directory(requestedDirectory, directoryError))
            model.scanGameDirectory(requestedDirectory);
        else
            model.setStatusMessage("ROM directory is not available. Choose a ROM folder.");
    }

    std::string activeRomPath;
    std::size_t saveActionSelection = 0;
    auto launchSelectedGame = [&]() {
        const GameEntry* selected = model.selectedGame();
        if (selected == nullptr) {
            model.setStatusMessage("Select a ROM before launching.");
            return;
        }
        activeRomPath = selected->path.string();
        usingSnes9x = snes9x.loadRom("snes9x_libretro_android.so", activeRomPath);
        if (usingSnes9x) {
            model.enterPlaying(selected->title);
            model.setStatusMessage("Snes9x core loaded.");
        } else if (emulator.loadRom(activeRomPath)) {
            model.enterPlaying(selected->title);
            model.setStatusMessage("Snes9x unavailable; prototype fallback is active.");
        } else {
            model.setStatusMessage(snes9x.status() + " / " + emulator.status());
        }
    };
    auto goBack = [&]() {
        if (model.screen() == FrontendScreen::Playing) {
            if (usingSnes9x && !activeRomPath.empty()) snes9x.saveRam(activeRomPath + ".srm");
            else emulator.saveBatteryRam();
            emulator.unload();
            snes9x.unload();
            usingSnes9x = false;
            if (coreTexture != nullptr) { SDL_DestroyTexture(coreTexture); coreTexture = nullptr; }
            model.returnToLibrary();
            model.setStatusMessage("Returned to game library.");
        } else {
            model.handle(FrontendAction::Back);
        }
    };
    auto performSaveAction = [&]() {
        if (!usingSnes9x || !snes9x.loaded() || activeRomPath.empty()) {
            model.setStatusMessage("Load a game first. Save states require the Snes9x backend.");
            return;
        }
        const unsigned slot = (saveActionSelection < 2) ? 1u : 2u;
        const bool save = (saveActionSelection % 2u) == 0u;
        const std::string statePath = activeRomPath + ".state" +
            std::to_string(slot) + ".s9x";
        const bool success = save ? snes9x.saveState(statePath)
                                  : snes9x.loadState(statePath);
        model.setStatusMessage(snes9x.status());
        if (!success) return;
    };
    auto confirm = [&]() {
        if (model.screen() == FrontendScreen::Saves) {
            performSaveAction();
        } else if (model.screen() == FrontendScreen::GameLibrary) {
            launchSelectedGame();
        } else if (model.screen() == FrontendScreen::MainMenu && model.selectedIndex() == 1) {
#ifdef __ANDROID__
            if (!callAndroidPickerMethod("openRomPicker"))
                model.setStatusMessage("Android ROM picker could not be opened.");
            else
                model.setStatusMessage("Choose a supported SNES ROM file.");
#else
            model.setStatusMessage("ROM file picker is available in the Android build.");
#endif
        } else if (model.screen() == FrontendScreen::MainMenu && model.selectedIndex() == 6) {
#ifdef __ANDROID__
            if (!callAndroidPickerMethod("openRomFolderPicker"))
                model.setStatusMessage("Android ROM folder picker could not be opened.");
            else
                model.setStatusMessage("Choose a folder containing your SNES ROM files.");
#else
            model.setStatusMessage("Persistent ROM folder access is available in the Android build.");
#endif
        } else {
            model.handle(FrontendAction::Confirm);
        }
    };

#ifdef __ANDROID__
    callAndroidPickerMethod("importPersistedRomFolder");
#endif

    auto openMenuItem = [&](std::size_t index) {
        switch (index) {
        case 0: model.navigateTo(FrontendScreen::GameLibrary); model.setStatusMessage("Choose a game from your library."); break;
        case 1:
#ifdef __ANDROID__
            if (!callAndroidPickerMethod("openRomPicker")) model.setStatusMessage("Android ROM picker could not be opened.");
            else model.setStatusMessage("Choose a supported SNES ROM file.");
#else
            model.setStatusMessage("ROM file picker is available in the Android build.");
#endif
            break;
        case 2: model.navigateTo(FrontendScreen::Settings); model.setStatusMessage("Settings panel selected."); break;
        case 3: model.navigateTo(FrontendScreen::Controllers); model.setStatusMessage("Controller setup selected."); break;
        case 4: saveActionSelection = 0; model.navigateTo(FrontendScreen::Saves); model.setStatusMessage("Save-state manager selected."); break;
        case 5: model.navigateTo(FrontendScreen::About); model.setStatusMessage("About Snes-Macheen."); break;
        case 6:
#ifdef __ANDROID__
            if (!callAndroidPickerMethod("openRomFolderPicker")) model.setStatusMessage("Android ROM folder picker could not be opened.");
            else model.setStatusMessage("Choose a folder containing your SNES ROM files.");
#else
            model.setStatusMessage("Persistent ROM folder access is available in the Android build.");
#endif
            break;
        default: break;
        }
    };
    auto handlePointerTap = [&](int x, int y) {
        if (x >= 30 && x <= 304) {
            for (std::size_t i = 0; i < model.mainMenuItems().size(); ++i) {
                const int top = 164 + static_cast<int>(i) * 52;
                if (y >= top && y < top + 43) {
                    if (model.screen() == FrontendScreen::MainMenu) model.selectIndex(i);
                    openMenuItem(i);
                    return;
                }
            }
            if (model.screen() != FrontendScreen::MainMenu && y >= 548 && y <= 595) {
                model.navigateTo(FrontendScreen::MainMenu);
                model.setStatusMessage("Returned to main menu.");
                return;
            }
        }
        if (model.screen() == FrontendScreen::Saves &&
            x >= 356 && x <= 966 && y >= 206 && y < 420) {
            const std::size_t action = static_cast<std::size_t>((y - 206) / 56);
            if (action < 4) {
                saveActionSelection = action;
                performSaveAction();
            }
            return;
        }
        if (model.screen() == FrontendScreen::GameLibrary &&
            x >= 356 && x <= 966 && y >= 190 && y < 568 && !model.games().empty()) {
            const std::size_t start = model.selectedIndex() > 7 ? model.selectedIndex() - 7 : 0;
            const std::size_t row = static_cast<std::size_t>((y - 190) / 42);
            const std::size_t index = start + row;
            if (index < model.games().size() && model.selectIndex(index)) confirm();
        }
    };

    std::string inputDiagnostic = "INPUT: WAITING FOR TOUCH";

    auto handleWindowTap = [&](int windowX, int windowY, const char* source) {
        float logicalX = 0.0f;
        float logicalY = 0.0f;
        SDL_RenderWindowToLogical(renderer, windowX, windowY, &logicalX, &logicalY);
        const int x = static_cast<int>(logicalX);
        const int y = static_cast<int>(logicalY);
        inputDiagnostic = std::string(source) + " X " +
            std::to_string(x) + " Y " + std::to_string(y);
        handlePointerTap(x, y);
    };

    std::array<ControllerSlot, 2> slots{};
    std::array<int16_t, 2> controllerXAxis{};
    std::array<int16_t, 2> controllerYAxis{};
    auto openController = [&](int deviceIndex) {
        for (std::size_t slot = 0; slot < slots.size(); ++slot) {
            if (slots[slot].handle == nullptr && SDL_IsGameController(deviceIndex)) {
                SDL_GameController* pad = SDL_GameControllerOpen(deviceIndex);
                if (pad != nullptr) {
                    slots[slot].handle = pad;
                    slots[slot].instance = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
                    break;
                }
            }
        }
    };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) openController(i);

    bool running = true;
    FramePacer framePacer(VideoStandard::Ntsc);
    auto previousFrameTick = std::chrono::steady_clock::now();
    while (running) {
        const auto frameTick = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            frameTick - previousFrameTick).count();
        previousFrameTick = frameTick;
        const std::size_t framesDue = framePacer.advance(
            elapsed > 0 ? static_cast<uint64_t>(elapsed) : 0u);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                handleWindowTap(event.button.x, event.button.y, "MOUSE");
            } else if (event.type == SDL_FINGERDOWN) {
                int windowWidth = 0;
                int windowHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                if (windowWidth > 0 && windowHeight > 0) {
                    const int windowX = static_cast<int>(
                        event.tfinger.x * static_cast<float>(windowWidth));
                    const int windowY = static_cast<int>(
                        event.tfinger.y * static_cast<float>(windowHeight));
                    handleWindowTap(windowX, windowY, "FINGER");
                }
            }
            else if (event.type == SDL_DROPFILE) {
                const std::string droppedValue = event.drop.file != nullptr ? event.drop.file : "";
                SDL_free(event.drop.file);
                if (!droppedValue.empty()) {
                    const std::filesystem::path droppedPath(droppedValue);
                    std::error_code dropError;
                    if (std::filesystem::is_directory(droppedPath, dropError) && !dropError) {
                        if (model.scanGameDirectory(droppedPath)) {
                            model.setStatusMessage("Persistent ROM folder imported. " +
                                std::to_string(model.games().size()) + " supported ROM file(s) ready.");
                        }
                    } else if (model.scanGameDirectory(droppedPath.parent_path())) {
                        const auto& games = model.games();
                        const auto found = std::find_if(games.begin(), games.end(),
                            [&](const GameEntry& game) { return game.path == droppedPath; });
                        if (found != games.end() &&
                            model.selectIndex(static_cast<std::size_t>(std::distance(games.begin(), found)))) {
                            launchSelectedGame();
                        } else {
                            model.setStatusMessage("Selected ROM was copied but could not be listed.");
                        }
                    }
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                case SDLK_UP:
                    if (model.screen() == FrontendScreen::Saves)
                        saveActionSelection = saveActionSelection == 0 ? 3 : saveActionSelection - 1;
                    else model.handle(FrontendAction::Up);
                    break;
                case SDLK_DOWN:
                    if (model.screen() == FrontendScreen::Saves)
                        saveActionSelection = (saveActionSelection + 1) % 4;
                    else model.handle(FrontendAction::Down);
                    break;
                case SDLK_LEFT: model.handle(FrontendAction::Left); break;
                case SDLK_RIGHT: model.handle(FrontendAction::Right); break;
                case SDLK_RETURN: case SDLK_SPACE: confirm(); break;
                case SDLK_ESCAPE: goBack(); break;
                default: break;
                }
            } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                openController(event.cdevice.which);
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                for (std::size_t slot = 0; slot < slots.size(); ++slot) {
                    if (slots[slot].instance == event.cdevice.which) {
                        if (slots[slot].handle != nullptr) SDL_GameControllerClose(slots[slot].handle);
                        slots[slot] = {};
                        controllerXAxis[slot] = 0;
                        controllerYAxis[slot] = 0;
                        mapper.setLeftStick(slot, 0, 0);
                        for (unsigned control = 0;
                             control < static_cast<unsigned>(GamepadControl::Count); ++control) {
                            mapper.setButton(slot, static_cast<GamepadControl>(control), false);
                        }
                    }
                }
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN ||
                       event.type == SDL_CONTROLLERBUTTONUP) {
                for (std::size_t slot = 0; slot < slots.size(); ++slot) {
                    if (slots[slot].instance != event.cbutton.which) continue;
                    GamepadControl control{};
                    if (mapControllerButton(static_cast<SDL_GameControllerButton>(event.cbutton.button), control)) {
                        const bool pressed = event.type == SDL_CONTROLLERBUTTONDOWN;
                        mapper.setButton(slot, control, pressed);
                        if (pressed && control == GamepadControl::DpadUp) {
                            if (model.screen() == FrontendScreen::Saves)
                                saveActionSelection = saveActionSelection == 0 ? 3 : saveActionSelection - 1;
                            else model.handle(FrontendAction::Up);
                        }
                        else if (pressed && control == GamepadControl::DpadDown) {
                            if (model.screen() == FrontendScreen::Saves)
                                saveActionSelection = (saveActionSelection + 1) % 4;
                            else model.handle(FrontendAction::Down);
                        }
                        else if (pressed && control == GamepadControl::DpadLeft) model.handle(FrontendAction::Left);
                        else if (pressed && control == GamepadControl::DpadRight) model.handle(FrontendAction::Right);
                        else if (pressed && control == GamepadControl::South) confirm();
                        else if (pressed && control == GamepadControl::East) goBack();
                    }
                }
            } else if (event.type == SDL_CONTROLLERAXISMOTION) {
                for (std::size_t slot = 0; slot < slots.size(); ++slot) {
                    if (slots[slot].instance != event.caxis.which) continue;
                    // SDL delivers one axis at a time; preserve the other axis.
                    if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                        controllerXAxis[slot] = event.caxis.value;
                    if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                        controllerYAxis[slot] = event.caxis.value;
                    mapper.setLeftStick(slot, controllerXAxis[slot], controllerYAxis[slot]);
                }
            }
        }
        // Publish both mapped controller states to the emulated $4016/$4017
        // serial ports. This also clears inputs after controller disconnects.
        emulator.setControllerState(0, mapper.state(0));
        emulator.setControllerState(1, mapper.state(1));
        snes9x.setControllerState(0, mapper.state(0));
        snes9x.setControllerState(1, mapper.state(1));

        if (model.screen() == FrontendScreen::Playing) {
            if (usingSnes9x && snes9x.loaded()) {
                for (std::size_t frame = 0; frame < framesDue; ++frame) snes9x.runFrame();
                const auto& pixels = snes9x.framebuffer();
                if (!pixels.empty()) {
                    if (coreTexture == nullptr) {
                        if (coreTexture) SDL_DestroyTexture(coreTexture);
                        coreTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, static_cast<int>(snes9x.width()),
                            static_cast<int>(snes9x.height()));
                    }
                    if (coreTexture) SDL_UpdateTexture(coreTexture, nullptr, pixels.data(),
                        static_cast<int>(snes9x.width() * sizeof(uint32_t)));
                }
            } else if (emulator.loaded()) {
                if (emulator.state() != EmulatorSession::State::Halted &&
                    emulator.state() != EmulatorSession::State::Faulted) {
                    for (std::size_t frame = 0; frame < framesDue; ++frame) emulator.runFrame(2000);
                }
                if (gameTexture != nullptr) {
                    const auto& pixels = emulator.framebufferArgb8888();
                    SDL_UpdateTexture(gameTexture, nullptr, pixels.data(),
                        static_cast<int>(PPU::SCREEN_WIDTH * sizeof(uint32_t)));
                }
            }
        }
        renderUi(renderer, model, {slots[0].handle, slots[1].handle},
                 emulator, snes9x, usingSnes9x, gameTexture, coreTexture,
                 inputDiagnostic, saveActionSelection, skinTexture);
        SDL_Delay(framePacer.delayMilliseconds());
    }

    if (usingSnes9x && !activeRomPath.empty()) snes9x.saveRam(activeRomPath + ".srm");
    else emulator.saveBatteryRam();
    snes9x.unload();
    if (coreTexture != nullptr) SDL_DestroyTexture(coreTexture);
    for (auto& slot : slots) if (slot.handle != nullptr) SDL_GameControllerClose(slot.handle);
    if (skinTexture != nullptr) SDL_DestroyTexture(skinTexture);
    if (gameTexture != nullptr) SDL_DestroyTexture(gameTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
