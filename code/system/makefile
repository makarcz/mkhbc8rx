#
# File: makefile
# Author: Marek Karcz
# Purpose: Definitions for make utility how to build MKHBC OS firmware
#          and MKHBCROM library for MKHBC-8-Rx hobby computer.
# Date created: 3/1/2018
# NOTE: For now this works in DOS console on Windows only.
#       Make sure a make utility is on the PATH.
#       E.g.:
#          set PATH=C:\mingw-w64\x86_64-5.3.0\mingw64\bin;%PATH%
#          mingw32-make all
#

clean:
	del *.o
	del *.lst
	del *.map
	del *_prg.txt
	del mkhbcos_sbc romlib.BIN

romlib: mkhbcos_fmware.o mkhbcos_serialio.o mkhbcos_ansi.o mkhbcos_ds1685.o romlib.c romlib.cfg
	@echo      Update library ...
	ar65 a mkhbcrom.lib mkhbcos_fmware.o mkhbcos_serialio.o mkhbcos_ansi.o mkhbcos_ds1685.o
	@echo      Building image "romlib.BIN" ...
	cl65 -t none --cpu 6502 --config romlib.cfg -o romlib.BIN -l -m romlib.map romlib.c mkhbcrom.lib
	@echo      Done.
	@echo ----------------------------------------------------------------
	@echo Image:                romlib.BIN
	@echo Map file:             romlib.map
	@echo Listing:              romlib.lst
	@echo Startup code listing: mkhbcos_fmware.lst

mkhbcos_fmware.o: mkhbcos_fmware.s
	ca65 mkhbcos_fmware.s -l

mkhbcos_serialio.o: mkhbcos_serialio.s
	ca65 mkhbcos_serialio.s

mkhbcos_ansi.o: mkhbcos_ansi.c
	cc65 -t none --cpu 6502 mkhbcos_ansi.c
	ca65 mkhbcos_ansi.s
	del mkhbcos_ansi.s

mkhbcos_ds1685.o: mkhbcos_ds1685.s
	ca65 mkhbcos_ds1685.s

all: clean romlib
