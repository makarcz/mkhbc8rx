echo off
rem
rem File: make_date.bat
rem
rem Purpose:
rem
rem    Build program that shows date / time.
rem
rem Author: Marek Karcz 2018
rem
rem Revision history:
rem
rem 2/12/2018
rem    Created
rem
rem

echo Building application "date" ...
rem
rem build date app.
rem
cl65 -t none --cpu 6502 --config mkhbcoslib.cfg -l -m date.map date.c mkhbcos.lib
rem

echo Generating terminal program loading script "date_prg.txt" ...
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
rem       exhibits strange behavior/hungs up or crashes
rem       then remove -z flag from command below and
rem       analyse file enhshell_prg.txt. If there is any
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
..\bin2hex -f date -o date_prg.txt -w 1024 -x 1024 -z

rem
rem command below will buid hex load file with
rem all load statements (including lines with all zeroes)
rem and will suppress the execute statement.
rem
rem bin2hex -f date -o date_prg.txt -w 1024 -x 1024 -s

echo Done.

pause prompt
