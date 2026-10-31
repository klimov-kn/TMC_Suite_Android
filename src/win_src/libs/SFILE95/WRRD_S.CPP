/*
**  TAMIC_soft(R) 1989,1991,1992
**
**  make_S_matrix_name   - make temporary S-file names
**  copy_S_matrix        - copy temporary file to output file if need
**
**  FILE : ..\tamic\post_pro\wrrd_S.c
**
**  LIB  : ooutpXY.lib
**
**  OVERLAY: YES
**
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <Typedef.h>
#include <TMCLIMIT.H>
#include <TMCGRAPH.H>
#include <RUN_FLAG.H>
#include <TMC_LIB.H>
#include <INDANDEC.H>
#include <COMPFILE.H>
#include <FRQDATA.H>
#include <Postpro.h>
#include <TPL_FILE.H>
#include <DATACNST.H>

  #include "_FILE.H"



char S_filename[FILENAME_MAX];
static char S_filename_buff[FILENAME_MAX];
static int file_counter = 0;


void make_S_matrix_name( char *S_name, S_INDEX *si )
 {
  char *llist_filename = get_tpl_file_name();
  char *ptr, buff[80];

                                /* store file name for control*/
  if( get_current_frq_num() == 0 )
   {
   if( file_counter == 0 )
    {
     strcpy( S_filename_buff, llist_filename );
     file_counter++;
     }
    else
     {
      if( strcmp( S_filename_buff, llist_filename ) == 0 ) file_counter = (file_counter+1)%10;
       else                      /* new name of tpl file*/
        {
         file_counter = 1;
         strcpy( S_filename_buff, llist_filename );
/*         S_filename_buff[0] = 0;*/
         }
      }
   }
                                                  /* make S_matrix file name */
  sprintf( buff, "S%d%d", file_counter, get_tpl_step() );
  if( (ptr = strchr( buff, ' ' )) != NULL ) *ptr = '0';
  change_file_extension( S_filename, llist_filename, buff );

  strcpy( si->tpl_name, llist_filename );
  strcpy( S_name, S_filename );


  }




int copy_S_matrix( void )
 {
  char buff[FILENAME_MAX], tag_name[FILENAME_MAX], *ptr;

  if( get_comp_file_flag() == 0 ) return( 0 );

  strcpy( buff, get_comp_file_name() );
  strcpy( tag_name, ( ((ptr = strrchr( buff, '\\')) != NULL)? (ptr + 1):buff) );

  if( (ptr = strrchr( tag_name, '.')) == NULL ) *(ptr = strrchr( tag_name, '\0')) = '.';
  *++ptr = '\0';

  return( copy_file( S_filename, strcat( tag_name, "S" ) ) );
  }
