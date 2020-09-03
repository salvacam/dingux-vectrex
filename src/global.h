#ifndef _GLOBAL_H_
#define _GLOBAL_H_

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#include "gp2x_psp.h"
#include "gp2x_cpu.h"
#include <time.h>

#define PSP_FMGR_MAX_PATH    512
#define PSP_FMGR_MAX_NAME    256
#define PSP_FMGR_MAX_ENTRY  2048

char user_filename[PSP_FMGR_MAX_NAME];

#define COLOR_BLUE    1
#define COLOR_RED     2
#define COLOR_GREEN   3
#define COLOR_GRAY    4
#define COLOR_YELLOW  5

# ifndef CLK_TCK
# define CLK_TCK  CLOCKS_PER_SEC
# endif

#define CPU_CLOCK_IDLE 60
#define CPU_CLOCK_STD 200

//LUDO:
# define DVE_RENDER_NORMAL     0
# define DVE_RENDER_ROT90      1
# define DVE_LAST_RENDER       1

# define MAX_PATH   256
# define DVE_MAX_SAVE_STATE 5
# define DVE_MAX_CHEAT      10

#define DVE_CHEAT_NONE    0
#define DVE_CHEAT_ENABLE  1
#define DVE_CHEAT_DISABLE 2

#define DVE_CHEAT_COMMENT_SIZE 25
#define DVE_CHEAT_RAM_SIZE   1024

  typedef struct DVE_cheat_t {
    unsigned char  type;
    unsigned short addr;
    unsigned char  value;
    char           comment[DVE_CHEAT_COMMENT_SIZE];
  } DVE_cheat_t;

# define BLIT_WIDTH   300
# define BLIT_HEIGHT  300

# define SNAP_HEIGHT  100
# define SNAP_WIDTH    80

#include <SDL/SDL.h>

  typedef unsigned char byte;
  typedef unsigned short word;
  typedef unsigned int dword;
  typedef unsigned long long ddword;

  typedef struct DVE_save_t {

    SDL_Surface    *surface;
    char            used;
    char            thumb;
    time_t          date;

  } DVE_save_t;

  typedef struct DVE_t {

    DVE_save_t  dve_save_state[DVE_MAX_SAVE_STATE];
    DVE_cheat_t dve_cheat[DVE_MAX_CHEAT];
    char        dve_save_name[MAX_PATH];
    char        dve_home_dir[MAX_PATH];
    int         comment_present;
    int         dve_speed_limiter;
    int         psp_screenshot_id;
    int         psp_sound_volume;
    int         psp_cpu_clock;
    int         dve_color;
    int         dve_snd_enable;
    int         dve_view_fps;
    int         dve_current_fps;
    int         dve_render_mode;
    int         dve_overlay_mode;
    int         dve_delta_x;
    int         dve_delta_y;
    int         psp_skip_max_frame;
    int         psp_skip_cur_frame;
    int         dve_auto_fire_period;
    int         dve_auto_fire;
    int         dve_auto_fire_pressed;
    int         overlay_loaded;

    // extra config
    char      rom_directory[MAX_PATH];
    int       font_size;

  } DVE_t;

  extern DVE_t DVE;


//END_LUDO:

# endif
