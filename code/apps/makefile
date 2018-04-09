#
# File: makefile
# Author: Marek Karcz
# Purpose: Definitions for make utility how to build MKHBCOS library
#          and applications for MKHBC-8-Rx hobby computer.
# Date created: 2/28/2018
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
	del hello microchess test1 testansi tinybasic tinybas022 d2hb enhmon clock
	del texted
	del eh_basic
	del floader

lib: mkhbcos_init.o mkhbcos_serialio.o mkhbcos_lcd.o mkhbcos_lcd1602.o mkhbcos_ansi.o mkhbcos_ds1685.o
	@echo      Update library ...
	ar65 a mkhbcos.lib mkhbcos_init.o mkhbcos_serialio.o mkhbcos_lcd.o mkhbcos_lcd1602.o mkhbcos_ansi.o mkhbcos_ds1685.o

mkhbcos_lcd1602.o: ..\system\mkhbcos_lcd1602.c ..\system\mkhbcos_ml.h romlib.h
	cc65 -t none --cpu 6502 -I ..\system ..\system\mkhbcos_lcd1602.c -o mkhbcos_lcd1602.s
	ca65 -I ..\system mkhbcos_lcd1602.s -o mkhbcos_lcd1602.o
	del mkhbcos_lcd1602.s

mkhbcos_ansi.o: ..\system\mkhbcos_ansi.c ..\system\mkhbcos_ml.h romlib.h
	cc65 -t none --cpu 6502 -I ..\system ..\system\mkhbcos_ansi.c -o mkhbcos_ansi.s
	ca65 -I ..\system mkhbcos_ansi.s
	del mkhbcos_ansi.s

mkhbcos_init.o: mkhbcos_init.s
	ca65 mkhbcos_init.s -o mkhbcos_init.o

mkhbcos_serialio.o: ..\system\mkhbcos_serialio.s ..\system\mkhbcos_ml.inc
	ca65 -I ..\system ..\system\mkhbcos_serialio.s -o mkhbcos_serialio.o

mkhbcos_lcd.o: ..\system\mkhbcos_lcd.s ..\system\mkhbcos_ml.inc
	ca65 -I ..\system ..\system\mkhbcos_lcd.s -o mkhbcos_lcd.o

mkhbcos_ds1685.o: ..\system\mkhbcos_ds1685.s ..\system\mkhbcos_ml.inc
	ca65 -I ..\system ..\system\mkhbcos_ds1685.s -l -o mkhbcos_ds1685.o
	move /Y ..\system\mkhbcos_ds1685.lst .

hello: hello.c ..\system\mkhbcos_ml.h romlib.h mkhbcoslib.cfg mkhbcos.lib
	cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -m hello.map hello.c mkhbcos.lib
	..\bin2hex -f hello -o hello_prg.txt -w 2816 -x 2816 -z

enhmon: enhmon.c ..\system\mkhbcos_ml.h romlib.h mkhbcoslib.cfg mkhbcos.lib
	cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -m enhmon.map enhmon.c mkhbcos.lib
	..\bin2hex -f enhmon -o enhmon_prg.txt -w 2816 -x 2816 -z

clock: clock.c ..\system\mkhbcos_ml.h romlib.h mkhbcoslib.cfg mkhbcos.lib
	cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -m clock.map clock.c mkhbcos.lib
	..\bin2hex -f clock -o clock_prg.txt -w 2816 -x 2816 -z

conv: d2hexbin.c ..\system\mkhbcos_ml.h romlib.h mkhbcoslib.cfg mkhbcos.lib
	cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -o d2hb -m d2hb.map d2hexbin.c mkhbcos.lib
	..\bin2hex -f d2hb -o d2hb_prg.txt -w 2816 -x 2816 -z

texted: texted.c ..\system\mkhbcos_ml.h romlib.h mkhbcoslib.cfg mkhbcos.lib
	cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -o texted -m texted.map texted.c mkhbcos.lib
	..\bin2hex -f texted -o texted_prg.txt -w 2816 -x 2816 -z

tinybas022: tinybas022.asm tinybasic.cfg
	cl65 --verbose --cpu 6502 --asm-include-dir ..\system --config tinybasic.cfg --target none --mapfile tinybas022.map --listing tinybas022.asm
	..\bin2hex -f tinybas022 -o tinybas022_prg.txt -w 2816 -x 5104 -z

tinybasic: tinybasic.asm tinybasic.cfg
	cl65 --verbose --asm-include-dir ..\system --config tinybasic.cfg --target none --mapfile tinybasic.map --listing tinybasic.asm
	..\bin2hex -f tinybasic -o tinybasic_prg.txt -w 2816 -x 5104 -z

ehbasic: eh_basic.asm ehbasic.cfg
	cl65 --verbose --asm-include-dir ..\system --config ehbasic.cfg --target none --mapfile ehbasic.map --listing eh_basic.asm
	..\bin2hex -f eh_basic -o ehbasic_prg.txt -w 2816 -x 2816 -z

chess: microchess.asm microchess.cfg
	cl65 --verbose --asm-include-dir ..\system --config microchess.cfg --target none --mapfile microchess.map --listing microchess.asm
	..\bin2hex -f microchess -o microchess_prg.txt -w 2816 -x 2816

floader: floader.c himem.cfg
	cl65 -t none --cpu 6502 -I ..\system --config himem.cfg -l -o floader -m floader.map floader.c mkhbcos.lib
	..\bin2hex -f floader -o floader_prg.txt -w 22272 -x 22272 -z

all: clean lib hello enhmon clock conv texted tinybasic tinybas022 chess floader
