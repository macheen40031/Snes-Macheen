cmake_minimum_required(VERSION 3.20)

project(SnesMacheen VERSION 1.23 LANGUAGES CXX)

include(CTest)
option(SNES_STRICT_WARNINGS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(supernes
    src/main.cpp
    cpu/cpu.cpp
    memory/bus.cpp
    cartridge/cartridge.cpp
    ppu/ppu.cpp
    frontend/frontend_model.cpp
    frontend/controller_mapper.cpp
    input/joypad.cpp
    timing/frame_pacer.cpp
    frontend/emulator_session.cpp
)

target_include_directories(supernes PRIVATE include)

target_compile_options(supernes PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(SNES_STRICT_WARNINGS)
    target_compile_options(supernes PRIVATE -Werror)
endif()

if(BUILD_TESTING)
    add_test(NAME supernes_regression COMMAND supernes)
    set_tests_properties(supernes_regression PROPERTIES TIMEOUT 30)
endif()


option(SNES_BUILD_SDL2_FRONTEND "Build the optional SDL2 desktop frontend" OFF)
if(SNES_BUILD_SDL2_FRONTEND)
    find_package(SDL2 CONFIG REQUIRED)
    add_executable(snes-macheen-ui
        frontend/sdl_frontend.cpp
        frontend/frontend_model.cpp
        frontend/controller_mapper.cpp
        input/joypad.cpp
        timing/frame_pacer.cpp
        frontend/emulator_session.cpp
        cpu/cpu.cpp
        memory/bus.cpp
        cartridge/cartridge.cpp
        ppu/ppu.cpp
    )
    target_include_directories(snes-macheen-ui PRIVATE include)
    if(TARGET SDL2::SDL2)
        target_link_libraries(snes-macheen-ui PRIVATE SDL2::SDL2)
    elseif(TARGET SDL2)
        target_link_libraries(snes-macheen-ui PRIVATE SDL2)
    else()
        message(FATAL_ERROR "SDL2 package found but no SDL2 target was exported")
    endif()
    if(TARGET SDL2::SDL2main)
        target_link_libraries(snes-macheen-ui PRIVATE SDL2::SDL2main)
    elseif(TARGET SDL2main)
        target_link_libraries(snes-macheen-ui PRIVATE SDL2main)
    endif()
    target_compile_options(snes-macheen-ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
