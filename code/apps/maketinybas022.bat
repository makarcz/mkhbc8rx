cl65 --verbose --config tinybasic.cfg --target none --mapfile tinybas022.map --listing tinybas022.asm
rem bin2hex -f tinybas022 -o tinybas022_prg.txt -w 1024 -x 3312 -s -z
..\bin2hex -f tinybas022 -o tinybas022_prg.txt -w 17408 -x 19696 -s -z
pause prompt
