/*
**  TAMIC_soft(R) 1991 v1.00
**
**  tpl_file.h
**
**  FILE : tpl_file.c
**
**  LIB  :
**
**  OVERLAY:
**
*/

#ifndef _TPL_FILE_H
 #define _TPL_FILE_H


#define DEVIDE_CHAR      ';'



Uint get_tpl_line_number( void );
void set_tpl_line_number( Uint line_number );

char *get_tpl_file_name( void );
void set_tpl_file_name( char *name );

int  open_tpl_file( FILE_LIST *fl );
void close_tpl_file( void );
void fseek_tpl_file( void );

char * fget_tpl_line( char *str_buff );
char *          get_tpl_line( char *str_buff );

int  tpl_str_scan( Uint code, char *string, ... );
int  test_unknown_string( char *string );
int  test_end_block_dscr( Uint dscr_code, char *string );
int  test_wild_block_dscr( Uint dscr_code, char *string );

int seek_step( int step_num );
char * _fget_tpl_line( char *str_buff );

int   str_scan( char devider, char *string, char *format, ... );
int   vstr_scan( char devider, char *string, char *format, va_list argptr );
int   tpl_str_scan( Uint code, char *string, ... );

char  *bf_get_tpl_line( char *str );
void  set_prepr_echo_flag( int flag );
int   get_prepr_echo_flag( void );

void  set_include_echo_flag( int flag );
int   get_include_echo_flag( void );
#endif
