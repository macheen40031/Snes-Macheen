#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

enum class FrontendScreen {
    MainMenu,
    GameLibrary,
    Settings,
    Controllers,
    Saves,
    About,
    Playing
};

enum class FrontendAction {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back
};

struct GameEntry {
    std::string title;
    std::filesystem::path path;
    std::uintmax_t fileSize = 0;
};

// Platform-neutral state model shared by SDL2, Android and desktop frontends.
// It owns navigation/library state only; rendering and OS file pickers stay in
// the frontend adapter.
class FrontendModel {
public:
    FrontendModel();

    FrontendScreen screen() const noexcept;
    std::size_t selectedIndex() const noexcept;
    const std::string& statusMessage() const noexcept;
    const std::vector<std::string>& mainMenuItems() const noexcept;
    const std::vector<GameEntry>& games() const noexcept;

    void handle(FrontendAction action);
    bool scanGameDirectory(const std::filesystem::path& directory);
    bool selectGame(std::size_t index);
    const GameEntry* selectedGame() const noexcept;
    void enterPlaying(const std::string& gameTitle);
    void setStatusMessage(std::string message);
    void returnToLibrary() noexcept;

private:
    FrontendScreen currentScreen = FrontendScreen::MainMenu;
    std::size_t currentSelection = 0;
    std::string status = "Ready. Select Game Library or Load ROM.";
    std::vector<std::string> menuItems;
    std::vector<GameEntry> library;

    void setScreen(FrontendScreen next) noexcept;
    void moveSelection(int direction) noexcept;
    static bool isSupportedRom(const std::filesystem::path& path);
};
