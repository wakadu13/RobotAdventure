# BOUCLIER 2050 - Makefile
#
#   make                      compile le jeu (raylib trouve via pkg-config)
#   make RAYLIB_PATH=/chemin  utilise un raylib compile a la main (dossier src/ de raylib)
#   make sim                  banc d'essai d'equilibrage sans interface
#   make test                 tests des mecaniques (nucleaire, decrets, elections...)
#   make run                  compile et lance
#   make ui-test              tests d'interface : captures comparees a tests/ui (xvfb-run sans ecran)
#   make net-test             partie en reseau sur la boucle locale (hote + client, sommes de controle)
#   make ui-ref               regenere les captures de reference apres un changement voulu de l'interface
#
# Windows (MinGW/w64devkit) : make RAYLIB_PATH=C:/raylib/raylib/src

CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -ffp-contract=off   # pas de FMA : memes calculs sur toutes les plateformes (reseau)
BUILD   := build
TARGET  := $(BUILD)/bouclier2050
SIM     := src/game.c src/world.c src/society.c src/war.c src/war_ai.c src/war_diplo.c src/war_logistics.c src/military.c src/ops.c src/resources.c src/intl.c src/command.c src/economy.c src/politics.c src/nuclear.c src/map_data.c src/save_fields.c src/events.c src/cabinet.c src/lang.c src/lang_en.c src/lang_es.c src/lang_de.c
SRC     := src/main.c src/ui_widgets.c src/ui_map.c src/ui_tabs_war.c src/ui_tabs_state.c src/ui_screens.c src/ui_net.c src/ui_options.c src/net.c src/sound.c $(SIM)

ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
    PLATFORM_LIBS := -lopengl32 -lgdi32 -lwinmm -lws2_32
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Darwin)
        PLATFORM_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio
    else
        PLATFORM_LIBS := -lGL -lm -lpthread -ldl -lrt -lX11
    endif
endif

ifdef RAYLIB_PATH
    ifeq ($(wildcard $(RAYLIB_PATH)/libraylib.a),)
        $(error $(RAYLIB_PATH)/libraylib.a introuvable : compilez raylib (make dans raylib/src) ou utilisez CMake)
    endif
    RAYLIB_CFLAGS := -I$(RAYLIB_PATH)
    RAYLIB_LIBS   := $(RAYLIB_PATH)/libraylib.a $(PLATFORM_LIBS)
else
    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
    RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
    ifeq ($(strip $(RAYLIB_LIBS)),)
        RAYLIB_LIBS := -lraylib $(PLATFORM_LIBS)
    endif
endif

.PHONY: all run sim test clean ui-test ui-ref net-test

all: $(TARGET)

$(TARGET): $(SRC) src/ui.h src/sound.h src/game.h src/game_internal.h src/map_data.h | $(BUILD)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(SRC) -o $@ $(RAYLIB_LIBS) -lm

$(BUILD)/autoplay: $(SIM) src/game.h src/game_internal.h src/map_data.h tools/autoplay.c | $(BUILD)
	$(CC) $(CFLAGS) $(SIM) tools/autoplay.c -o $@ -lm

$(BUILD):
	mkdir -p $(BUILD)

run: $(TARGET)
	./$(TARGET)

$(BUILD)/tests: $(SIM) src/game.h src/game_internal.h src/map_data.h tools/tests.c | $(BUILD)
	$(CC) $(CFLAGS) $(SIM) tools/tests.c -o $@ -lm

test: $(BUILD)/tests
	./$(BUILD)/tests

sim: $(BUILD)/autoplay
	./$(BUILD)/autoplay

clean:
	rm -rf $(BUILD)

$(BUILD)/ui_compare: tools/ui_compare.c | $(BUILD)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) tools/ui_compare.c -o $@ $(RAYLIB_LIBS) -lm

ui-test: $(TARGET) $(BUILD)/ui_compare
	mkdir -p $(BUILD)/ui $(BUILD)/ui-diff
	./$(TARGET) --ui-test $(BUILD)/ui
	./$(BUILD)/ui_compare tests/ui $(BUILD)/ui $(BUILD)/ui-diff

ui-ref: $(TARGET)
	mkdir -p tests/ui
	./$(TARGET) --ui-test tests/ui

$(BUILD)/net_test: $(SIM) src/net.c src/net.h tools/net_test.c | $(BUILD)
	$(CC) $(CFLAGS) $(SIM) src/net.c tools/net_test.c -o $@ -lm

net-test: $(BUILD)/net_test
	./$(BUILD)/net_test
