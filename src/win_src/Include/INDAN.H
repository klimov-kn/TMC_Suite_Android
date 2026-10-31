/*
**  TAMIC_soft(R) 1989,1990,1991,1992 v1.00
**
**  indan.h
**
**  FILE : indan.c
**
**  LIB  :
**
**  OVERLAY:
**
*/

#ifndef _INDAN_H
 #define _INDAN_H

#define LISTING_MODE    0
#define SAVE_P_MODE     1


int   load_step( FILE_LIST *fl, int step_num );

int   in_data( void );
void  LoadStepStartStatOutput( void );
void  LoadStepEndStatOutput( void );
void  SeekStepErrorHandler( void );

int   in_attributes( void );
int   in_output( void );

int   s_printf( char *format, ... );

int   corr_all_data( void );

void  set_output_mode( int mode );

void  set_data_base_out_flag( int value );
int   get_data_base_out_flag( void );

void  set_DB_out_mode_flag( int value );
int   get_DB_out_mode_flag( void );

void  output_data_base( void );

void  SetIncludePathName( char *ptr );
char  * GetIncludePathName( void );

void  SetInternalDataBaseSize( DWORD dwSize );
DWORD GetInternalDataBaseSize( void );
#endif
