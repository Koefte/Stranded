cmake_minimum_required(VERSION 3.10)

project(Stranded)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure MSVC uses DLL runtime to avoid CRT conflicts (LNK4098)
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

# Use vcpkg CONFIG packages
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_net CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

# Sources
file(GLOB_RECURSE SOURCES "src/**/*.cpp")

add_executable(Stranded ${SOURCES})

# Link imported targets
target_link_libraries(Stranded PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    SDL2_net::SDL2_net
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
)

# Copy assets to build output
add_custom_command(TARGET Stranded POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/sprites
    $<TARGET_FILE_DIR:Stranded>/sprites
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/tilesets
    $<TARGET_FILE_DIR:Stranded>/tilesets
)
