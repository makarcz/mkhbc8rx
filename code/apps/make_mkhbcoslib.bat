echo off
rem
rem File: make_mkhbcoslib.bat
rem
rem Purpose:
rem
rem    Build mkhbcos.lib library and demo app.: hello
rem    Library mkhbcos.lib is a system library for
rem    applications running in MKHBCOS environment
rem    linked with mkhbcos.lib (created originally
rem    from supervision.lib and modified crt0.s).
rem
rem Author: Marek Karcz (C) 2012-2018
rem
rem Revision history:
rem
rem Jan 2012
rem    Created
rem
rem 1/19/2012
rem
rem    Assembling mkhbcos_init.s (created from crt0.s), mkhbcos_serialio.s,
rem    mkhbcos_lcd.s, mkhbcos_ansi.s and mkhbcos_ds1685.s added.
rem    Objects mkhbcos_serialio.o, mkhbcos_lcd.o, mkhbcos_ds1685.o,
rem    mkhbcos_ansi.s are added to mkhbcos.lib.
rem    Removed objects: puts.o, fgetc.o, gets.o,
rem    putchar.o, getchar.o from mkhbcos.lib
rem    (These objects were std implementations from
rem     supervision.lib causing duplicate symbols and
rem     missing object references. Now I supply
rem     my own implementation of these routines for MKHBCOS).
rem
rem 1/20/2018
rem
rem    Added comments and (C) notice.
rem
rem 1/28/2018
rem
rem     Renamed from makehello.bat to make_mkhbcoslib.bat.
rem
rem 2/18/2018
rem
rem     Added deleting generated assembly files after compilation.
rem

echo Building library "mkhbcos.lib" ...
rem
rem build library mkhbcos.lib
rem (runtime and firmware)
rem
echo      Delete objects ...
rem del crt0.o mkhbcos_serialio.o mkhbcos_lcd.o mkhbcos_ds1685.o
del mkhbcos_init.o mkhbcos_serialio.o mkhbcos_lcd.o mkhbcos_ds1685.o
echo      Assemble/compile source code ...
cc65 -t none --cpu 6502 -I ..\system ..\system\mkhbcos_lcd1602.c -o mkhbcos_lcd1602.s
cc65 -t none --cpu 6502 -I ..\system ..\system\mkhbcos_ansi.c -o mkhbcos_ansi.s
ca65 -I ..\system mkhbcos_lcd1602.s -o mkhbcos_lcd1602.o
del mkhbcos_lcd1602.s
ca65 -I ..\system mkhbcos_ansi.s
del mkhbcos_ansi.s
rem ca65 crt0.s
ca65 mkhbcos_init.s -o mkhbcos_init.o
ca65 -I ..\system ..\system\mkhbcos_serialio.s -o mkhbcos_serialio.o
ca65 -I ..\system ..\system\mkhbcos_lcd.s -o mkhbcos_lcd.o
ca65 -I ..\system ..\system\mkhbcos_ds1685.s -l -o mkhbcos_ds1685.o
move /Y ..\system\mkhbcos_ds1685.lst .
echo      Update library ...
rem ar65 a mkhbcos.lib crt0.o mkhbcos_serialio.o mkhbcos_lcd.o mkhbcos_lcd1602.o mkhbcos_ansi.o mkhbcos_ds1685.o
ar65 a mkhbcos.lib mkhbcos_init.o mkhbcos_serialio.o mkhbcos_lcd.o mkhbcos_lcd1602.o mkhbcos_ansi.o mkhbcos_ds1685.o


echo Building application "hello" ...
rem
rem build hello app.
rem
cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -m hello.map hello.c mkhbcos.lib
rem

rem OBSOLETE section, to be removed
rem cl65 -t none --cpu 6502 --config mkhbcoslib.cfg -l -m hello.map hello.c mkhbcos_serialio.s mkhbcos_lcd.s mkhbcos.lib

echo Generating terminal program loading script "hello_prg.txt" ...
rem
rem command below will build hex load file with
rem suppressed all zeroes lines and will put
rem execute statement at the end of file
rem NOTE: Use feature 'remove all zeroes' (flag -z)
rem       with care. Situation may occur when line
rem       with zeroes only is removed and that the
rem       first null/zero code in that line was
rem       the actual null terminating character
rem       of some string or buffer. If your application
rem       exhibits strange behavior/hungs up or crashes
rem       then remove -z flag from command below and
rem       analyse file hello_prg.txt. If there is any
rem       write memory statement preceding the all
rem       zeroes line that does not end with at
rem       least one null/zero byte, this may be the case.
rem       In such case, you need to edit hello_prg.txt
rem       manually and do not remove the "all zeroes" line
rem       that follows the line that does not end with
rem       null/zero byte unless you know it is not
rem       a buffer that needs terminating with null.
rem       It is also possible that zero value bytes
rem       are initial values of some variable and they
rem       should remain in place. in such case do not
rem       use -z flag. It is safer in general not to use
rem       it, even though the program load time is much
rem       longer with all unnecessary null bytes.
rem
..\bin2hex -f hello -o hello_prg.txt -w 2816 -x 2816 -z

rem
rem command below will buid hex load file with
rem all load statements (including lines with all zeroes)
rem and will suppress the execute statement.
rem
rem bin2hex -f hello -o hello_prg.txt -w 2816 -x 2816 -s

echo Done.

pause prompt
