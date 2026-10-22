cmake_minimum_required(VERSION 3.14)
project(bouclier2050 C)

set(CMAKE_C_STANDARD 99)

# Utilise raylib installe sur le systeme, sinon le telecharge (raylib 5.0)
find_package(raylib 5.0 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    # raylib 5.0 declare cmake_minimum_required(VERSION 3.0), refuse par CMake >= 4
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

set(SIM_SOURCES src/game.c src/world.c src/society.c src/war.c src/military.c src/ops.c src/resources.c src/intl.c src/command.c src/economy.c src/politics.c src/nuclear.c src/map_data.c src/save_fields.c src/events.c)
add_executable(bouclier2050 src/main.c ${SIM_SOURCES})
target_link_libraries(bouclier2050 raylib)
if (NOT MSVC)
    target_link_libraries(bouclier2050 m)
endif()
if (APPLE)
    target_link_libraries(bouclier2050 "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

add_executable(autoplay ${SIM_SOURCES} tools/autoplay.c)
add_executable(tests ${SIM_SOURCES} tools/tests.c)
if (NOT MSVC)
    target_link_libraries(tests m)
endif()
if (NOT MSVC)
    target_link_libraries(autoplay m)
endif()
