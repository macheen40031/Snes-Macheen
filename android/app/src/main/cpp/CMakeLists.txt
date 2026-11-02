cmake_minimum_required(VERSION 3.22.1)
project(SnesMacheenAndroid LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(SNES_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(SDL_ROOT "${CMAKE_CURRENT_LIST_DIR}/SDL")
if(NOT EXISTS "${SDL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "SDL2 source not found. Run: bash android/fetch_sdl2.sh")
endif()

set(SDL_SHARED ON CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build SDL static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Do not build SDL tests" FORCE)
add_subdirectory("${SDL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/SDL2")

add_library(main SHARED
    "${SNES_ROOT}/frontend/sdl_frontend.cpp"
    "${SNES_ROOT}/frontend/frontend_model.cpp"
    "${SNES_ROOT}/frontend/controller_mapper.cpp"
    "${SNES_ROOT}/input/joypad.cpp"
    "${SNES_ROOT}/timing/frame_pacer.cpp"
    "${SNES_ROOT}/frontend/emulator_session.cpp"
    "${SNES_ROOT}/frontend/libretro_session.cpp"
    "${SNES_ROOT}/cpu/cpu.cpp"
    "${SNES_ROOT}/memory/bus.cpp"
    "${SNES_ROOT}/cartridge/cartridge.cpp"
    "${SNES_ROOT}/ppu/ppu.cpp"
    "${SDL_ROOT}/src/main/android/SDL_android_main.c"
)

target_include_directories(main PRIVATE "${SNES_ROOT}/include")
target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(main PRIVATE SDL2 SDL2main android log dl)
