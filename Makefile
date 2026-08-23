GBDK ?= $(HOME)/gbdk-install/gbdk
LCC = $(GBDK)/bin/lcc

# CGB compatible cart, no MBC (32KB)
LCCFLAGS = -Wm-yc -Wm-yn"KANTANGB"

ROM = build/kantan-gb-play.gbc
SRCS = src/main.c src/chord.c src/sound.c src/ym2151.c src/ui.c src/wheel_gfx.c src/badge_gfx.c src/adpcm_smp.c

all: $(ROM)

$(ROM): $(SRCS) src/chord.h src/sound.h src/ym2151.h src/ui.h
	mkdir -p build
	$(LCC) $(LCCFLAGS) -o $(ROM) $(SRCS)

run: $(ROM)
	mgba $(ROM)

clean:
	rm -rf build

.PHONY: all run clean
