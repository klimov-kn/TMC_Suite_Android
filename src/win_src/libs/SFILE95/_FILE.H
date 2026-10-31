/*
**  TAMIC_soft(R) 1989,1990,1991 v1.00
**
** Macros and declarations for file_h program
**
** FILE: a\include\file.h
**
*/

#ifndef __FILE_H
  #define __FILE_H


#ifndef FILENAME_MAX
 #define FILENAME_MAX   63
#endif

#define FILENAME       13

#define PATH_TOO_LONG  95
#define BAD_DRIVE      15
#define BAD_PATH        3

#define FILE_FOUND      0
#define NO_FILE         2
#define OUT_OF_FILE    18
#define END_OF_FL    (-1)
#define HARD_ERROR   (-2)


#define FL_S          256
#define _SAVE           1
#define _LOAD           0


struct menu_window_size
	{
	 int h1, h2;
	 };

struct scale_parm
	  {
	   int fill;
	   int color;
	   float dy;
	   int height;
	   };

struct scale_parm_h
	  {
	   int fill;
	   int color;
	   float dy;
	   int height;
	   };


void delete_files( char *file_mask );
void delete_S_files( char *file_mask );
int  copy_file( char *source, char *target );

void init_all_window( int x_pos, int y_pos,   WINDOW *l0_wndw,
                           WINDOW *l1_wndw, WINDOW *l2_wndw,
                           WINDOW *l3_wndw, WINDOW *l4_wndw,
                           WINDOW *l5_wndw);
void drow_load_wn( WINDOW *l0_wndw, WINDOW *l1_wndw,
                        WINDOW *l4_wndw, WINDOW *l5_wndw,
                        char **dn_mess);
int  get_drive_number( void );
int  get_current_driver( void );
int  check_drive( char drive_code );
int  set_default_drive( char drive_code );
int  set_path( char *path );
int  get_current_dir( char *path, char drive);
int  get_full_path( char *path_buff, char drive);
int  get_file_list( char *file_l[], char *file_mask, int fl_size, int *f_num );
int  get_dir_list( char **dir_l, char *file_mask, int fl_size, int *f_num);
int  get_drive_list( char **drive_list );
void drow_vert_scale( WINDOW *l5, int offset );
void clr_dir_list( WINDOW *l3_wndw );
int  drow_dir_list( WINDOW *l3_wndw, char **path_l,
                         int pth_size, int first_item, int curr_item, int prev,
                         WINDOW *l5);
void drow_hor_scale( WINDOW *l4, int offset );
void clr_file_list( WINDOW *l2_wndw );
int  drow_file_list( WINDOW *l2_wndw, char **fl_l,
                          int fl_size, int first_item, int curr_item, int prev,
                          WINDOW *l4_wndw);
int  dir_menu( ACTIVE_CHAR *ach1, WINDOW *l3_wndw, WINDOW *l5_wndw,
                    char **dir_l, int pth_size, int *first_item,
                    int *current, int *ch, char *path_buff,
                    WINDOW *l1_wndw, int num_i,
                    int *prev_f, int *curr_f, int *enter_flag,
                    int x, int y);
int  file_menu( ACTIVE_CHAR *ach1, WINDOW *l2_wndw, WINDOW *l4_wndw,
                     char **fl_l, int fl_size, int *first_item,
                     int *current, int *ch, char *name_buff,
                     WINDOW *l1_wndw, int num_i,
                     int *prev_f, int *curr_f, int *enter_flag,
                     int x, int y);
char *get_file_name( int x_pos, int y_pos, FILE_LIST *fl, int load_save_flag);
void clr_file_list( WINDOW *l2_wndw );
int  drow_file_list( WINDOW *l2_wndw, char **fl_l,
                          int fl_size, int first_item, int curr_item, int prev,
                          WINDOW *l4_wndw);
int  check_path( char *path );
int  check_file_name( char *name ); 


#endif

