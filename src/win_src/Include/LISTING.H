/*
**  TAMIC_soft(R) 1989 v1.00
**
** Macros and declaration for listing output programs.
**
** FILE: \include\listing.h
**
*/

#ifndef _LISTING_H
  #define _LISTING_H



#define WR_BEG          1
#define WR_END          0


struct listing_data
          {
           char filename[FILENAME_MAX]; /* file name*/
           char title[256];             /* listing title*/
           char subtitle[256];          /* listing subtitle*/
           FILE *f;                     /* internal file pointer*/
           int line_num;                /* listing line number*/
           int page_num;                /* listing page number*/
           int page_width;              /* listing page width*/
           int page_size;               /* listing page size*/
           Uint numeration_flag;        /* listing numeration flag*/
           Uint echo_flag;              /* listing echo flag*/
           };

void set_listing_title( char *title );
void set_listing_subtitle( char *title );
int  open_listing_file( int open_mode_flag );
void close_listing_file( void );

void set_listing_page_size( int size );
void set_listing_numeration_flag( Uint size );

void set_listing_echo_flag( Uint value );
int  get_listing_echo_flag( void );

char *get_listing_file_name( void );

int  put_listing_string( char *string );
void put_list_title( void );
int  l_printf( char *format_string, ...);


#endif
