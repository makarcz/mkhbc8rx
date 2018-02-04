cl65 --verbose --config tinybasic.cfg --target none --mapfile tinybasic.map --listing tinybasic.asm
rem bin2hex -f tinybasic -o tinybasic_prg.txt -w 1024 -x 3312 -s -z
..\bin2hex -f tinybasic -o tinybasic_prg.txt -w 17408 -x 19696 -s -z
pause prompt