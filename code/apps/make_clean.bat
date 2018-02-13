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
rem Feb 11 2018
rem    Added d2hb program to delete list.
rem
rem Feb 12 2018
rem    Added date and setdt programs to delete list.
rem
del *.o
del *.lst
del *.map
del *_prg.txt
del enhshell hello microchess test1 testansi tinybasic d2hb date setdt
