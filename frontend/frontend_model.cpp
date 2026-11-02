#include "frontend_model.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

FrontendModel::FrontendModel()
    : menuItems{"Game Library", "Load ROM", "Settings", "Controllers",
                "Saves / States", "Cheats", "About", "ROM FOLDER"} {}

FrontendScreen FrontendModel::screen() const noexcept { return currentScreen; }
std::size_t FrontendModel::selectedIndex() const noexcept { return currentSelection; }
const std::string& FrontendModel::statusMessage() const noexcept { return status; }
const std::vector<std::string>& FrontendModel::mainMenuItems() const noexcept { return menuItems; }
const std::vector<GameEntry>& FrontendModel::games() const noexcept { return library; }

void FrontendModel::setScreen(FrontendScreen next) noexcept {
    currentScreen = next;
    currentSelection = 0;
}

void FrontendModel::moveSelection(int direction) noexcept {
    const std::size_t count = currentScreen == FrontendScreen::MainMenu
        ? menuItems.size() : library.size();
    if (count == 0) {
        currentSelection = 0;
        return;
    }
    if (direction < 0)
        currentSelection = currentSelection == 0 ? count - 1 : currentSelection - 1;
    else
        currentSelection = (currentSelection + 1) % count;
}

bool FrontendModel::isSupportedRom(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".sfc" || extension == ".smc" ||
           extension == ".fig" || extension == ".swc";
}

void FrontendModel::handle(FrontendAction action) {
    switch (currentScreen) {
    case FrontendScreen::MainMenu:
        if (action == FrontendAction::Up) moveSelection(-1);
        else if (action == FrontendAction::Down) moveSelection(1);
        else if (action == FrontendAction::Confirm) {
            switch (currentSelection) {
            case 0: setScreen(FrontendScreen::GameLibrary); status = "Choose a game."; break;
            case 1: setScreen(FrontendScreen::GameLibrary); status = "Choose a ROM to load."; break;
            case 2: setScreen(FrontendScreen::Settings); status = "Settings are ready."; break;
            case 3: setScreen(FrontendScreen::Controllers); status = "Controller setup is ready."; break;
            case 4: setScreen(FrontendScreen::Saves); status = "Save-state manager is not connected yet."; break;
            case 5: setScreen(FrontendScreen::About); status = "Snes-Macheen — original C++ emulator project."; break;
            default: break;
            }
        }
        break;
    case FrontendScreen::GameLibrary:
        if (action == FrontendAction::Up) moveSelection(-1);
        else if (action == FrontendAction::Down) moveSelection(1);
        else if (action == FrontendAction::Confirm) {
            if (selectGame(currentSelection))
                status = "Selected " + library[currentSelection].title + ". ROM loading is the next integration step.";
            else
                status = "No ROM selected. Scan a game directory first.";
        } else if (action == FrontendAction::Back) {
            setScreen(FrontendScreen::MainMenu);
            status = "Returned to main menu.";
        }
        break;
    case FrontendScreen::Playing:
        if (action == FrontendAction::Back) {
            returnToLibrary();
            status = "Returned to game library.";
        }
        break;
    case FrontendScreen::Settings:
    case FrontendScreen::Controllers:
    case FrontendScreen::Saves:
    case FrontendScreen::Cheats:
    case FrontendScreen::About:
        if (action == FrontendAction::Back) {
            setScreen(FrontendScreen::MainMenu);
            status = "Returned to main menu.";
        }
        break;
    }
}

bool FrontendModel::scanGameDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        status = "Unable to open game directory.";
        return false;
    }

    std::vector<GameEntry> found;
    std::filesystem::directory_iterator iterator(directory,
        std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error)) {
        std::error_code itemError;
        if (!iterator->is_regular_file(itemError) || itemError ||
            !isSupportedRom(iterator->path()))
            continue;
        const auto size = iterator->file_size(itemError);
        found.push_back({iterator->path().stem().string(), iterator->path(),
                         itemError ? 0u : size});
        if (found.size() >= 10000) break;
    }

    std::sort(found.begin(), found.end(), [](const GameEntry& left, const GameEntry& right) {
        std::string a = left.title;
        std::string b = right.title;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return a == b ? left.path.string() < right.path.string() : a < b;
    });

    library = std::move(found);
    currentSelection = 0;
    status = "Found " + std::to_string(library.size()) + " supported ROM file(s).";
    return !error;
}

bool FrontendModel::selectIndex(std::size_t index) noexcept {
    const std::size_t count = currentScreen == FrontendScreen::MainMenu
        ? menuItems.size() : library.size();
    if (index >= count) return false;
    currentSelection = index;
    return true;
}

bool FrontendModel::selectGame(std::size_t index) {
    if (index >= library.size()) return false;
    currentSelection = index;
    return true;
}

const GameEntry* FrontendModel::selectedGame() const noexcept {
    return currentSelection < library.size() ? &library[currentSelection] : nullptr;
}

void FrontendModel::enterPlaying(const std::string& gameTitle) {
    setScreen(FrontendScreen::Playing);
    status = "Playing " + gameTitle + ". Escape returns to the library.";
}

void FrontendModel::setStatusMessage(std::string message) {
    status = std::move(message);
}

void FrontendModel::returnToLibrary() noexcept {
    setScreen(FrontendScreen::GameLibrary);
}

void FrontendModel::navigateTo(FrontendScreen screen) noexcept {
    setScreen(screen);
}
