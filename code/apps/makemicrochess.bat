cl65 --verbose --config microchess.cfg --target none --mapfile microchess.map --listing microchess.asm
..\bin2hex -f microchess -o microchess_prg.txt -w 1024 -x 1024
pause prompt