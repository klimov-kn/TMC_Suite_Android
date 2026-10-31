/*
 *  TAMIC_soft(R) 1992 vR1.02
 *
 *  put_error_messege - dummy error processing routine.
 *
 *  FILE: ..\errors.c
 *
 *  OVERLAY: NO
*/


#include <stdio.h>
#include <stdarg.h>

#include <Typedef.h>
#include <Error1.h>
#include "ERROR2.H"


char msg1[] = "Fatal ERROR.\nNot enough memory to run program.\nRemove resident programs and restart system. ";
char msg2[] = "Error !\nCan not open listing output file:\n%s!";
char msg3[] = "Warning !\nNot enough memory for all frequency points!\nOnly %d frequencies processed!";
char msg4[] = "Error !\nCan't open S-matrix data file\n%s!";
char msg5[] = "Error while reading from S-file:\n%s!";
char msg6[] = "Error !\nThis is not scattering matrix file:\n%s!";
char msg7[] = "Error !\nToo old scattering matrix file:\n%s!";
char msg8[] = "Error !\nDifferent number of inputs! See listing.";
char msg9[] = "Error !\nDifferent number of modes! See listing.";
char msg10[] = "Error !\nCan't open file for text table\n%s!";

int  put_error_messege( int /*err_num*/, ... )
  {
return 0;
  }
