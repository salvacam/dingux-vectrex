TARGET = dingux-vectrex

CROSS_COMPILE	= /opt/gcw0-toolchain/usr/bin/mipsel-linux-
CC				= $(CROSS_COMPILE)gcc
CXX				= $(CROSS_COMPILE)g++
STRIP			= $(CROSS_COMPILE)strip

SYSROOT		= $(shell $(CC) --print-sysroot)
SDL_CFLAGS	= $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS	= $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

ifdef V
	CMD =
	SUM = @\#
else
	CMD = @
	SUM = @echo
endif

INCLUDES	= -I.
CFLAGS		= $(SDL_CFLAGS) $(INCLUDES) -DGCW0_MODE -g -O3 -Wall -fstrength-reduce -flto -s -DSOUND_SUPPORT
CXXFLAGS	= $(CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS		= $(CXXFLAGS) -lpthread -lz -lpng -lm -lgcc -lSDL_image $(SDL_LIBS)

OBJS = \
	gp2x_psp.o \
	e6809.o \
	e8910.o  \
	osint.o \
	vecx.o \
	global.o \
	psp_main.o \
	psp_sdl.o \
	psp_kbd.o \
	psp_dve.o \
	psp_fmgr.o \
	psp_font.o \
	psp_danzeff.o \
	psp_editor.o \
	psp_menu_set.o \
	psp_menu_cheat.o \
	psp_menu_list.o \
	psp_menu.o \
	psp_menu_kbd.o \
	psp_menu_help.o

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