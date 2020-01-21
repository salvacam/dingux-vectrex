#
# dingux-vectrex for the RetroFW
#
# by pingflood; 2019
#

CHAINPREFIX := /opt/mipsel-RetroFW-linux-uclibc
CROSS_COMPILE := $(CHAINPREFIX)/usr/bin/mipsel-linux-

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SYSROOT		= $(shell $(CC) --print-sysroot)
SDL_CFLAGS	= $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS	= $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

TARGET = dingux-vectrex/dingux-vectrex.dge

ifdef V
	CMD =
	SUM = @\#
else
	CMD = @
	SUM = @echo
endif

INCLUDES	= -I./src
CFLAGS		= $(SDL_CFLAGS) $(INCLUDES) -DGCW0_MODE -g -O2 -Wall -fstrength-reduce -s -DSOUND_SUPPORT -DNO_STDIO_REDIRECT
# CFLAGS		= $(SDL_CFLAGS) $(INCLUDES) -DGCW0_MODE -ggdb -O2 -Wall -fstrength-reduce -DSOUND_SUPPORT -DNO_STDIO_REDIRECT
CXXFLAGS	= $(CFLAGS) # -fno-exceptions -fno-rtti
LDFLAGS		= $(CXXFLAGS) $(SDL_LIBS) -lSDL_image -lpng -lz -lpthread -lm #-lgcc

OBJS = \
	src/gp2x_psp.o \
	src/e6809.o \
	src/e8910.o  \
	src/osint.o \
	src/vecx.o \
	src/global.o \
	src/psp_main.o \
	src/psp_sdl.o \
	src/psp_kbd.o \
	src/psp_dve.o \
	src/psp_fmgr.o \
	src/psp_font.o \
	src/psp_danzeff.o \
	src/psp_editor.o \
	src/psp_menu_set.o \
	src/psp_menu_cheat.o \
	src/psp_menu_list.o \
	src/psp_menu.o \
	src/psp_menu_kbd.o \
	src/psp_menu_help.o

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(SUM) "LD $@"
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(SUM) "CC $@"
	$(CMD)$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(SUM) "CXX $@"
	$(CMD)$(CXX) $(CFLAGS) -c $< -o $@

clean :
	$(SUM) "CLEAN"
	$(CMD)rm -f $(OBJS) $(TARGET)

ipk: $(TARGET)
	@rm -rf /tmp/.dingux-vectrex-ipk/ && mkdir -p /tmp/.dingux-vectrex-ipk/root/home/retrofw/emus/dingux-vectrex /tmp/.dingux-vectrex-ipk/root/home/retrofw/roms  /tmp/.dingux-vectrex-ipk/root/home/retrofw/apps/gmenu2x/sections/emulators /tmp/.dingux-vectrex-ipk/root/home/retrofw/apps/gmenu2x/sections/emulators.systems
	@cp -r dingux-vectrex/graphics dingux-vectrex/background.png dingux-vectrex/dingux-vectrex.png dingux-vectrex/default.bin dingux-vectrex/dingux-vectrex.elf /tmp/.dingux-vectrex-ipk/root/home/retrofw/emus/dingux-vectrex
	@cp -r dingux-vectrex/vectrex /tmp/.dingux-vectrex-ipk/root/home/retrofw/roms
	@cp dingux-vectrex/dingux-vectrex.lnk /tmp/.dingux-vectrex-ipk/root/home/retrofw/apps/gmenu2x/sections/emulators
	@cp dingux-vectrex/vectrex.dingux-vectrex.lnk /tmp/.dingux-vectrex-ipk/root/home/retrofw/apps/gmenu2x/sections/emulators.systems
	@sed "s/^Version:.*/Version: $$(date +%Y%m%d)/" dingux-vectrex/control > /tmp/.dingux-vectrex-ipk/control
	@cp dingux-vectrex/conffiles /tmp/.dingux-vectrex-ipk/
	@tar --owner=0 --group=0 -czvf /tmp/.dingux-vectrex-ipk/control.tar.gz -C /tmp/.dingux-vectrex-ipk/ control conffiles
	@tar --owner=0 --group=0 -czvf /tmp/.dingux-vectrex-ipk/data.tar.gz -C /tmp/.dingux-vectrex-ipk/root/ .
	@echo 2.0 > /tmp/.dingux-vectrex-ipk/debian-binary
	@ar r dingux-vectrex/dingux-vectrex.ipk /tmp/.dingux-vectrex-ipk/control.tar.gz /tmp/.dingux-vectrex-ipk/data.tar.gz /tmp/.dingux-vectrex-ipk/debian-binary

opk: $(TARGET)
	@mksquashfs \
	dingux-vectrex/default.retrofw.desktop \
	dingux-vectrex/dingux-vectrex.dge \
	dingux-vectrex/graphics \
	dingux-vectrex/background.png \
	dingux-vectrex/dingux-vectrex.png \
	dingux-vectrex/default.bin \
	dingux-vectrex/dingux-vectrex.opk \
	-all-root -noappend -no-exports -no-xattrs
