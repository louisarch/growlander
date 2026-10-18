cmake_minimum_required(VERSION 3.22.1)
project(GrowlanderAndroid LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL2's Android Activity loads libSDL2.so and libmain.so by default.
set(SDL_SHARED ON CACHE BOOL "Build SDL2 as the Activity-loaded shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build an unused static SDL2 library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Do not build SDL tests" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL tests" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "Do not install bundled SDL2" FORCE)
set(SDL2_ROOT "${CMAKE_CURRENT_LIST_DIR}/SDL")
set(SDL2_TTF_ROOT "${CMAKE_CURRENT_LIST_DIR}/SDL_ttf")
if(NOT EXISTS "${SDL2_ROOT}/CMakeLists.txt" OR NOT EXISTS "${SDL2_TTF_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "SDL sources are missing. Run ./bootstrap.sh from the Android project root.")
endif()

add_subdirectory("${SDL2_ROOT}" SDL2)

# SDL2 and SDL2_ttf are shared libraries; SDLActivity loads SDL2 and main, and
# Android's dynamic linker resolves libSDL2_ttf.so when it loads libmain.so.
# SDL_ttf's pinned release archive includes its FreeType source dependency.
set(BUILD_SHARED_LIBS ON)
set(SDL2TTF_VENDORED ON CACHE BOOL "Use release-bundled FreeType" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "Disable unused text shaping" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "Do not build SDL_ttf samples" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "Do not install bundled SDL_ttf" FORCE)
add_subdirectory("${SDL2_TTF_ROOT}" SDL2_ttf)

# Resolve sources from hasil/src without copying/modifying the shared game core.
get_filename_component(GROWLANDER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../" ABSOLUTE)
add_library(main SHARED
    "${GROWLANDER_ROOT}/src/main.cpp"
    "${GROWLANDER_ROOT}/src/game.cpp"
)
target_compile_features(main PRIVATE cxx_std_17)
target_compile_definitions(main PRIVATE SDL_ANDROID=1)
target_include_directories(main PRIVATE "${GROWLANDER_ROOT}/src")
target_link_libraries(main PRIVATE SDL2 SDL2main SDL2_ttf)
