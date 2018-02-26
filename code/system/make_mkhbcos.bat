rem obsolete
rem cl65 --verbose --config mkhbc_mobo.cfg --target none --listing mkhbcos_mobo.asm
make_romlib
cl65 --verbose --config mkhbc_sbc.cfg --target none --listing mkhbcos_sbc.asm
pause prompt
