
    Welcome to Portable Vectrex Emulator

Original Author of VecX 

  Valavan Manohararajah

Author of the PSP & Caanoo port version 

  Ludovic Jacomme alias Zx-81 zx81.zx81(at)gmail.com

  Homepage: http://zx81.zx81.free.fr

Port on Dingux by Coccijoe
http://coccijoe.blogspot.com/


1. INTRODUCTION
   ------------

  VecX emulates the Vectrex game console on systems such as Linux and Windows.

  Dingux-Vectrex is a port on Caanoo of one latest version of VecX.

  This package is under GPL Copyright, read COPYING file for
  more information about it.


2. INSTALLATION
   ------------

  Unzip the zip file, and copy the content of the directory game to your
  SD card.

  Put your roms files on "roms" sub-directory. 

  For any comments or questions on this version
  http://coccijoe.blogspot.com/


3. CONTROL
   ------------

  In the VECTREX emulator window, there are three
different mapping (standard, left trigger, and
right Trigger mappings).  You can toggle between
while playing inside the emulator using the two 
Dingoo trigger keys.

    -------------------------------------
    Dingoo        VECTREX            (standard)
  
    A          1
    Y          2
    B          3
    X          4

    Pad        Joystick

    -------------------------------------
    Dingoo        VECTREX   (left trigger)
  
    A          FPS  
    Y          LOAD Snapshot
    B          Swap digital / Analog
    X          SAVE Snapshot
    Left       Render mode
    Right      Render mode

    -------------------------------------
    Dingoo        VECTREX   (right trigger)
  
    A          1
    Y          2
    B          3
    X          Auto-fire
    Left       Dec fire
    Right      Inc fire
  
Press Start+L+R to exit and return to eloader.
Press Select    to enter in emulator main menu.
Press Start     open/close the On-Screen keyboard

  In the main menu

    RTrigger   Reset the emulator

    X   Go Up directory
    A      Valid
    B     Valid
    Y     Go Back to the emulator window

The On-Screen Keyboard of "Danzel" and "Jeff Chen"

Use d-pad to choose one of the 9 squares,
and use Triangle, Square, Cross and Circle to
choose one of the 4 letters of the highlighted
square.

Use LTrigger and RTrigger to see other 9 squares
figures.

4. LOADING ROM FILES (.rom or .bin)
   ------------

  If you want to load rom image in your emulator, you have to put 
  your rom file (with .rom or .bin file extension) on your SD
  memory card in the 'roms' directory. 

  Then, while inside emulator, just press SELECT to enter in the emulator 
  main menu, choose "Load Rom", and then using the file selector choose one 
  rom image  file to load in your emulator.


5. LOADING KEY MAPPING FILES
   ------------

  For given games, the default keyboard mapping between Dingoo Keys and VECTREX keys,
  is not suitable, and the game can't be played on Dingux-VECTREX.

  To overcome the issue, you can write your own mapping file. Using notepad for
  example you can edit a file with the .kbd extension and put it in the kbd 
  directory.

  For the exact syntax of those mapping files, have a look on sample files already
  presents in the kbd directory (default.kbd etc ...).

  After writting such keyboard mapping file, you can load them using 
  the main menu inside the emulator.

  If the keyboard filename is the same as the rom file then when you load 
  this file, the corresponding keyboard file is automatically loaded !

  You can now use the Keyboard menu and edit, load and save your
  keyboard mapping files inside the emulator. The Save option save the .kbd
  file in the kbd directory using the "Game Name" as filename. The game name
  is displayed on the right corner in the emulator menu.

  If you have saved the state of a game, then a thumbnail image will be
  displayed in the file requester while selecting any file (roms, keyboard,
  settings) with game name, to help you to recognize that game later.
  
  You can use the virtual keyboard in the file requester menu to choose the
  first letter of the game you search (it might be useful when you have tons of
  games in the same folder)

6. OVERLAYS
   ------------

  Overlays are now supported. You need to create two overlay images called 
  <rom_name>_rot90.png and <rom_name>_norm.png in the 'over' folder.
  You may have a look to the existing ones (Mine-Storm & Armor attack) 
  to create your own. The geometry of those images must be 320x240.

  The overlays will be automatically displayed when you load the <rom_name>.
  
7. COMPILATION
   ------------

  It has been developped under Linux using gcc with GPH-SDK. 
  To rebuild the homebrew run the Makefile in the src archive.

