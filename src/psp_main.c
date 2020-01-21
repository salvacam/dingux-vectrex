/* PSPVE - Vectrex Emulator

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <zlib.h>
#include "global.h"
#include <SDL/SDL.h>

#include <stdlib.h>
#include <stdio.h>

#define STDOUT_FILE  "stdout.txt"
#define STDERR_FILE  "stderr.txt"

extern int SDL_main(int argc, char *argv[]);

static void cleanup_output(void);

/* Remove the output files if there was no output written */
static void cleanup_output(void)
{

#if defined(GP2X_MODE) || defined(WIZ_MODE) || defined(CAANOO_MODE)
  cpu_deinit();
  chdir("/usr/gp2x");
  execl("/usr/gp2x/gp2xmenu", "/usr/gp2x/gp2xmenu", NULL);
#endif
}

int
main(int argc, char *argv[])
{
#if defined(GP2X_MODE) || defined(WIZ_MODE) || defined(CAANOO_MODE)
  cpu_init();
#endif

  if (argc > 1) strcpy(user_filename, argv[1]);
  atexit(cleanup_output);

  SDL_main(argc,argv);

  return 0;
}
