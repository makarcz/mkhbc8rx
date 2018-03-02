Hello retro-computing adventurer!

Before you start your adventures with BASIC programming, especially with
Tiny Basic interpreter, you need to know a few facts:

1) Tiny Basic is really small.

TB is small, which is good for systems with little memory, but there are
penalties to pay:

 - performance

    Yep, TB is slow. The actual BASIC interpreter code is in fact written
    in another Virtual Machine language interpreter. It is an interpreter
    running on top of another interpreter. This helps with keeping the
    code small, but you pay the price in performance.

 - limited features

    Tiny Basic is not a language for numeric calculations and complex data
    processing. There is no floating point support, no arrays and no strings.
    There is a fixed amount of variables which are named A..Z at fixed memory
    locations. There are no structured loops, just good old GOTO. Not even
    built in commands for PEEK and POKE, but there is alternative in the form
    of ML routine which you can call with USR function.
    There are no DATA statements, so if you want to enter a lot of data,
    execute loop with input command and send the data via standard input
    (serial port, paper tape, etc.) or hardcode values in the program.

    Here is the list of all keywords of TB:

    LET GOTO REM IF...THEN GOSUB CLEAR INPUT RETURN LIST PRINT END RUN

    You can find manual for TB on the internet in few places.
    E.g.: http://users.telenet.be/kim1-6502/tinybasic/tbum.html

2) Customizing TB.

   If you ever modify the TB port included with this project, especially
   when you relocate it in memory, remember than Tiny Basic programs often
   use hardcoded address of the Peek and Poke ML routines. You need to find
   that code in the BASIC code and modify it accordingly so the program runs
   properly in your relocated interpreter.
   A good example of this is BASIC code in tinyadventure_bas.txt file.
   In this code, variable Z is expected to hold address of the single byte
   peek routine, which in currently implemented port which loads at address
   $0B00 is located at $0B14 (2836 decimal):

   110 Z=2836
   [...]
   750 PR"FOR HINTS ON WHAT YOU CAN DO,"
   760 PR"TYPE H (FOR HELP). HIT 'RETURN'"
   770 PR"KEY TO ENTER YOUR SELECTION."
   780 PR
   790 PR"HAPPY HUNTING!"
   880 PR"OK";
   890 GOSUB1710
   [...]
   1650 REM TEST END OF INPUT LINE (0= YES)
   1650
   1660   X=USR(Z,USR(Z,46))-13
   1660   X=USR(Z,USR(Z,47))-13
   1670 RETURN
   1700
   1700 REM INPUT SUBROUTINES
   1700
   1710   X=USR(Z+4,USR(Z,46),13)
   1710   X=USR(Z+4,USR(Z,47),13)
   [...]
