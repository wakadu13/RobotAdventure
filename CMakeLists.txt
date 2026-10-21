cmake_minimum_required(VERSION 3.14)
project(bouclier2050 C)

set(CMAKE_C_STANDARD 99)

# Utilise raylib installe sur le systeme, sinon le telecharge (raylib 5.0)
find_package(raylib 5.0 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(bouclier2050 src/main.c src/game.c)
target_link_libraries(bouclier2050 raylib)
if (NOT MSVC)
    target_link_libraries(bouclier2050 m)
endif()
if (APPLE)
    target_link_libraries(bouclier2050 "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

add_executable(autoplay src/game.c tools/autoplay.c)
if (NOT MSVC)
    target_link_libraries(autoplay m)
endif()
