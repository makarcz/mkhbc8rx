echo off
rem
rem File: make_clean.bat
rem
rem Purpose:
rem
rem    Remove all files that are product of compilation.
rem
rem Author: Marek Karcz (C) 2018
rem 
rem Revision history:
rem 
rem Feb 1 2018
rem    Created
rem
del *.o
del *.lst
del *.map
del *_prg.txt
del mkhbcos_mobo
del mkhbcos_sbc