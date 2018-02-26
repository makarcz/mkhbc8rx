echo off
rem
rem File: make_romlib.bat
rem
rem Purpose:
rem
rem    Build mkhbcrom.lib library.
rem
rem Author: Marek Karcz (C) 2012-2018
rem
rem Revision history:
rem
rem Feb 2018
rem    Created
rem
rem

echo Building library "mkhbcrom.lib" ...
rem
rem build library mkhbcrom.lib
rem
echo      Delete objects ...
del mkhbcos_fmware.o mkhbcos_serialio.o mkhbcos_ds1685.o
echo      Assemble/compile source code ...
cc65 -t none --cpu 6502 mkhbcos_ansi.c
ca65 mkhbcos_ansi.s
del mkhbcos_ansi.s
ca65 mkhbcos_fmware.s -l
ca65 mkhbcos_serialio.s
ca65 mkhbcos_ds1685.s
echo      Update library ...
ar65 a mkhbcrom.lib mkhbcos_fmware.o mkhbcos_serialio.o mkhbcos_ansi.o mkhbcos_ds1685.o


echo Building image "romlib.BIN" ...
rem
rem build romlib.BIN, an EPROM image (binary format, 8 kB, e.g.: 27C64).
rem
cl65 -t none --cpu 6502 --config romlib.cfg -o romlib.BIN -l -m romlib.map romlib.c mkhbcrom.lib
rem

echo Done.
echo ----------------------------------------------------------------
echo Image:                romlib.BIN
echo Map file:             romlib.map
echo Listing:              romlib.lst
echo Startup code listing: mkhbcos_fmware.lst

pause prompt
