cmake_minimum_required(VERSION 3.16)

project(StrandedSDL2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)

add_executable(stranded
    src/game/main.cpp
)

target_link_libraries(stranded PRIVATE SDL2::SDL2main SDL2::SDL2)
target_compile_features(stranded PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(stranded PRIVATE /W4 /permissive-)
else()
    target_compile_options(stranded PRIVATE -Wall -Wextra -Wpedantic)
endif()
