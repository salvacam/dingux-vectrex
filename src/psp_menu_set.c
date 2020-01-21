/*
 *  Copyright (C) 2006 Ludovic Jacomme (ludovic.jacomme@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "global.h"
#include "psp_sdl.h"
#include "psp_kbd.h"
#include "psp_menu.h"
#include "psp_fmgr.h"
#include "psp_menu_kbd.h"
#include "psp_menu_set.h"

extern SDL_Surface *back_surface;

enum{
MENU_SET_SOUND,
//MENU_SET_VOLUME,
MENU_SET_VIEW_FPS,
MENU_SET_SPEED_LIMIT,
MENU_SET_SKIP_FPS,
//MENU_SET_CLOCK,
MENU_SET_RENDER,
MENU_SET_OVERLAY,
MENU_SET_DELTA_X,
MENU_SET_DELTA_Y,
MENU_SET_COLOR,
MENU_SET_AUTOFIRE,

MENU_KEYBOARD,

MENU_SET_LOAD,
MENU_SET_SAVE,
MENU_SET_RESET,

// MENU_SET_BACK,

MAX_MENU_SET_ITEM
};

  static menu_item_t menu_list[] =
  {
    { "Sound enable       :"},
    //{ "Sound Volume       :"},
    { "Display fps        :"},
    { "Speed limiter      :"},
    { "Skip frame         :"},
    //{ "Clock frequency    :"},
    { "Render mode        :"},
    { "Use overlay        :"},
    { "Delta X            :"},
    { "Delta Y            :"},
    { "Color              :"},
    { "Auto fire period   :"},

  { "Keyboard" },

    { "Load settings"        },
    { "Save settings"        },
    { "Reset settings"       },
    // { "Back to Menu"         }
  };

  static int cur_menu_id = 0;

  static int dve_snd_enable        = 0;
  static int dve_speed_limiter     = 50;
  static int dve_view_fps          = 0;
  static int dve_color             = 0;
  static int dve_render_mode       = 0;
  static int dve_overlay_mode      = 0;
  static int dve_delta_x           = 0;
  static int dve_delta_y           = 0;
  static int dve_auto_fire_period  = 0;
  static int psp_cpu_clock         = GP2X_DEF_CLOCK;
  static int dve_skip_fps          = 0;
  static int psp_sound_volume     = 1;

static void
psp_settings_menu_reset(void);

static void
psp_display_screen_settings_menu(void)
{
  char buffer[64];
  int menu_id = 0;
  int color   = 0;
  int x       = 0;
  int y       = 0;
  int y_step  = 0;

  psp_sdl_blit_help();

  x      = 5;
  y      = 15;
  y_step = 10;

  for (menu_id = 0; menu_id < MAX_MENU_SET_ITEM; menu_id++) {
    color = PSP_MENU_TEXT_COLOR;
    if (cur_menu_id == menu_id) color = PSP_MENU_SEL_COLOR;

    psp_sdl_back2_print(x, y, menu_list[menu_id].title, color);

    if (menu_id == MENU_SET_SOUND) {
      if (dve_snd_enable) strcpy(buffer,"yes");
      else                strcpy(buffer,"no ");
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_OVERLAY) {
      if (dve_overlay_mode) strcpy(buffer,"yes");
      else                strcpy(buffer,"no ");
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_VIEW_FPS) {
      if (dve_view_fps) strcpy(buffer,"on ");
      else              strcpy(buffer,"off");
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_COLOR) {
      switch(dve_color){
        case COLOR_BLUE:
          strcpy(buffer,"blue");
          break;
        case COLOR_RED:
          strcpy(buffer,"red");
          break;
        case COLOR_GREEN:
          strcpy(buffer,"green");
          break;
        case COLOR_GRAY:
          strcpy(buffer,"gray");
          break;
        case COLOR_YELLOW:
          strcpy(buffer,"yellow");
          break;
      }
      string_fill_with_space(buffer, 5);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_AUTOFIRE) {
      sprintf(buffer,"%d", dve_auto_fire_period+1);
      string_fill_with_space(buffer, 7);
      psp_sdl_back2_print(135, y, buffer, color);
      y += y_step;
    } else
    /*if (menu_id == MENU_SET_VOLUME) {
      sprintf(buffer,"%d", psp_sound_volume);
      string_fill_with_space(buffer, 7);
      psp_sdl_back2_print(140, y, buffer, color);
    } else*/
    if (menu_id == MENU_SET_SKIP_FPS) {
      sprintf(buffer,"%d", dve_skip_fps);
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_DELTA_X) {
      sprintf(buffer,"%d", dve_delta_x);
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_DELTA_Y) {
      sprintf(buffer,"%d", dve_delta_y);
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_SPEED_LIMIT) {
      if (dve_speed_limiter == 0)  strcpy(buffer, "no");
      else sprintf(buffer, "%d fps", dve_speed_limiter);
      string_fill_with_space(buffer, 7);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    if (menu_id == MENU_SET_RENDER) {
      if (dve_render_mode == DVE_RENDER_NORMAL   ) strcpy(buffer, "normal");
      else                                         strcpy(buffer, "rot90");

      string_fill_with_space(buffer, 13);
      psp_sdl_back2_print(135, y, buffer, color);
    } else
    /*if (menu_id == MENU_SET_CLOCK) {
      sprintf(buffer,"%d", psp_cpu_clock);
      string_fill_with_space(buffer, 4);
      psp_sdl_back2_print(140, y, buffer, color);
    } else*/
    if (menu_id == MENU_KEYBOARD || menu_id == MENU_SET_AUTOFIRE) {
      y += y_step;
    }

    y += y_step;
  }

  psp_menu_display_save_name();
}

static void
psp_settings_menu_skip_fps(int step)
{
  if (step > 0) {
    if (dve_skip_fps < 25) dve_skip_fps++;
  } else {
    if (dve_skip_fps > 0) dve_skip_fps--;
  }
}

#define MAX_CLOCK_VALUES 7
/*static int clock_values[MAX_CLOCK_VALUES] = { 466, 533, 633, 666, 700, 766, 800 };

static void
psp_settings_menu_clock(int step)
{
  int index;
  for (index = 0; index < MAX_CLOCK_VALUES; index++) {
    if (psp_cpu_clock <= clock_values[index]) break;
  }
  if (step > 0) {
    index++;
    if (index >= MAX_CLOCK_VALUES) index = 0;
    psp_cpu_clock = clock_values[index];

  } else {
    index--;

    if (index < 0) index = MAX_CLOCK_VALUES - 1;
    psp_cpu_clock = clock_values[index];
  }
}*/

static void
psp_settings_menu_autofire(int step)
{
  if (step > 0) {
    if (dve_auto_fire_period < 19) dve_auto_fire_period++;
  } else {
    if (dve_auto_fire_period >  0) dve_auto_fire_period--;
  }
}

static void
psp_settings_menu_delta_y(int step)
{
  if (step > 0) {
    if (dve_delta_y <  60) dve_delta_y++;
  } else {
    if (dve_delta_y >   0) dve_delta_y--;
  }
}

static void
psp_settings_menu_delta_x(int step)
{
  if (step > 0) {
    if (dve_delta_x <  60) dve_delta_x++;
  } else {
    if (dve_delta_x >   0) dve_delta_x--;
  }
}

void
psp_settings_menu_render(int step)
{
  if (step > 0) {
    if (dve_render_mode < DVE_LAST_RENDER) dve_render_mode++;
    else                                   dve_render_mode = 0;
  } else {
    if (dve_render_mode > 0) dve_render_mode--;
    else                     dve_render_mode = DVE_LAST_RENDER;
  }
}

static void
psp_settings_menu_limiter(int step)
{
  if (step > 0) {
    if (dve_speed_limiter < 50) dve_speed_limiter++;
    else                        dve_speed_limiter = 0;
  } else {
    if (dve_speed_limiter > 0) dve_speed_limiter--;
    else                       dve_speed_limiter = 50;
  }
}

/*static void
psp_settings_menu_volume(int step)
{
  if (step > 0) {
    if (psp_sound_volume < 10) psp_sound_volume++;
    else                       psp_sound_volume = 1;
  } else {
    if (psp_sound_volume >  1) psp_sound_volume--;
    else                       psp_sound_volume = 10;
  }
}*/


static void
psp_settings_menu_init(void)
{
  dve_snd_enable       = DVE.dve_snd_enable;
  dve_view_fps         = DVE.dve_view_fps;
  dve_color            = DVE.dve_color;
  dve_speed_limiter    = DVE.dve_speed_limiter;
  dve_render_mode      = DVE.dve_render_mode;
  dve_overlay_mode     = DVE.dve_overlay_mode;
  dve_delta_x          = DVE.dve_delta_x;
  dve_delta_y          = DVE.dve_delta_y;
  dve_auto_fire_period = DVE.dve_auto_fire_period;
  dve_skip_fps         = DVE.psp_skip_max_frame;
  //psp_sound_volume     = DVE.psp_sound_volume;
  //psp_cpu_clock        = DVE.psp_cpu_clock;
}

static void
psp_settings_menu_load(int format)
{
  int ret;

  ret = psp_fmgr_menu(format);
  if (ret ==  1) /* load OK */
  {
    psp_display_screen_settings_menu();
    psp_sdl_back2_print(165, 110, "File loaded !",
                       PSP_MENU_NOTE_COLOR);
    psp_sdl_flip();
    sleep(1);
    psp_settings_menu_init();
  }
  else
  if (ret == -1) /* Load Error */
  {
    psp_display_screen_settings_menu();
    psp_sdl_back2_print(165, 110, "Can't load file !",
                       PSP_MENU_WARNING_COLOR);
    psp_sdl_flip();
    sleep(1);
  }
}

static void
psp_settings_menu_validate(void)
{
  DVE.dve_snd_enable        = dve_snd_enable;
  DVE.dve_view_fps          = dve_view_fps;
  //DVE.psp_cpu_clock         = psp_cpu_clock;
  DVE.psp_skip_max_frame    = dve_skip_fps;
  DVE.dve_speed_limiter     = dve_speed_limiter;
  DVE.dve_delta_x           = dve_delta_x;
  DVE.dve_delta_y           = dve_delta_y;
  DVE.psp_skip_cur_frame    = 0;
  DVE.dve_auto_fire_period  = dve_auto_fire_period;
  DVE.dve_overlay_mode      = dve_overlay_mode;
  //DVE.psp_sound_volume    = psp_sound_volume;

  dve_change_render_mode( dve_render_mode );
  dve_change_color( dve_color );
  //gp2xPowerSetClockFrequency(DVE.psp_cpu_clock);
}


static void
psp_settings_menu_save()
{
  int error;

  psp_settings_menu_validate();
  error = dve_save_settings();

  if (! error) /* save OK */
  {
    psp_display_screen_settings_menu();
    psp_sdl_back2_print(165, 110, "File saved !",
                       PSP_MENU_NOTE_COLOR);
    psp_sdl_flip();
    sleep(1);
  }
  else
  {
    psp_display_screen_settings_menu();
    psp_sdl_back2_print(165, 110, "Can't save file !",
                       PSP_MENU_WARNING_COLOR);
    psp_sdl_flip();
    sleep(1);
  }
}

static void
psp_settings_menu_reset(void)
{
  psp_display_screen_settings_menu();
  psp_sdl_back2_print(165, 110, "Reset Settings !",
                     PSP_MENU_WARNING_COLOR);
  psp_sdl_flip();
  dve_default_settings();
  psp_settings_menu_init();
  sleep(1);
}

int
psp_settings_menu(void)
{
  gp2xCtrlData c;
  long        new_pad;
  long        old_pad;
  int         last_time;
  int         end_menu;

  psp_kbd_wait_no_button();

  old_pad   = 0;
  last_time = 0;
  end_menu  = 0;

  psp_settings_menu_init();

  while (! end_menu)
  {
    psp_display_screen_settings_menu();
    psp_sdl_flip();

    while (1)
    {
      gp2xCtrlPeekBufferPositive(&c, 1);
      gp2xCtrlConvertKeyboardSym( &c );
      c.Buttons &= PSP_ALL_BUTTON_MASK;

      if (c.Buttons) break;
    }

    new_pad = c.Buttons;

    if ((old_pad != new_pad) || ((c.TimeStamp - last_time) > PSP_MENU_MIN_TIME)) {
      last_time = c.TimeStamp;
      old_pad = new_pad;

    } else continue;

    if ((c.Buttons & GP2X_CTRL_RTRIGGER) == GP2X_CTRL_RTRIGGER) {
      psp_settings_menu_reset();
      end_menu = 1;
    } else
    if ((new_pad == GP2X_CTRL_LEFT ) || (new_pad == GP2X_CTRL_RIGHT))
    {
      int step = 1;
      if (new_pad & GP2X_CTRL_LEFT) step = -1;

      switch (cur_menu_id )
      {
        case MENU_SET_SOUND     : dve_snd_enable = ! dve_snd_enable;
        break;
        case MENU_SET_SPEED_LIMIT : psp_settings_menu_limiter( step );
        break;
        case MENU_SET_VIEW_FPS    : dve_view_fps     = ! dve_view_fps;
        break;
        case MENU_SET_COLOR       :
          switch(dve_color){
            case COLOR_BLUE:
              dve_color = COLOR_RED;
              break;
            case COLOR_RED:
              dve_color = COLOR_GREEN;
              break;
            case COLOR_GREEN:
              dve_color = COLOR_GRAY;
              break;
            case COLOR_GRAY:
              dve_color = COLOR_YELLOW;
              break;
            case COLOR_YELLOW:
              dve_color = COLOR_BLUE;
              break;
          }
        break;
        case MENU_SET_OVERLAY     : dve_overlay_mode = ! dve_overlay_mode;
        break;
        /*case MENU_SET_VOLUME     : psp_settings_menu_volume( step );
        break;  */
        case MENU_SET_SKIP_FPS  : psp_settings_menu_skip_fps( step );
        break;
        case MENU_SET_RENDER    : psp_settings_menu_render( step );
        break;
        case MENU_SET_DELTA_X   : psp_settings_menu_delta_x( step );
        break;
        case MENU_SET_DELTA_Y   : psp_settings_menu_delta_y( step );
        break;
        case MENU_SET_AUTOFIRE  : psp_settings_menu_autofire( step );
        break;
    if ((new_pad == GP2X_CTRL_CIRCLE))
       /* case MENU_SET_CLOCK     : psp_settings_menu_clock( step );
        break;*/
        case MENU_SET_LOAD       : psp_settings_menu_load(FMGR_FORMAT_SET);
                                   old_pad = new_pad = 0;
        break;
        case MENU_SET_SAVE       : psp_settings_menu_save();
                                   old_pad = new_pad = 0;
        break;
        case MENU_SET_RESET      : psp_settings_menu_reset();
        break;

        case MENU_SET_BACK       : end_menu = 1;
        break;
      }

    } else
    if(new_pad & GP2X_CTRL_UP) {

      if (cur_menu_id > 0) cur_menu_id--;
      else                 cur_menu_id = MAX_MENU_SET_ITEM-1;

    } else
    if(new_pad & GP2X_CTRL_DOWN) {

      if (cur_menu_id < (MAX_MENU_SET_ITEM-1)) cur_menu_id++;
      else                                     cur_menu_id = 0;

    } else
    if(new_pad & GP2X_CTRL_SQUARE) {
      /* Cancel */
      end_menu = -1;
    } else
    if((new_pad & GP2X_CTRL_CROSS) || (new_pad & GP2X_CTRL_SELECT)) {
      /* Back to DVE */
      end_menu = 1;
    }
  }

  if (end_menu > 0) {
    psp_settings_menu_validate();
  }

  psp_kbd_wait_no_button();

  psp_sdl_clear_screen( PSP_MENU_BLACK_COLOR );
  psp_sdl_flip();
  psp_sdl_clear_screen( PSP_MENU_BLACK_COLOR );
  psp_sdl_flip();

  return 1;
}

