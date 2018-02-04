/*
 *----------------------------------------------------------------------------
 * File:  bin2hex.c
 *
 * Author: Marek Karcz
 *
 * Date created: 1/1/2012
 *
 * Purpose:
 *
 *    Converting raw binary program files produced by cc65/cl65 compiler/linker
 *    to plain text write memory statements understood by M.O.S. running on
 *    MKHBC-8-Rx homebrew 8-bit computer system. Such file can be then sent
 *    to the computer system using "Send Text File" feature of the terminal
 *    emulation program (e.g: Hyper Terminal Private Edition).
 *
 * Revision history:
 *
 * 1/1/2012:
 * 	Created.
 * 	
 * 1/5/2012:
 * 	Added options -x and -s.
 * 	Extended write memory data to 16 bytes.
 *
 * 1/19/2012:
 * 	Added option -z (suppress lines with all values = 0).
 * 	This will shorten upload times in HT.
 * 
 * 2/11/2016
 *  After migrating to Dec C++, the output file has extra
 *  new line between each meaningful data line.
 *  Replaced \r\n with \n.
 *
 * 2/21/2016
 *	Bug corrected - input file opened in text mode instead of binary.
 *----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int DEBUG = 0;

char g_szInputFileName[256];
char g_szHexFileName[256];
int g_nAddWriteSt = 0;
int g_nStartAddr = 2048; // $0800
int g_nExecAddr = 2048;	// $0800
int g_nSuppressAutoExec = 1;
int g_nSuppressAllZeroRows = 0;

char g_aszHexTbl[][256] =
{
"00","01","02","03","04","05","06","07","08","09","0a","0b","0c","0d","0e","0f",
"10","11","12","13","14","15","16","17","18","19","1a","1b","1c","1d","1e","1f",
"20","21","22","23","24","25","26","27","28","29","2a","2b","2c","2d","2e","2f",
"30","31","32","33","34","35","36","37","38","39","3a","3b","3c","3d","3e","3f",
"40","41","42","43","44","45","46","47","48","49","4a","4b","4c","4d","4e","4f",
"50","51","52","53","54","55","56","57","58","59","5a","5b","5c","5d","5e","5f",
"60","61","62","63","64","65","66","67","68","69","6a","6b","6c","6d","6e","6f",
"70","71","72","73","74","75","76","77","78","79","7a","7b","7c","7d","7e","7f",
"80","81","82","83","84","85","86","87","88","89","8a","8b","8c","8d","8e","8f",
"90","91","92","93","94","95","96","97","98","99","9a","9b","9c","9d","9e","9f",
"a0","a1","a2","a3","a4","a5","a6","a7","a8","a9","aa","ab","ac","ad","ae","af",
"b0","b1","b2","b3","b4","b5","b6","b7","b8","b9","ba","bb","bc","bd","be","bf",
"c0","c1","c2","c3","c4","c5","c6","c7","c8","c9","ca","cb","cc","cd","ce","cf",
"d0","d1","d2","d3","d4","d5","d6","d7","d8","d9","da","db","dc","dd","de","df",
"e0","e1","e2","e3","e4","e5","e6","e7","e8","e9","ea","eb","ec","ed","ee","ef",
"f0","f1","f2","f3","f4","f5","f6","f7","f8","f9","fa","fb","fc","fd","fe","ff"
};


void ScanArgs(int argc, char *argv[]);
void ConvertFile(void);
char *ToHex(int addr);


int main(int argc, char *argv[])
{
   ScanArgs(argc, argv);
   ConvertFile();
}


/*
 * bin2hex -f InputFile -o OutputFile -w StartAddr
 */
void ScanArgs(int argc, char *argv[])
{
   int n = 1;

   while (n < argc)
   {
      if (strcmp(argv[n], "-f") == 0)
      {
         n++;
         strcpy(g_szInputFileName,argv[n]);
      }
      else if (strcmp(argv[n],"-o") == 0)
      {
         n++;
         strcpy(g_szHexFileName,argv[n]);
      }
      else if (strcmp(argv[n],"-w") == 0)
      {
         g_nAddWriteSt = 1;
         n++;
         g_nStartAddr = atoi(argv[n]);
		 		 g_nExecAddr = g_nStartAddr;
		 		 g_nSuppressAutoExec = 0;
      }
      else if (strcmp(argv[n],"-x") == 0)
      {
         n++;
         g_nExecAddr = atoi(argv[n]);
		 		 g_nSuppressAutoExec = 0;
      }
      else if (strcmp(argv[n],"-s") == 0)
      {
		 		 g_nSuppressAutoExec = 1;
      }
      else if (strcmp(argv[n],"-z") == 0)
      {
		 		 g_nSuppressAllZeroRows = 1;
      }

      n++;
   }
}

void ConvertFile(void)
{
   FILE *fpi = NULL;
   FILE *fpo = NULL;
   unsigned char bt[17];
   char hex[80];
   int i, addr, allzero;

   addr = g_nStartAddr;
   printf("Processing...\n");
   printf("Start address: %s\n", ToHex(addr));
   if (NULL != (fpi = fopen(g_szInputFileName,"rb")))
   {
      if (NULL != (fpo = fopen(g_szHexFileName,"w")))
      {
         while(0 == feof(fpi))
         {
					 memset(bt, 17, sizeof(char));
           memset(hex, 80, sizeof(char));
					 if (DEBUG) printf("Reading input file...");
           fread(bt, sizeof(char), 16, fpi);
					 if (DEBUG) printf("done.\n");
           if (g_nAddWriteSt)
               sprintf(hex, "w %s", ToHex(addr));
					 if (DEBUG) printf("Preparing hex string...\n");
					 allzero = 1;
           for(i=0; i<16; i++)
           {
			   		 if (DEBUG) printf("Code: %d\n", bt[i]);
			   		 sprintf(hex, "%s %s", hex, g_aszHexTbl[bt[i]]);
			   		 if (allzero && bt[i] > 0)
			      		allzero = 0;
           }
					 if (0 == g_nSuppressAllZeroRows
							||
							(g_nSuppressAllZeroRows && 0 == allzero)
			   			)
					 {
          	 sprintf(hex, "%s\n", hex);
						 if (DEBUG) printf("Adding line: %s", hex);
             fputs(hex, fpo);
					 }
           addr += 16;
         }
		 		 if (0 == g_nSuppressAutoExec)
		 		 {
           memset(hex, 80, sizeof(char));
           sprintf(hex, "x %s\n", ToHex(g_nExecAddr));
		    	 if (DEBUG) printf("Adding line: %s", hex);
           fputs(hex, fpo);
		 		 }
         fclose(fpi);
         fclose(fpo);
		 		 printf("Done.\n");
		 		 printf("End address: %s\n", ToHex(addr));
		 		 printf("Run address: %s\n", ToHex(g_nExecAddr));
      }
      else
         printf("ERROR: Unable to create output file.\n");
   }
   else
      printf("ERROR: Unable to open input file.\n");
}

char *ToHex(int addr)
{
   short lo, hi;
   static char ret[5];

   memset(ret, 5, sizeof(char));
   hi = addr / 256;
   lo = addr - hi*256;
   sprintf(ret,"%s%s", g_aszHexTbl[hi], g_aszHexTbl[lo]);

   return ret;
}
