cmake_minimum_required(VERSION 3.16)
project(Growlander VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EMSCRIPTEN)
    # SDL2 and SDL2_ttf are provided by the Emscripten ports rather than
    # host packages. Port flags are needed while compiling SDL-using sources
    # and again when linking the final HTML application.
    add_compile_options(-sUSE_SDL=2 -sUSE_SDL_TTF=2)
    add_link_options(
        -sUSE_SDL=2
        -sUSE_SDL_TTF=2
        -sASYNCIFY
        -sALLOW_MEMORY_GROWTH=1
        -sMINIFY_HTML=0
        -sTOTAL_STACK=1048576
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
endif()

add_library(growlander_core
    src/game.cpp
)
target_include_directories(growlander_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(game
    src/main.cpp
)
set_target_properties(game PROPERTIES OUTPUT_NAME growlander)
target_link_libraries(game PRIVATE growlander_core)
target_compile_definitions(game PRIVATE
    "ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\""
)

if(EMSCRIPTEN)
    # SDL ports are enabled through the Emscripten compiler/linker flags above.
elseif(WIN32)
    # SDL2 config packages are commonly supplied by vcpkg. Pass the vcpkg
    # toolchain file when configuring; Windows builds are not asserted tested.
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    set(GROWLANDER_SDL2_TARGET SDL2::SDL2)
    set(GROWLANDER_SDL2_TTF_TARGET SDL2_ttf::SDL2_ttf)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.18)
    pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
    set(GROWLANDER_SDL2_TARGET PkgConfig::SDL2)
    set(GROWLANDER_SDL2_TTF_TARGET PkgConfig::SDL2_TTF)
endif()

if(EMSCRIPTEN)
    set_target_properties(game PROPERTIES
        OUTPUT_NAME index
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    set_property(TARGET game APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
else()
    # SDL2main (where provided) must precede the SDL2 target in link order.
    if(TARGET SDL2::SDL2main)
        target_link_libraries(game PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(game PRIVATE
        ${GROWLANDER_SDL2_TARGET}
        ${GROWLANDER_SDL2_TTF_TARGET}
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:game>/assets"
        COMMENT "Copying game assets beside the executable"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(growlander_tests
        tests/core_tests.cpp
    )
    target_link_libraries(growlander_tests PRIVATE growlander_core)
    add_test(NAME growlander_core_tests COMMAND growlander_tests)
    add_executable(growlander_touch_tests tests/touch_tests.cpp)
    target_link_libraries(growlander_touch_tests PRIVATE growlander_core)
    add_test(NAME growlander_touch_tests COMMAND growlander_touch_tests)
endif()
