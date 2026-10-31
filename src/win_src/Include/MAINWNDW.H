/*
**  TAMIC_soft(R) 1991 v1.00
**
** Macros and declarations for mainwndw program.
**
** FILE: a\include\mainwndw.h
**
*/

#ifndef _MAINWNDW_H
  #define _MAINWNDW_H

#define MIN_TRACE_DIM  30

struct titel_color_table
                {
                 int fgTextColor;
                 int fgBorder;
                 int bgBorder;
                 int fgTBorder;
                 int bgTBorder;
                 };

struct window_colors
                {
                 int fgGeneralBorder;
                 int fgTimeColor;
                 int bgGeneralColor;
                 int fgWindowColor;
                 int fgStatisticColor;
                 int bgStatisticColor;
                 int fgMenuColor;
                 };

#define NO_PAUSE    0
#define YES_PAUSE   1

void main_window( int pause_flag, int daun_messge_flag );


#define _STAT_WNDW       0
#define _OUT_WNDW        1
#define _LEGEND_WNDW     2

#define MAX_ST_IT_LEN   11

/* ETIME collides with the ETIME macro from modern <errno.h> (UCRT);
   undef it before the enum so the enumerator compiles. win32/win64 build fix. */
#ifdef ETIME
#undef ETIME
#endif

enum ST_ITEMS
           {
            SFILE,
            STEP,
            FREQ,
            ST_LINK,
            ST_BLOCK,
            TRACE,
            ETIME,
            ESTIME,
            FREEMEM
            };


#define put_step( num )   put_stat_num( STEP, num )
#define put_link( num )   put_stat_num( ST_LINK, num )
#define put_block( num )   put_stat_num( ST_BLOCK, num )
#define put_trace( string)   stat_messege( TRACE, string )

void put_all_statistic( void );
void draw_windows( void );
void first_window( int t_num );
void second_window( int t_num );
void stat_item( int x, int y, char *item );
void put_elapsed_time( void );
void put_time( void );
void SetTimeFlag( int nFlag );
void put_frequence( void );
void put_file_name( void );
void stat_messege( int stat_item_num, char *string);
void put_stat_num( int stat_item_num, int put_num );

  int       get_dos_freemem( void );

void put_free_mem( void );
void drow_statistic_window( void );
void OutPercent( char *title, int nPercent );

#endif
