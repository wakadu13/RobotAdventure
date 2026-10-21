# BOUCLIER 2050 - Makefile
#
#   make                      compile le jeu (raylib trouve via pkg-config)
#   make RAYLIB_PATH=/chemin  utilise un raylib compile a la main (dossier src/ de raylib)
#   make sim                  banc d'essai d'equilibrage sans interface
#   make run                  compile et lance
#
# Windows (MinGW/w64devkit) : make RAYLIB_PATH=C:/raylib/raylib/src

CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra
BUILD   := build
TARGET  := $(BUILD)/bouclier2050
SIM     := src/game.c src/world.c src/society.c src/war.c src/map_data.c
SRC     := src/main.c $(SIM)

ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
    PLATFORM_LIBS := -lopengl32 -lgdi32 -lwinmm
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Darwin)
        PLATFORM_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio
    else
        PLATFORM_LIBS := -lGL -lm -lpthread -ldl -lrt -lX11
    endif
endif

ifdef RAYLIB_PATH
    RAYLIB_CFLAGS := -I$(RAYLIB_PATH)
    RAYLIB_LIBS   := $(RAYLIB_PATH)/libraylib.a $(PLATFORM_LIBS)
else
    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
    RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
    ifeq ($(strip $(RAYLIB_LIBS)),)
        RAYLIB_LIBS := -lraylib $(PLATFORM_LIBS)
    endif
endif

.PHONY: all run sim clean

all: $(TARGET)

$(TARGET): $(SRC) src/game.h src/game_internal.h src/map_data.h | $(BUILD)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(SRC) -o $@ $(RAYLIB_LIBS) -lm

$(BUILD)/autoplay: $(SIM) src/game.h tools/autoplay.c | $(BUILD)
	$(CC) $(CFLAGS) $(SIM) tools/autoplay.c -o $@ -lm

$(BUILD):
	mkdir -p $(BUILD)

run: $(TARGET)
	./$(TARGET)

sim: $(BUILD)/autoplay
	./$(BUILD)/autoplay

clean:
	rm -rf $(BUILD)
