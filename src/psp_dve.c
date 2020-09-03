#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <SDL/SDL.h>

#include "global.h"
#include "vecx.h"
#include "psp_dve.h"
#include "psp_kbd.h"
#include "psp_fmgr.h"
#include "psp_sdl.h"

typedef struct {
   char *pchZipFile;
   char *pchExtension;
   char *pchFileNames;
   char *pchSelection;
   int iFiles;
   unsigned int dwOffset;
} t_zip_info;

  DVE_t DVE;

  int psp_screenshot_mode = 0;
  t_zip_info zip_info;

  byte *pbGPBuffer = NULL;


void
dve_emulator_reset()
{
  vecx_reset();
}

int
dve_key_up(int bit_mask)
{
  snd_regs[14] |= bit_mask;
  return 0;
}

int
dve_key_down(int bit_mask)
{
  snd_regs[14] &= ~bit_mask;
  return 0;
}

void
dve_reset_keyboard()
{
  snd_regs[14] |= 0xf;
  alg_jch0 = 0x80;
  alg_jch1 = 0x80;
  alg_jch2 = 0x80;
  alg_jch3 = 0x80;
}

int
dve_joy_up(int joy_id)
{
  switch (joy_id) {
    case 0x1 : alg_jch1 = 0x80; break;
    case 0x2 : alg_jch1 = 0x80; break;
    case 0x4 : alg_jch0 = 0x80; break;
    case 0x8 : alg_jch0 = 0x80; break;
  }
  return 0;
}

int
dve_joy_down(int joy_id)
{
  switch (joy_id) {
    case 0x1 : alg_jch1 = 0xff; break;
    case 0x2 : alg_jch1 = 0x00; break;
    case 0x4 : alg_jch0 = 0x00; break;
    case 0x8 : alg_jch0 = 0xff; break;
  }
  return 0;
}

void
dve_update_save_name(char *Name)
{
  char        TmpFileName[MAX_PATH];
  struct stat aStat;
  int         index;
  char       *SaveName;
  char       *Scan1;
  char       *Scan2;

  SaveName = strrchr(Name,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = Name;

  if (!strncasecmp(SaveName, "sav_", 4)) {
    Scan1 = SaveName + 4;
    Scan2 = strrchr(Scan1, '_');
    if (Scan2 && (Scan2[1] >= '0') && (Scan2[1] <= '5')) {
      strncpy(DVE.dve_save_name, Scan1, MAX_PATH);
      DVE.dve_save_name[Scan2 - Scan1] = '\0';
    } else {
      strncpy(DVE.dve_save_name, SaveName, MAX_PATH);
    }
  } else {
    strncpy(DVE.dve_save_name, SaveName, MAX_PATH);
  }

  if (DVE.dve_save_name[0] == '\0') {
    strcpy(DVE.dve_save_name,"default");
  }

  for (index = 0; index < DVE_MAX_SAVE_STATE; index++) {
    DVE.dve_save_state[index].used  = 0;
    memset(&DVE.dve_save_state[index].date, 0, sizeof(time_t));
    DVE.dve_save_state[index].thumb = 0;

    snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.snz", DVE.dve_home_dir, DVE.dve_save_name, index);
    if (! stat(TmpFileName, &aStat))
    {
      DVE.dve_save_state[index].used = 1;
      DVE.dve_save_state[index].date = aStat.st_mtime;
      snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.png", DVE.dve_home_dir, DVE.dve_save_name, index);
      if (! stat(TmpFileName, &aStat))
      {
        if (psp_sdl_load_thumb_png(DVE.dve_save_state[index].surface, TmpFileName)) {
          DVE.dve_save_state[index].thumb = 1;
        }
      }
    }
  }
}

typedef struct thumb_list {
  struct thumb_list *next;
  char              *name;
  char              *thumb;
} thumb_list;

static thumb_list* loc_head_thumb = 0;

static void
loc_del_thumb_list()
{
  while (loc_head_thumb != 0) {
    thumb_list *del_elem = loc_head_thumb;
    loc_head_thumb = loc_head_thumb->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->thumb) free( del_elem->thumb );
    free(del_elem);
  }
}

static void
loc_add_thumb_list(char* filename)
{
  thumb_list *new_elem;
  char tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".png"))) return;
  *Scan = 0;

  if (strncasecmp(save_name, "sav_", 4)) return;
  save_name += 4;

  Scan = strrchr(save_name, '_');
  if (! Scan) return;
  *Scan = 0;

  /* only one png for a give save name */
  new_elem = loc_head_thumb;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (thumb_list *)malloc( sizeof( thumb_list ) );
  new_elem->next = loc_head_thumb;
  loc_head_thumb = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->thumb = strdup( filename );
}

void
load_thumb_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_thumb_list();

  snprintf(SaveDirName, MAX_PATH, "%s/save", DVE.dve_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR)
    {
      loc_add_thumb_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

int
load_thumb_if_exists(char *Name)
{
  char        FileName[MAX_PATH];
  char        ThumbFileName[MAX_PATH];
  struct stat aStat;
  char       *SaveName;
  char       *Scan;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  thumb_list *scan_list = loc_head_thumb;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(ThumbFileName, MAX_PATH, "%s/save/%s", DVE.dve_home_dir, scan_list->thumb);
      if (! stat(ThumbFileName, &aStat))
      {
        if (psp_sdl_load_thumb_png(save_surface, ThumbFileName)) {
          return 1;
        }
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

#define ERR_INPUT_INIT           1
#define ERR_VIDEO_INIT           2
#define ERR_VIDEO_SET_MODE       3
#define ERR_VIDEO_SURFACE        4
#define ERR_VIDEO_PALETTE        5
#define ERR_VIDEO_COLOUR_DEPTH   6
#define ERR_AUDIO_INIT           7
#define ERR_AUDIO_RATE           8
#define ERR_OUT_OF_MEMORY        9
#define ERR_DVE_ROM_MISSING      10
#define ERR_NOT_A_DVE_ROM        11
#define ERR_ROM_NOT_FOUND        12
#define ERR_FILE_NOT_FOUND       13
#define ERR_FILE_BAD_ZIP         14
#define ERR_FILE_EMPTY_ZIP       15
#define ERR_FILE_UNZIP_FAILED    16
#define ERR_SNA_INVALID          17
#define ERR_SNA_SIZE             18
#define ERR_SNA_DVE_TYPE         19
#define ERR_SNA_WRITE            20
#define ERR_DSK_INVALID          21
#define ERR_DSK_SIDES            22
#define ERR_DSK_SECTORS          23
#define ERR_DSK_WRITE            24
#define MSG_DSK_ALTERED          25
#define ERR_TAP_INVALID          26
#define ERR_TAP_UNSUPPORTED      27
#define ERR_TAP_BAD_VOC          28
#define ERR_PRINTER              29
#define ERR_SDUMP                31

static dword
loc_get_dword(byte *buff)
{
  return ( (((dword)buff[3]) << 24) |
           (((dword)buff[2]) << 16) |
           (((dword)buff[1]) <<  8) |
           (((dword)buff[0]) <<  0) );
}

static void
loc_set_dword(byte *buff, dword value)
{
  buff[3] = (value >> 24) & 0xff;
  buff[2] = (value >> 16) & 0xff;
  buff[1] = (value >>  8) & 0xff;
  buff[0] = (value >>  0) & 0xff;
}

static word
loc_get_word(byte *buff)
{
  return( (((word)buff[1]) <<  8) |
          (((word)buff[0]) <<  0) );
}

int
zip_dir(t_zip_info *zi)
{
   FILE* a_file = 0;
   int n, iFileCount;
   long lFilePosition;
   dword dwCentralDirPosition, dwNextEntry;
   word wCentralDirEntries, wCentralDirSize, wFilenameLength;
   byte *pbPtr;
   char *pchStrPtr;
   dword dwOffset;

   iFileCount = 0;
   if ((a_file = fopen(zi->pchZipFile, "rb")) == NULL) {
      return ERR_FILE_NOT_FOUND;
   }

   wCentralDirEntries = 0;
   wCentralDirSize = 0;
   dwCentralDirPosition = 0;
   lFilePosition = -256;
   do {
      fseek(a_file, lFilePosition, SEEK_END);
      if (fread(pbGPBuffer, 256, 1, a_file) == 0) {
         fclose(a_file);
         return ERR_FILE_BAD_ZIP; // exit if loading of data chunck failed
      }
      pbPtr = pbGPBuffer + (256 - 22); // pointer to end of central directory (under ideal conditions)
      while (pbPtr != (byte *)pbGPBuffer) {
         if (loc_get_dword(pbPtr) == 0x06054b50) { // check for end of central directory signature
            wCentralDirEntries = loc_get_word(pbPtr + 10);
            wCentralDirSize = loc_get_word(pbPtr + 12);
            dwCentralDirPosition = loc_get_dword(pbPtr + 16);
            break;
         }
         pbPtr--; // move backwards through buffer
      }
      lFilePosition -= 256; // move backwards through ZIP file
   } while (wCentralDirEntries == 0);
   if (wCentralDirSize == 0) {
      fclose(a_file);
      return ERR_FILE_BAD_ZIP; // exit if no central directory was found
   }
   fseek(a_file, dwCentralDirPosition, SEEK_SET);
   if (fread(pbGPBuffer, wCentralDirSize, 1, a_file) == 0) {
      fclose(a_file);
      return ERR_FILE_BAD_ZIP; // exit if loading of data chunck failed
   }

   pbPtr = pbGPBuffer;
   if (zi->pchFileNames) {
      free(zi->pchFileNames); // dealloc old string table
   }
   zi->pchFileNames = (char *)malloc(wCentralDirSize); // approximate space needed by using the central directory size
   pchStrPtr = zi->pchFileNames;

   for (n = wCentralDirEntries; n; n--) {
      wFilenameLength = loc_get_word(pbPtr + 28);
      dwOffset = loc_get_dword(pbPtr + 42);
      dwNextEntry = wFilenameLength + loc_get_word(pbPtr + 30) + loc_get_word(pbPtr + 32);
      pbPtr += 46;
      char *pchThisExtension = zi->pchExtension;
      while (*pchThisExtension != '\0') { // loop for all extensions to be checked
         if (strncasecmp((char *)pbPtr + (wFilenameLength - 4), pchThisExtension, 4) == 0) {
            strncpy(pchStrPtr, (char *)pbPtr, wFilenameLength); // copy filename from zip directory
            pchStrPtr[wFilenameLength] = 0; // zero terminate string
            pchStrPtr += wFilenameLength+1;
            loc_set_dword((byte*)pchStrPtr, dwOffset);
            pchStrPtr += 4;
            iFileCount++;
            break;
         }
         pchThisExtension += 4; // advance to next extension
      }
      pbPtr += dwNextEntry;
   }
   fclose(a_file);

   if (iFileCount == 0) { // no files found?
      return ERR_FILE_EMPTY_ZIP;
   }

   zi->iFiles = iFileCount;
   return 0; // operation completed successfully
}

int
zip_extract(char *pchZipFile, char *pchFileName, dword dwOffset)
{
   int iStatus, iCount;
   dword dwSize;
   byte *pbInputBuffer, *pbOutputBuffer;
   FILE *pfileOut, *pfileIn;
   z_stream z;

   strcpy(pchFileName, DVE.dve_home_dir);
   strcat(pchFileName, "/unzip.tmp");
   if (!(pfileOut = fopen(pchFileName, "wb"))) {
      return ERR_FILE_UNZIP_FAILED; // couldn't create output file
   }
   pfileIn = fopen(pchZipFile, "rb"); // open ZIP file for reading
   fseek(pfileIn, dwOffset, SEEK_SET); // move file pointer to beginning of data block
   fread(pbGPBuffer, 30, 1, pfileIn); // read local header
   dwSize = loc_get_dword(pbGPBuffer + 18); // length of compressed data
   dwOffset += 30 + loc_get_word(pbGPBuffer + 26) + loc_get_word(pbGPBuffer + 28);
   fseek(pfileIn, dwOffset, SEEK_SET); // move file pointer to start of compressed data

   pbInputBuffer = pbGPBuffer; // space for compressed data chunck
   pbOutputBuffer = pbInputBuffer + 16384; // space for uncompressed data chunck
   z.zalloc = (alloc_func)0;
   z.zfree = (free_func)0;
   z.opaque = (voidpf)0;
   iStatus = inflateInit2(&z, -MAX_WBITS); // init zlib stream (no header)
   do {
      z.next_in = pbInputBuffer;
      if (dwSize > 16384) { // limit input size to max 16K or remaining bytes
         z.avail_in = 16384;
      } else {
         z.avail_in = dwSize;
      }
      z.avail_in = fread(pbInputBuffer, 1, z.avail_in, pfileIn); // load compressed data chunck from ZIP file
      while ((z.avail_in) && (iStatus == Z_OK)) { // loop until all data has been processed
         z.next_out = pbOutputBuffer;
         z.avail_out = 16384;
         iStatus = inflate(&z, Z_NO_FLUSH); // decompress data
         iCount = 16384 - z.avail_out;
         if (iCount) { // save data to file if output buffer is full
            fwrite(pbOutputBuffer, 1, iCount, pfileOut);
         }
      }
      dwSize -= 16384; // advance to next chunck
   } while ((dwSize > 0) && (iStatus == Z_OK)) ; // loop until done
   if (iStatus != Z_STREAM_END) {
      return ERR_FILE_UNZIP_FAILED; // abort on error
   }
   iStatus = inflateEnd(&z); // clean up
   fclose(pfileIn);
   fclose(pfileOut);

   return 0; // data was successfully decompressed
}


static int
loc_rom_load (char *filename)
{
  int error = dve_loadcart(filename);
  if (! error) {
    vecx_reset();
  }
  return error;
}

void
dve_reset_save_name()
{
  dve_update_save_name("");
}

void
dve_kbd_load()
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", DVE.dve_home_dir, DVE.dve_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_kbd_load_mapping(TmpFileName);
  }
}

int
dve_kbd_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", DVE.dve_home_dir, DVE.dve_save_name );
  return( psp_kbd_save_mapping(TmpFileName) );
}

int
loc_dve_save_settings(char *FileName)
{
  FILE* FileDesc;
  int   error = 0;

  FileDesc = fopen(FileName, "w");
  if (FileDesc != (FILE *)0 ) {

    fprintf(FileDesc, "dve_speed_limiter=%d\n"  , DVE.dve_speed_limiter);
    fprintf(FileDesc, "dve_auto_fire_period=%d\n", DVE.dve_auto_fire_period);
    fprintf(FileDesc, "dve_view_fps=%d\n"       , DVE.dve_view_fps);
    fprintf(FileDesc, "dve_color=%d\n"          , DVE.dve_color);
    fprintf(FileDesc, "psp_cpu_clock=%d\n"      , DVE.psp_cpu_clock);
    fprintf(FileDesc, "psp_skip_max_frame=%d\n" , DVE.psp_skip_max_frame);
    fprintf(FileDesc, "dve_render_mode=%d\n"    , DVE.dve_render_mode);
    fprintf(FileDesc, "dve_delta_x=%d\n"        , DVE.dve_delta_x);
    fprintf(FileDesc, "dve_delta_y=%d\n"        , DVE.dve_delta_y);
    fprintf(FileDesc, "dve_overlay_mode=%d\n"   , DVE.dve_overlay_mode);

    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
dve_save_settings(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", DVE.dve_home_dir, DVE.dve_save_name);
  error = loc_dve_save_settings(FileName);

  return error;
}

void
dve_update_blit_overlay()
{
  if (! DVE.overlay_loaded) return;

  if (DVE.dve_render_mode == DVE_RENDER_ROT90) {
    blit_ovl_surface = ovl_rot90_surface;
  } else {
    blit_ovl_surface = ovl_norm_surface;
  }
}

#define dve_load_image(filename) IMG_Load(filename)

int
dve_load_overlay()
{
  DVE.overlay_loaded = 0;

  char dir_name[512];

  sprintf(dir_name, "%s", user_filename);
  dirname(dir_name);

  char  FileName[MAX_PATH+1];
  snprintf(FileName, MAX_PATH, "%s/%s_rot90.png", dir_name, DVE.dve_save_name);

  SDL_Surface* my_surface_rot90 = dve_load_image( FileName );
  if ( (! my_surface_rot90)||
       (my_surface_rot90->w != PSP_SDL_SCREEN_WIDTH) ||
       (my_surface_rot90->h != PSP_SDL_SCREEN_HEIGHT)) {
    return -1;
  }

  snprintf(FileName, MAX_PATH, "%s/%s_norm.png", dir_name, DVE.dve_save_name);

  SDL_Surface* my_surface_norm = dve_load_image( FileName );
  if ((! my_surface_norm)  ||
      (my_surface_norm->w != PSP_SDL_SCREEN_WIDTH) ||
      (my_surface_norm->h != PSP_SDL_SCREEN_HEIGHT)) {
     SDL_FreeSurface( my_surface_rot90 ); return -1;
  }

  SDL_BlitSurface( my_surface_rot90, NULL, ovl_rot90_surface, NULL );
  SDL_FreeSurface( my_surface_rot90 );

  SDL_BlitSurface( my_surface_norm , NULL, ovl_norm_surface , NULL );
  SDL_FreeSurface( my_surface_norm );

  DVE.overlay_loaded = 1;
  dve_update_blit_overlay();

  return 0;
}

void
dve_change_render_mode( int new_render )
{
  if (DVE.dve_render_mode != new_render) {
    osint_video_reset();
    psp_init_keyboard();
    DVE.dve_render_mode = new_render;
    if (DVE.overlay_loaded) {
      dve_update_blit_overlay();
    }
  }
}

void
dve_change_color( int new_color )
{
  if (DVE.dve_color != new_color) {
    osint_set_color( new_color );
    osint_video_reset();
    DVE.dve_color = new_color;
  }
}

int
loc_dve_load_settings(char *FileName)
{
  char  Buffer[512];
  char *Scan;
  unsigned int Value;
  FILE* FileDesc;

  FileDesc = fopen(FileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  int render_mode = -1;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    Scan = strchr(Buffer,'=');
    if (! Scan) continue;

    *Scan = '\0';
    Value = atoi(Scan+1);

    if (!strcasecmp(Buffer,"dve_speed_limiter")) DVE.dve_speed_limiter = Value;
    else
    if (!strcasecmp(Buffer,"dve_auto_fire_period")) DVE.dve_auto_fire_period = Value;
    else
    if (!strcasecmp(Buffer,"dve_view_fps")) DVE.dve_view_fps = Value;
    else
    if (!strcasecmp(Buffer,"dve_color")) DVE.dve_color = Value;
    else
    if (!strcasecmp(Buffer,"psp_cpu_clock")) DVE.psp_cpu_clock = Value;
    else
    if (!strcasecmp(Buffer,"psp_skip_max_frame")) DVE.psp_skip_max_frame = Value;
    else
    if (!strcasecmp(Buffer,"dve_render_mode")) render_mode = Value;
    else
    if (!strcasecmp(Buffer,"dve_delta_x")) DVE.dve_delta_x = Value;
    else
    if (!strcasecmp(Buffer,"dve_delta_y")) DVE.dve_delta_y = Value;
    else
    if (!strcasecmp(Buffer,"dve_overlay_mode")) DVE.dve_overlay_mode = Value;
  }

  fclose(FileDesc);

  gp2xPowerSetClockFrequency(DVE.psp_cpu_clock);

  if (render_mode >= 0) {
    dve_change_render_mode( render_mode );
  }

  osint_set_color( DVE.dve_color );
  osint_video_reset();

  return 0;
}

void
dve_default_settings()
{
  DVE.dve_snd_enable        = 1;
  DVE.dve_speed_limiter     = 30;
  DVE.dve_view_fps          = 0;
  DVE.dve_auto_fire         = 0;
  DVE.dve_auto_fire_period  = 6;
  DVE.dve_auto_fire_pressed = 0;
  DVE.dve_delta_x           = 0;
  DVE.dve_delta_y           = 0;

  DVE.psp_cpu_clock        = GP2X_EMU_CLOCK;

  DVE.psp_skip_max_frame = 0;
  DVE.overlay_loaded = 0;
  DVE.dve_overlay_mode = 1;

  dve_change_render_mode( DVE_RENDER_NORMAL );
  dve_change_color( COLOR_GRAY );
  gp2xPowerSetClockFrequency(DVE.psp_cpu_clock);
}

int
dve_load_settings(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", DVE.dve_home_dir, DVE.dve_save_name);
  error = loc_dve_load_settings(FileName);

  return error;
}

int
dve_load_file_settings(char *FileName)
{
  return loc_dve_load_settings(FileName);
}

int
dve_rom_load(char *FileName, int zip_format)
{
  char *pchPtr;
  char *scan;
  char  SaveName[MAX_PATH+1];
  char  TmpFileName[MAX_PATH + 1];
  dword n;
  int   format;
  int   error;

  error = 1;

  if (zip_format) {

    zip_info.pchZipFile   = FileName;
    zip_info.pchExtension = ".rom.bin.vec.gam";

    if (!zip_dir(&zip_info))
    {
      pchPtr = zip_info.pchFileNames;
      for (n = zip_info.iFiles; n != 0; n--)
      {
        format = psp_fmgr_getExtId(pchPtr);
        if (format == FMGR_FORMAT_ROM) break;
        pchPtr += strlen(pchPtr) + 5; // skip offset
      }
      if (n) {
        strncpy(SaveName,pchPtr,MAX_PATH);
        scan = strrchr(SaveName,'.');
        if (scan) *scan = '\0';
        dve_update_save_name(SaveName);

        zip_info.dwOffset = loc_get_dword((byte *)(pchPtr + (strlen(pchPtr)+1)));
        if (!zip_extract(FileName, TmpFileName, zip_info.dwOffset)) {
          dve_emulator_reset();
          error = loc_rom_load(TmpFileName);
          remove(TmpFileName);
        }
      }
    }

  } else {
    strncpy(SaveName,FileName,MAX_PATH);
    scan = strrchr(SaveName,'.');
    if (scan) *scan = '\0';
    dve_update_save_name(SaveName);

    dve_emulator_reset();
    error = loc_rom_load(FileName);
  }

  if (! error ) {
    dve_kbd_load();
    dve_load_settings();
    dve_load_cheat();
    dve_load_overlay();
  }

  return error;
}


int
dve_initialize()
{
  memset(&DVE, 0, sizeof(DVE));
  getcwd(DVE.dve_home_dir, sizeof(DVE.dve_home_dir)-1); // get the location of the executable

#if defined(GCW0_MODE)
   static char *tmp_directory; // *home_name;

   sprintf(DVE.dve_home_dir, "%s/.dingux-vectrex", getenv("HOME"));
   mkdir(DVE.dve_home_dir, 0777);

   tmp_directory = malloc(MAX_PATH + 1);
   sprintf(tmp_directory, "%s/save/", DVE.dve_home_dir);
   mkdir(tmp_directory, 0777);
   free (tmp_directory);

   tmp_directory = malloc(MAX_PATH + 1);
   sprintf(tmp_directory, "%s/set/", DVE.dve_home_dir);
   mkdir(tmp_directory, 0777);
   free (tmp_directory);

   tmp_directory = malloc(MAX_PATH + 1);
   sprintf(tmp_directory, "%s/scr/", DVE.dve_home_dir);
   mkdir(tmp_directory, 0777);
   free (tmp_directory);

   tmp_directory = malloc(MAX_PATH + 1);
   sprintf(tmp_directory, "%s/roms/", DVE.dve_home_dir);
   strcpy(DVE.rom_directory, tmp_directory);
   mkdir(tmp_directory, 0777);
   free (tmp_directory);

   tmp_directory = malloc(MAX_PATH + 1);
   sprintf(tmp_directory, "%s/kbd/", DVE.dve_home_dir);
   mkdir(tmp_directory, 0777);
   free (tmp_directory);

   tmp_directory = malloc(MAX_PATH + 1);
   sprintf(tmp_directory, "%s/cht/", DVE.dve_home_dir);
   mkdir(tmp_directory, 0777);
   free (tmp_directory);
#endif;

  psp_sdl_init();

  dve_load_default();

  pbGPBuffer = (byte*)malloc( sizeof(byte) * 128*1024); // attempt to allocate the general purpose buffer

  return 0;
}

static int
loc_snapshot_save(char *FileName)
{
  return vecx_save_state( FileName );
}

static int
loc_snapshot_load(char *FileName)
{
  return vecx_load_state(FileName);
}

int
dve_snapshot_save_slot(int save_id)
{
  char        FileName[MAX_PATH+1];
  struct stat aStat;
  int         error;

  error = 1;

  if (save_id < DVE_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.snz", DVE.dve_home_dir, DVE.dve_save_name, save_id);
    error = loc_snapshot_save(FileName);
    if (! error) {
      if (! stat(FileName, &aStat))
      {
        DVE.dve_save_state[save_id].used  = 1;
        DVE.dve_save_state[save_id].thumb = 0;
        DVE.dve_save_state[save_id].date  = aStat.st_mtime;
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", DVE.dve_home_dir, DVE.dve_save_name, save_id);
        if (psp_sdl_save_thumb_png(DVE.dve_save_state[save_id].surface, FileName)) {
          DVE.dve_save_state[save_id].thumb = 1;
        }
      }
    }
  }

  return error;
}

int
dve_snapshot_del_slot(int save_id)
{
  char        FileName[MAX_PATH+1];
  struct stat aStat;
  int         error;

  error = 1;

  if (save_id < DVE_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.snz", DVE.dve_home_dir, DVE.dve_save_name, save_id);
    error = remove(FileName);
    if (! error) {
      DVE.dve_save_state[save_id].used  = 0;
      DVE.dve_save_state[save_id].thumb = 0;
      memset(&DVE.dve_save_state[save_id].date, 0, sizeof(time_t));

      /* We keep always thumbnail with id 0, to have something to display in the file requester */
      if (save_id != 0) {
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", DVE.dve_home_dir, DVE.dve_save_name, save_id);
        if (! stat(FileName, &aStat))
        {
          remove(FileName);
        }
      }
    }
  }

  return error;
}

static int
loc_dve_save_cheat(char *chFileName)
{
  FILE* FileDesc;
  int   cheat_num;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    for (cheat_num = 0; cheat_num < DVE_MAX_CHEAT; cheat_num++) {
      DVE_cheat_t* a_cheat = &DVE.dve_cheat[cheat_num];
      if (a_cheat->type != DVE_CHEAT_NONE) {
        fprintf(FileDesc, "%d,%x,%x,%s\n",
                a_cheat->type, a_cheat->addr, a_cheat->value, a_cheat->comment);
      }
    }
    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
dve_save_cheat(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", DVE.dve_home_dir, DVE.dve_save_name);
  error = loc_dve_save_cheat(FileName);

  return error;
}

static int
loc_dve_load_cheat(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  char *Field;
  unsigned int  cheat_addr;
  unsigned int  cheat_value;
  unsigned int  cheat_type;
  char         *cheat_comment;
  int           cheat_num;
  FILE* FileDesc;

  memset(DVE.dve_cheat, 0, sizeof(DVE.dve_cheat));
  cheat_num = 0;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    /* %d, %x, %x, %s */
    Field = Buffer;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%d", &cheat_type) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_addr) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_value) != 1) continue;
    Field = Scan + 1;
    cheat_comment = Field;

    if (cheat_type <= DVE_CHEAT_NONE) continue;

    DVE_cheat_t* a_cheat = &DVE.dve_cheat[cheat_num];

    a_cheat->type  = cheat_type;
    a_cheat->addr  = cheat_addr & (DVE_CHEAT_RAM_SIZE-1);
    a_cheat->value = cheat_value;
    strncpy(a_cheat->comment, cheat_comment, sizeof(a_cheat->comment));
    a_cheat->comment[sizeof(a_cheat->comment)-1] = 0;

    if (++cheat_num >= DVE_MAX_CHEAT) break;
  }
  fclose(FileDesc);

  return 0;
}

int
dve_load_cheat()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", DVE.dve_home_dir, DVE.dve_save_name);
  error = loc_dve_load_cheat(FileName);

  return error;
}

int
dve_load_file_cheat(char *FileName)
{
  return loc_dve_load_cheat(FileName);
}

extern unsigned char ram[1024];
void
dve_apply_cheats()
{
  int cheat_num;
  for (cheat_num = 0; cheat_num < DVE_MAX_CHEAT; cheat_num++) {
    DVE_cheat_t* a_cheat = &DVE.dve_cheat[cheat_num];
    if (a_cheat->type == DVE_CHEAT_ENABLE) {
      ram[a_cheat->addr] = a_cheat->value;
    }
  }
}


int
dve_snapshot_load_slot(int load_id)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  if (load_id < DVE_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.snz", DVE.dve_home_dir, DVE.dve_save_name, load_id);
    error = loc_snapshot_load(FileName);
  }

  return error;
}

int
dve_load_default()
{
  dve_update_save_name("");

  dve_default_settings();
  psp_kbd_default_settings();

  dve_load_settings();
  dve_kbd_load();
  dve_load_cheat();
  dve_load_overlay();

  return 0;
}


void
dve_audio_pause(void)
{
# ifdef SOUND_SUPPORT
  SDL_PauseAudio(1);
# endif
}

void
dve_audio_resume(void)
{
# ifdef SOUND_SUPPORT
  if (DVE.dve_snd_enable) {
    SDL_PauseAudio(0);
  }
# endif
}

void
dve_synchronize(void)
{
  static u32 nextclock = 1;
  static u32 next_sec_clock = 0;
  static u32 cur_num_frame = 0;

  u32 curclock = SDL_GetTicks();

  if (DVE.dve_speed_limiter) {
    while (curclock < nextclock) {
     curclock = SDL_GetTicks();
    }
    u32 f_period = 1000 / DVE.dve_speed_limiter;
    nextclock += f_period;
    if (nextclock < curclock) nextclock = curclock + f_period;
  }

  if (DVE.dve_view_fps) {
    cur_num_frame++;
    if (curclock > next_sec_clock) {
      next_sec_clock = curclock + 1000;
      DVE.dve_current_fps = cur_num_frame;
      cur_num_frame = 0;
    }
  }
}

static void
PutImage_gu_normal()
{
  // 240x300 -> 240x300
  unsigned int* src_ptr   = (unsigned int *)blit_surface->pixels;
  unsigned int* tgt_ptr   = (unsigned int *)back_surface->pixels;
  // center source
  src_ptr += (30 * BLIT_WIDTH) / 2;
  // center target
  tgt_ptr +=  ( (PSP_LINE_SIZE - 240) / 2 ) / 2;
  // apply delta
  src_ptr +=   (DVE.dve_delta_y * BLIT_WIDTH) / 2;
  src_ptr +=   (DVE.dve_delta_x) / 2;

  int h = 240;
  while (h-- > 0) {
    int w = 30;
    while (w-- > 0) {
      *tgt_ptr++ = *src_ptr++;
      *tgt_ptr++ = *src_ptr++;
      *tgt_ptr++ = *src_ptr++;
      *tgt_ptr++ = *src_ptr++;
    }
    tgt_ptr += (PSP_LINE_SIZE - 240) / 2;
    src_ptr += (BLIT_WIDTH    - 240) / 2;
  }
}

static void
PutImage_gu_normal_overlay()
{
  // 240x300 -> 240x300
  unsigned int* src_ptr   = (unsigned int *)blit_surface->pixels;
  unsigned int* tgt_ptr   = (unsigned int *)back_surface->pixels;
  unsigned int* ovl_ptr   = (unsigned int *)blit_ovl_surface->pixels;
  // center source
  src_ptr += (30 * BLIT_WIDTH) / 2;
  // center target
  tgt_ptr +=  ( (PSP_LINE_SIZE - 240) / 2 ) / 2;
  ovl_ptr +=  ( (PSP_LINE_SIZE - 240) / 2 ) / 2;
  // apply delta
  src_ptr +=   (DVE.dve_delta_y * BLIT_WIDTH) / 2;
  src_ptr +=   (DVE.dve_delta_x) / 2;

  int h = 240;
  while (h-- > 0) {
    int w = 30;
    while (w-- > 0) {
      unsigned int o = *ovl_ptr++;
      unsigned int c = *src_ptr++;
      unsigned int c0 = 0x10000 - ((c >> 16   ) != 0);
      c  = (0x10000 - ((c & 0xffff) != 0)) | (c0 << 16);
      *tgt_ptr++ = (((o ^ c) & 0xf7def7deU) >> 1) + (o & c);
      o = *ovl_ptr++;
      c = *src_ptr++;
      c0 = 0x10000 - ((c >> 16   ) != 0);
      c  = (0x10000 - ((c & 0xffff) != 0)) | (c0 << 16);
      *tgt_ptr++ = (((o ^ c) & 0xf7def7deU) >> 1) + (o & c);
      o = *ovl_ptr++;
      c = *src_ptr++;
      c0 = 0x10000 - ((c >> 16   ) != 0);
      c  = (0x10000 - ((c & 0xffff) != 0)) | (c0 << 16);
      *tgt_ptr++ = (((o ^ c) & 0xf7def7deU) >> 1) + (o & c);
      o = *ovl_ptr++;
      c = *src_ptr++;
      c0 = 0x10000 - ((c >> 16   ) != 0);
      c  = (0x10000 - ((c & 0xffff) != 0)) | (c0 << 16);
      *tgt_ptr++ = (((o ^ c) & 0xf7def7deU) >> 1) + (o & c);
    }
    tgt_ptr += (PSP_LINE_SIZE - 240) / 2;
    ovl_ptr += (PSP_LINE_SIZE - 240) / 2;
    src_ptr += (BLIT_WIDTH    - 240) / 2;
  }
}

static void
PutImage_gu_rot90()
{
  // 600x480 ->  600x480
  unsigned int* src_ptr   = (unsigned int *)blit_surface->pixels;
  unsigned int* tgt_ptr   = (unsigned int *)back_surface->pixels;

  // apply delta
  src_ptr += (DVE.dve_delta_x * BLIT_WIDTH) / 2;
  src_ptr += (DVE.dve_delta_y) / 2;

  int h = 240;
  tgt_ptr += (PSP_LINE_SIZE - 300) / 4;

  while (h-- > 0) {
    int w = 37;
    while (w-- > 0) {
      *tgt_ptr++ = *src_ptr++;
      *tgt_ptr++ = *src_ptr++;
      *tgt_ptr++ = *src_ptr++;
      *tgt_ptr++ = *src_ptr++;
    }
    *tgt_ptr++ = *src_ptr++;
    *tgt_ptr++ = *src_ptr++;
    tgt_ptr += (PSP_LINE_SIZE - 300) / 2;
    src_ptr += (BLIT_WIDTH    - 300) / 2;
  }
}

static void
PutImage_gu_rot90_overlay()
{
  // 600x480 ->  600x480
  unsigned int* src_ptr   = (unsigned int *)blit_surface->pixels;
  unsigned int* tgt_ptr   = (unsigned int *)back_surface->pixels;
  unsigned int* ovl_ptr   = (unsigned int *)blit_ovl_surface->pixels;
  int h = 240;

  // apply delta
  src_ptr += (DVE.dve_delta_x * BLIT_WIDTH) / 2;
  src_ptr += (DVE.dve_delta_y) / 2;

  tgt_ptr += (PSP_LINE_SIZE - 300) / 4;
  ovl_ptr += (PSP_LINE_SIZE - 300) / 4;
  while (h-- > 0) {
    int w = 75;
    while (w-- > 0) {
      unsigned int o = *ovl_ptr++;
      unsigned int c = *src_ptr++;
      unsigned int c0 = 0x10000 - ((c >> 16   ) != 0);
      c  = (0x10000 - ((c & 0xffff) != 0)) | (c0 << 16);
      *tgt_ptr++ = (((o ^ c) & 0xf7def7deU) >> 1) + (o & c);
      o = *ovl_ptr++;
      c = *src_ptr++;
      c0 = 0x10000 - ((c >> 16   ) != 0);
      c  = (0x10000 - ((c & 0xffff) != 0)) | (c0 << 16);
      *tgt_ptr++ = (((o ^ c) & 0xf7def7deU) >> 1) + (o & c);
    }

    tgt_ptr += (PSP_LINE_SIZE - 300) / 2;
    ovl_ptr += (PSP_LINE_SIZE - 300) / 2;
    src_ptr += (BLIT_WIDTH    - 300) / 2;
  }
}


void
dve_render()
{
  osint_render();

  if (DVE.psp_skip_cur_frame <= 0) {

    DVE.psp_skip_cur_frame = DVE.psp_skip_max_frame;

    if (DVE.dve_overlay_mode && DVE.overlay_loaded) {
      if (DVE.dve_render_mode == DVE_RENDER_ROT90) PutImage_gu_rot90_overlay();
      else                                         PutImage_gu_normal_overlay();
    } else {
      if (DVE.dve_render_mode == DVE_RENDER_ROT90) PutImage_gu_rot90();
      else                                         PutImage_gu_normal();
    }

    if (DVE.dve_view_fps) {
      char buffer[32];
      sprintf(buffer, "%3d", (int)DVE.dve_current_fps);
      psp_sdl_fill_print(6, 6, buffer, 0xffffff, 0 );
    }

    psp_sdl_flip();

    if (psp_screenshot_mode) {
      psp_screenshot_mode--;
      if (psp_screenshot_mode <= 0) {
        psp_sdl_save_screenshot();
        psp_screenshot_mode = 0;
      }
    }

  } else if (DVE.psp_skip_max_frame) {
    DVE.psp_skip_cur_frame--;
  }

  dve_synchronize();
}

void
dve_treat_command_key(int dve_idx)
{
  int new_render;

  switch (dve_idx)
  {
    case DVE_C_FPS: DVE.dve_view_fps = ! DVE.dve_view_fps;
    break;
    case DVE_C_RENDER:
      psp_sdl_black_screen();
      new_render = DVE.dve_render_mode + 1;
      if (new_render > DVE_LAST_RENDER) new_render = 0;
      dve_change_render_mode( new_render );
    break;
    case DVE_C_LOAD: psp_main_menu_load_current();
    break;
    case DVE_C_SAVE: psp_main_menu_save_current();
    break;
    case DVE_C_RESET:
       psp_sdl_black_screen();
       dve_emulator_reset();
       dve_reset_save_name();
    break;
    case DVE_C_AUTOFIRE:
       kbd_change_auto_fire(! DVE.dve_auto_fire);
    break;
    case DVE_C_DECFIRE:
      if (DVE.dve_auto_fire_period > 0) DVE.dve_auto_fire_period--;
    break;
    case DVE_C_INCFIRE:
      if (DVE.dve_auto_fire_period < 19) DVE.dve_auto_fire_period++;
    break;
    case DVE_C_SCREEN: psp_screenshot_mode = 10;
    break;
  }
}
