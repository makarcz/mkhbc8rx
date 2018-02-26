echo off
rem
rem File: make_enhmon.bat
rem
rem Purpose:
rem
rem    Build enhmon application.
rem
rem Author: Marek Karcz 2018
rem
rem Revision history:
rem
rem 2/16/2018
rem    Created
rem
rem

echo Building application "enhmon" ...
rem
rem build enhmon app.
rem
cl65 -t none --cpu 6502 -I ..\system --config mkhbcoslib.cfg -l -m enhmon.map enhmon.c mkhbcos.lib
rem

echo Generating terminal program loading script "enhmon_prg.txt" ...
rem
rem command below will build hex load file with
rem suppressed all zeroes lines and will put
rem execute statement at the end of file
rem NOTE: Use feature 'remove all zeroes' (flag -z)
rem       with care. Situation may occur than line
rem       with zeroes only is removed and that the
rem       first null/zero code in that line was
rem       the actual null terminating character
rem       of some string or buffer. If your application
rem       exhibits strange behavior/hangs up or crashes
rem       then remove -z flag from command below and
rem       analyze file enhshell_prg.txt. If there is any
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
..\bin2hex -f enhmon -o enhmon_prg.txt -w 2816 -x 2816 -z

rem
rem command below will build hex load file with
rem all load statements (including lines with all zeroes)
rem and will suppress the execute statement.
rem
rem bin2hex -f enhmon -o enhmon_prg.txt -w 2816 -x 2816 -s

echo Done.

pause prompt
