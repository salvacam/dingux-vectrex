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

#include <SDL/SDL.h>

#include "global.h"
#include "psp_dve.h"
#include "vecx.h"

#include "psp_kbd.h"
#include "psp_menu.h"
#include "psp_sdl.h"
#include "psp_danzeff.h"
#include "psp_fmgr.h"

# define KBD_MIN_ANALOG_TIME      150000
# define KBD_MIN_START_TIME      1500000
# define KBD_MAX_EVENT_TIME       500000
# define KBD_MIN_PENDING_TIME     300000
# define KBD_MIN_IR_PENDING_TIME  100000
//# define KBD_MIN_DANZEFF_TIME   150000
# define KBD_MIN_DANZEFF_TIME      10000
# define KBD_MIN_COMMAND_TIME     100000
# define KBD_MIN_BATTCHECK_TIME 90000000
# define KBD_MIN_AUTOFIRE_TIME   1000000

 static gp2xCtrlData    loc_button_data;
 static unsigned int   loc_last_event_time = 0;
 static long           first_time_stamp = -1;
 static long           first_time_auto_stamp = -1;
 static char           loc_button_press[ KBD_MAX_BUTTONS ];
 static char           loc_button_release[ KBD_MAX_BUTTONS ];
 static unsigned int   loc_button_mask[ KBD_MAX_BUTTONS ] =
 {
   GP2X_CTRL_UP         , /*  KBD_UP         */
   GP2X_CTRL_RIGHT      , /*  KBD_RIGHT      */
   GP2X_CTRL_DOWN       , /*  KBD_DOWN       */
   GP2X_CTRL_LEFT       , /*  KBD_LEFT       */
   GP2X_CTRL_TRIANGLE   , /*  KBD_TRIANGLE   */
   GP2X_CTRL_CIRCLE     , /*  KBD_CIRCLE     */
   GP2X_CTRL_CROSS      , /*  KBD_CROSS      */
   GP2X_CTRL_SQUARE     , /*  KBD_SQUARE     */
   GP2X_CTRL_SELECT     , /*  KBD_SELECT     */
   GP2X_CTRL_START      , /*  KBD_START      */
   GP2X_CTRL_LTRIGGER   , /*  KBD_LTRIGGER   */
   GP2X_CTRL_RTRIGGER   , /*  KBD_RTRIGGER   */
 };

 static unsigned int   loc_button_mask_rot90[ KBD_MAX_BUTTONS ] =
 {
   GP2X_CTRL_RIGHT      , /*  KBD_UP         */
   GP2X_CTRL_DOWN       , /*  KBD_RIGHT      */
   GP2X_CTRL_LEFT       , /*  KBD_DOWN       */
   GP2X_CTRL_UP         , /*  KBD_LEFT       */
   GP2X_CTRL_TRIANGLE   , /*  KBD_TRIANGLE   */
   GP2X_CTRL_CIRCLE     , /*  KBD_CIRCLE     */
   GP2X_CTRL_CROSS      , /*  KBD_CROSS      */
   GP2X_CTRL_SQUARE     , /*  KBD_SQUARE     */
   GP2X_CTRL_SELECT     , /*  KBD_SELECT     */
   GP2X_CTRL_START      , /*  KBD_START      */
   GP2X_CTRL_LTRIGGER   , /*  KBD_LTRIGGER   */
   GP2X_CTRL_RTRIGGER   , /*  KBD_RTRIGGER   */
 };

 static char loc_button_name[ KBD_ALL_BUTTONS ][20] =
 {
   "UP",
   "RIGHT",
   "DOWN",
   "LEFT",
# if defined(DINGUX_MODE)  || defined(GCW0_MODE)
   "X",      // Triangle
   "A",      // Circle
   "B",      // Cross
   "Y",      // Square
# else
   "Y",      // Triangle
   "B",      // Circle
   "X",      // Cross
   "A",      // Square
# endif
   "SELECT",
   "START",
   "LTRIGGER",
   "RTRIGGER",
   "JOY_UP",
   "JOY_RIGHT",
   "JOY_DOWN",
   "JOY_LEFT"
 };

 static char loc_button_name_L[ KBD_ALL_BUTTONS ][20] =
 {
   "L_UP",
   "L_RIGHT",
   "L_DOWN",
   "L_LEFT",
# if defined(DINGUX_MODE) || defined(GCW0_MODE)
   "L_X",      // Triangle
   "L_A",      // Circle
   "L_B",      // Cross
   "L_Y",      // Square
# else
   "L_Y",      // Triangle
   "L_B",      // Circle
   "L_X",      // Cross
   "L_A",      // Square
# endif
   "L_SELECT",
   "L_START",
   "L_LTRIGGER",
   "L_RTRIGGER",
   "L_JOY_UP",
   "L_JOY_RIGHT",
   "L_JOY_DOWN",
   "L_JOY_LEFT"
 };

  static char loc_button_name_R[ KBD_ALL_BUTTONS ][20] =
 {
   "R_UP",
   "R_RIGHT",
   "R_DOWN",
   "R_LEFT",
# if defined(DINGUX_MODE) || defined(GCW0_MODE)
   "R_X",      // Triangle
   "R_A",      // Circle
   "R_B",      // Cross
   "R_Y",      // Square
# else
   "R_Y",      // Triangle
   "R_B",      // Circle
   "R_X",      // Cross
   "R_A",      // Square
# endif
   "R_SELECT",
   "R_START",
   "R_LTRIGGER",
   "R_RTRIGGER",
   "R_JOY_UP",
   "R_JOY_RIGHT",
   "R_JOY_DOWN",
   "R_JOY_LEFT"
 };

  struct dve_key_trans psp_dve_key_info[DVE_MAX_KEY]=
  {
    // DVE            MASK    NAME
    { DVE_JOY_1,      0x01,   "1" },
    { DVE_JOY_2,      0x02,   "2" },
    { DVE_JOY_3,      0x04,   "3" },
    { DVE_JOY_4,      0x08,   "4" },
    { DVE_JOY_UP,     0x01,   "JOY_UP" },
    { DVE_JOY_DOWN,   0x02,   "JOY_DOWN" },
    { DVE_JOY_LEFT,   0x04,   "JOY_LEFT" },
    { DVE_JOY_RIGHT,  0x08,   "JOY_RIGHT" },
    { DVE_C_FPS,      0x00,   "C_FPS" },
    { DVE_C_RENDER,   0x00,   "C_RENDER" },
    { DVE_C_LOAD,     0x00,   "C_LOAD" },
    { DVE_C_SAVE,     0x00,   "C_SAVE" },
    { DVE_C_RESET,    0x00,   "C_RESET" },
    { DVE_C_AUTOFIRE, 0x00,   "C_AUTOFIRE" },
    { DVE_C_INCFIRE,  0x00,   "C_INCFIRE" },
    { DVE_C_DECFIRE,  0x00,   "C_DECFIRE" },
    { DVE_C_SCREEN,   0x00,   "C_SCREEN" }
  };

  static int loc_default_mapping[ KBD_ALL_BUTTONS ] = {
    DVE_JOY_UP          , /*  KBD_UP         */
    DVE_JOY_RIGHT       , /*  KBD_RIGHT      */
    DVE_JOY_DOWN        , /*  KBD_DOWN       */
    DVE_JOY_LEFT        , /*  KBD_LEFT       */
    DVE_JOY_2           , /*  KBD_TRIANGLE   */
    DVE_JOY_3           , /*  KBD_CIRCLE     */
    DVE_JOY_4           , /*  KBD_CROSS      */
    DVE_JOY_1           , /*  KBD_SQUARE     */
    -1                  , /*  KBD_SELECT     */
    -1                  , /*  KBD_START      */
    KBD_LTRIGGER_MAPPING, /*  KBD_LTRIGGER   */
    KBD_RTRIGGER_MAPPING, /*  KBD_RTRIGGER   */
    DVE_JOY_UP          , /*  KBD_JOY_UP     */
    DVE_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
    DVE_JOY_DOWN        , /*  KBD_JOY_DOWN   */
    DVE_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

  static int loc_default_mapping_L[ KBD_ALL_BUTTONS ] = {
    DVE_JOY_UP          , /*  KBD_UP         */
    DVE_C_RENDER        , /*  KBD_RIGHT      */
    DVE_JOY_DOWN        , /*  KBD_DOWN       */
    DVE_C_RENDER        , /*  KBD_LEFT       */
    DVE_C_LOAD          , /*  KBD_TRIANGLE   */
    DVE_C_FPS           , /*  KBD_CIRCLE     */
    DVE_C_SAVE          , /*  KBD_CROSS      */
    DVE_C_FPS           , /*  KBD_SQUARE     */
    -1                  , /*  KBD_SELECT     */
    -1                  , /*  KBD_START      */
    KBD_LTRIGGER_MAPPING, /*  KBD_LTRIGGER   */
    KBD_RTRIGGER_MAPPING, /*  KBD_RTRIGGER   */
    DVE_JOY_UP          , /*  KBD_JOY_UP     */
    DVE_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
    DVE_JOY_DOWN        , /*  KBD_JOY_DOWN   */
    DVE_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

  static int loc_default_mapping_R[ KBD_ALL_BUTTONS ] = {
    DVE_JOY_UP          , /*  KBD_UP         */
    DVE_C_INCFIRE       , /*  KBD_RIGHT      */
    DVE_JOY_DOWN        , /*  KBD_DOWN       */
    DVE_C_DECFIRE       , /*  KBD_LEFT       */
    DVE_JOY_2           , /*  KBD_TRIANGLE   */
    DVE_JOY_3           , /*  KBD_CIRCLE     */
    DVE_C_AUTOFIRE      , /*  KBD_CROSS      */
    DVE_JOY_1           , /*  KBD_SQUARE     */
    -1                  , /*  KBD_SELECT     */
    -1                  , /*  KBD_START      */
    KBD_LTRIGGER_MAPPING, /*  KBD_LTRIGGER   */
    KBD_RTRIGGER_MAPPING, /*  KBD_RTRIGGER   */
    DVE_JOY_UP          , /*  KBD_JOY_UP     */
    DVE_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
    DVE_JOY_DOWN        , /*  KBD_JOY_DOWN   */
    DVE_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

# define KBD_MAX_ENTRIES 4

  int kbd_layout[KBD_MAX_ENTRIES][2] = {
    /* Key            Ascii */
    { DVE_JOY_1,      '1' },
    { DVE_JOY_2,      '2' },
    { DVE_JOY_3,      '3' },
    { DVE_JOY_4,      '4' }
  };

 int psp_kbd_mapping[ KBD_ALL_BUTTONS ];
 int psp_kbd_mapping_L[ KBD_ALL_BUTTONS ];
 int psp_kbd_mapping_R[ KBD_ALL_BUTTONS ];
 int psp_kbd_presses[ KBD_ALL_BUTTONS ];
 int kbd_ltrigger_mapping_active;
 int kbd_rtrigger_mapping_active;

 static int danzeff_dve_key     = 0;
 static int danzeff_dve_pending = 0;
        int danzeff_mode        = 0;

#ifdef USE_PSP_IRKEYB
# define IRKEYB_DVE_MAX_PENDING 20
 static int irkeyb_dve_pending_max  = 0;
 static int irkeyb_dve_pending_curr = 0;
 static int irkeyb_dve_pending_keys[IRKEYB_DVE_MAX_PENDING];
# endif

int
dve_key_event(int dve_idx, int press)
{
  int bit_mask = 0;

  if ((dve_idx < 0) || (dve_idx >= DVE_MAX_KEY)) {
    return -1;
  }

  if ((dve_idx >= DVE_C_FPS) &&
      (dve_idx <= DVE_C_SCREEN)) {
    if (press) {
      dve_treat_command_key(dve_idx);
    }
    return 0;
  }

  if ((dve_idx >= DVE_JOY_UP) &&
      (dve_idx <= DVE_JOY_RIGHT)) {
    bit_mask = psp_dve_key_info[dve_idx].bit_mask;
    if (press) {
      dve_joy_down( bit_mask );
    } else {
      dve_joy_up( bit_mask );
    }
  } else {
    bit_mask = psp_dve_key_info[dve_idx].bit_mask;

    if (press) {
      dve_key_down( bit_mask );
    } else {
      dve_key_up( bit_mask );
    }

  }
  return 0;
}
int
dve_kbd_reset()
{
  return 0;
}

int
dve_get_key_from_ascii(int key_ascii)
{
  int index;
  for (index = 0; index < KBD_MAX_ENTRIES; index++) {
   if (kbd_layout[index][1] == key_ascii) return kbd_layout[index][0];
  }
  return -1;
}

void
psp_kbd_default_settings()
{
  memcpy(psp_kbd_mapping  , loc_default_mapping, sizeof(loc_default_mapping));
  memcpy(psp_kbd_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_L));
  memcpy(psp_kbd_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));
}

int
psp_kbd_reset_mapping(void)
{
  memcpy(psp_kbd_mapping  , loc_default_mapping, sizeof(loc_default_mapping));
  memcpy(psp_kbd_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_L));
  memcpy(psp_kbd_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));
  return 0;
}

int
psp_kbd_reset_hotkeys(void)
{
  int index;
  int key_id;
  for (index = 0; index < KBD_ALL_BUTTONS; index++) {
    key_id = loc_default_mapping[index];
    if ((key_id >= DVE_C_FPS) && (key_id <= DVE_C_SCREEN)) {
      psp_kbd_mapping[index] = key_id;
    }
    key_id = loc_default_mapping_L[index];
    if ((key_id >= DVE_C_FPS) && (key_id <= DVE_C_SCREEN)) {
      psp_kbd_mapping_L[index] = key_id;
    }
    key_id = loc_default_mapping_R[index];
    if ((key_id >= DVE_C_FPS) && (key_id <= DVE_C_SCREEN)) {
      psp_kbd_mapping_R[index] = key_id;
    }
  }
  return 0;
}

int
psp_kbd_load_mapping_file(FILE *KbdFile)
{
  char     Buffer[512];
  char    *Scan;
  int      tmp_mapping[KBD_ALL_BUTTONS];
  int      tmp_mapping_L[KBD_ALL_BUTTONS];
  int      tmp_mapping_R[KBD_ALL_BUTTONS];
  int      dve_key_id = 0;
  int      kbd_id = 0;

  memcpy(tmp_mapping  , loc_default_mapping  , sizeof(loc_default_mapping));
  memcpy(tmp_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_R));
  memcpy(tmp_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));

  while (fgets(Buffer,512,KbdFile) != (char *)0) {

      Scan = strchr(Buffer,'\n');
      if (Scan) *Scan = '\0';
      /* For this #@$% of windows ! */
      Scan = strchr(Buffer,'\r');
      if (Scan) *Scan = '\0';
      if (Buffer[0] == '#') continue;

      Scan = strchr(Buffer,'=');
      if (! Scan) continue;

      *Scan = '\0';
      dve_key_id = atoi(Scan + 1);

      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,loc_button_name[kbd_id])) {
          tmp_mapping[kbd_id] = dve_key_id;
          //break;
        }
      }
      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,loc_button_name_L[kbd_id])) {
          tmp_mapping_L[kbd_id] = dve_key_id;
          //break;
        }
      }
      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,loc_button_name_R[kbd_id])) {
          tmp_mapping_R[kbd_id] = dve_key_id;
          //break;
        }
      }
  }

  memcpy(psp_kbd_mapping, tmp_mapping, sizeof(psp_kbd_mapping));
  memcpy(psp_kbd_mapping_L, tmp_mapping_L, sizeof(psp_kbd_mapping_L));
  memcpy(psp_kbd_mapping_R, tmp_mapping_R, sizeof(psp_kbd_mapping_R));

  return 0;
}

int
psp_kbd_load_mapping(char *kbd_filename)
{
  FILE    *KbdFile;
  int      error = 0;

  KbdFile = fopen(kbd_filename, "r");
  error   = 1;

  if (KbdFile != (FILE*)0) {
    psp_kbd_load_mapping_file(KbdFile);
    error = 0;
    fclose(KbdFile);
  }

  kbd_ltrigger_mapping_active = 0;
  kbd_rtrigger_mapping_active = 0;

  return error;
}

int
psp_kbd_save_mapping(char *kbd_filename)
{
  FILE    *KbdFile;
  int      kbd_id = 0;
  int      error = 0;

  KbdFile = fopen(kbd_filename, "w");
  error   = 1;

  if (KbdFile != (FILE*)0) {

    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", loc_button_name[kbd_id], psp_kbd_mapping[kbd_id]);
    }
    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", loc_button_name_L[kbd_id], psp_kbd_mapping_L[kbd_id]);
    }
    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", loc_button_name_R[kbd_id], psp_kbd_mapping_R[kbd_id]);
    }
    error = 0;
    fclose(KbdFile);
  }

  return error;
}

int
psp_kbd_is_danzeff_mode()
{
  return danzeff_mode;
}

int
psp_kbd_enter_danzeff()
{
  unsigned int danzeff_key = 0;
  int          dve_key     = 0;

  gp2xCtrlData  c;

  if (! danzeff_mode) {
    psp_init_keyboard();
    danzeff_mode = 1;
  }

  gp2xCtrlPeekBufferPositive(&c, 1);

  if (danzeff_dve_pending)
  {
    if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_PENDING_TIME) {
      loc_last_event_time = c.TimeStamp;
      danzeff_dve_pending = 0;
      dve_key_event(danzeff_dve_key, 0);
    }
    return 0;
  }

  if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_DANZEFF_TIME) {
    loc_last_event_time = c.TimeStamp;

    gp2xCtrlPeekBufferPositive(&c, 1);
    danzeff_key = danzeff_readInput(c);
  }

  if (danzeff_key > DANZEFF_START) {
    dve_key = dve_get_key_from_ascii(danzeff_key);

    if (dve_key != -1) {
      danzeff_dve_key     = dve_key;
      danzeff_dve_pending = 1;
      dve_key_event(danzeff_dve_key, 1);
    }

    return 1;

  } else if (danzeff_key == DANZEFF_START) {
    danzeff_mode        = 0;
    danzeff_dve_pending = 0;
    danzeff_dve_key     = 0;

    psp_kbd_wait_no_button();

  } else if (danzeff_key == DANZEFF_SELECT) {
    danzeff_mode        = 0;
    danzeff_dve_pending = 0;
    danzeff_dve_key     = 0;
    psp_main_menu();
    psp_init_keyboard();

    psp_kbd_wait_no_button();
  }

  return danzeff_key;
}

void
psp_kbd_display_active_mapping()
{
  if (kbd_ltrigger_mapping_active) {
    psp_sdl_fill_rectangle(0, 0, 10, 3, psp_sdl_rgb(0x0, 0x0, 0xff), 0);
  } else {
    psp_sdl_fill_rectangle(0, 0, 10, 3, 0x0, 0);
  }

  if (kbd_rtrigger_mapping_active) {
    psp_sdl_fill_rectangle(309, 0, 10, 3, psp_sdl_rgb(0x0, 0x0, 0xff), 0);
  } else {
    psp_sdl_fill_rectangle(309, 0, 10, 3, 0x0, 0);
  }
}

int
dve_decode_key(int psp_b, int button_pressed)
{
  int wake = 0;

  if (psp_b == KBD_START) {
     if (button_pressed) psp_kbd_enter_danzeff();
  } else
  if (psp_b == KBD_SELECT) {
    if (button_pressed) {
      psp_main_menu();
      psp_init_keyboard();
    }
  } else {

    if (psp_kbd_mapping[psp_b] >= 0) {
      wake = 1;
      if (button_pressed) {
        // Determine which buton to press first (ie which mapping is currently active)
        if (kbd_ltrigger_mapping_active) {
          // Use ltrigger mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping_L[psp_b];
          dve_key_event(psp_kbd_presses[psp_b], button_pressed);
        } else
        if (kbd_rtrigger_mapping_active) {
          // Use rtrigger mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping_R[psp_b];
          dve_key_event(psp_kbd_presses[psp_b], button_pressed);
        } else {
          // Use standard mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping[psp_b];
          dve_key_event(psp_kbd_presses[psp_b], button_pressed);
        }
      } else {
          // Determine which button to release (ie what was pressed before)
          dve_key_event(psp_kbd_presses[psp_b], button_pressed);
      }

    } else {
      if (psp_kbd_mapping[psp_b] == KBD_LTRIGGER_MAPPING) {
        kbd_ltrigger_mapping_active = button_pressed;
        kbd_rtrigger_mapping_active = 0;
      } else
      if (psp_kbd_mapping[psp_b] == KBD_RTRIGGER_MAPPING) {
        kbd_rtrigger_mapping_active = button_pressed;
        kbd_ltrigger_mapping_active = 0;
      }
    }
  }
  return 0;
}

void
kbd_change_auto_fire(int auto_fire)
{
  DVE.dve_auto_fire = auto_fire;
  if (DVE.dve_auto_fire_pressed) {
    dve_key_event(DVE_JOY_1, 0);
    DVE.dve_auto_fire_pressed = 0;
  }
}

int
kbd_scan_keyboard(void)
{
  gp2xCtrlData c;
  long        delta_stamp;
  int         event;
  int         b;

  event = 0;
  gp2xCtrlPeekBufferPositive( &c, 1 );

  if (DVE.dve_auto_fire) {
    delta_stamp = c.TimeStamp - first_time_auto_stamp;
    if ((delta_stamp < 0) ||
        (delta_stamp > (KBD_MIN_AUTOFIRE_TIME / (1 + DVE.dve_auto_fire_period)))) {
      first_time_auto_stamp = c.TimeStamp;
      int fire_key = psp_kbd_mapping[KBD_CROSS];
      dve_key_event(fire_key, DVE.dve_auto_fire_pressed);
      DVE.dve_auto_fire_pressed = ! DVE.dve_auto_fire_pressed;
    }
  }


  unsigned int* loc_button_mask_p = loc_button_mask;
  if (DVE.dve_render_mode == DVE_RENDER_ROT90) {
    loc_button_mask_p = loc_button_mask_rot90;
  }

  for (b = 0; b < KBD_MAX_BUTTONS; b++)
  {
    if (c.Buttons & loc_button_mask_p[b]) {
      if (!(loc_button_data.Buttons & loc_button_mask_p[b])) {
        loc_button_press[b] = 1;
        event = 1;
      }
    } else {
      if (loc_button_data.Buttons & loc_button_mask_p[b]) {
        loc_button_release[b] = 1;
        loc_button_press[b] = 0;
        event = 1;
      }
    }
  }
  memcpy(&loc_button_data,&c,sizeof(gp2xCtrlData));

  return event;
}

void
psp_kbd_wait_start(void)
{
  while (1)
  {
    gp2xCtrlData c;
    gp2xCtrlReadBufferPositive(&c, 1);
    gp2xCtrlConvertKeyboardSym( &c );
    if (c.Buttons & GP2X_CTRL_START) break;
  }
  psp_kbd_wait_no_button();
}

void
psp_init_keyboard(void)
{
  dve_kbd_reset();
  kbd_ltrigger_mapping_active = 0;
  kbd_rtrigger_mapping_active = 0;
}

void
psp_kbd_wait_no_button(void)
{
  gp2xCtrlData c;

  do {
   gp2xCtrlPeekBufferPositive(&c, 1);
   c.Buttons &= PSP_ALL_BUTTON_MASK;
  } while (c.Buttons != 0);
}

void
psp_kbd_wait_button(void)
{
  gp2xCtrlData c;

  do {
   gp2xCtrlReadBufferPositive(&c, 1);
   c.Buttons &= PSP_ALL_BUTTON_MASK;
  } while (c.Buttons == 0);
}

int
psp_update_keys(void)
{
  int         b;

  static char first_time = 1;
  static int release_pending = 0;

  if (first_time) {

    memcpy(psp_kbd_mapping, loc_default_mapping, sizeof(loc_default_mapping));
    memcpy(psp_kbd_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_L));
    memcpy(psp_kbd_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));

    dve_kbd_load();

    gp2xCtrlData c;
    gp2xCtrlPeekBufferPositive(&c, 1);
    c.Buttons &= PSP_ALL_BUTTON_MASK;

    if (first_time_stamp == -1) first_time_stamp = c.TimeStamp;
    if ((! c.Buttons) && ((c.TimeStamp - first_time_stamp) < KBD_MIN_START_TIME)) return 0;

    first_time      = 0;
    release_pending = 0;

    for (b = 0; b < KBD_MAX_BUTTONS; b++) {
      loc_button_release[b] = 0;
      loc_button_press[b] = 0;
    }
    gp2xCtrlPeekBufferPositive(&loc_button_data, 1);
    loc_button_data.Buttons &= PSP_ALL_BUTTON_MASK;

    if (strlen(user_filename)) {
      psp_fmgr_menu_load(psp_fmgr_getExtId(user_filename), user_filename);
    }

    psp_init_keyboard();

    return 0;
  }

  if (danzeff_mode) {
    return psp_kbd_enter_danzeff();
  }


  if (release_pending)
  {
    release_pending = 0;
    for (b = 0; b < KBD_MAX_BUTTONS; b++) {
      if (loc_button_release[b]) {
        loc_button_release[b] = 0;
        dve_decode_key(b, 0);
      }
    }
  }

  kbd_scan_keyboard();

  /* check press event */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    if (loc_button_press[b]) {
      loc_button_press[b] = 0;
      release_pending     = 0;
      dve_decode_key(b, 1);
    }
  }
  /* check release event */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    if (loc_button_release[b]) {
      release_pending = 1;
      break;
    }
  }

  return 0;
}
