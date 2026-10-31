/*
**  TAMIC_soft(R) 1991 v1.00
**
** Macros and decloration for TAMIC_soft standart library.
**
** FILE: tmc_lib.h
*/

#ifndef _TMC_LIB_H
 #define _TMC_LIB_H


#define MEM_STACK_SIZE   10
#define SIGN_TOLERANCE   1.0e-7

struct memory_block
            {
             int size;
	     void *ptr;
             };

struct memory_stack
            {
             struct memory_block mem_blk[MEM_STACK_SIZE];
             struct memory_block *sp;
             };


  int       get_dos_freemem( void );
  int       _System_( char *lpszCommand );

void *_alloc_mem( long size );
void *alloc_mem( long size );
void *alloc_mem_err( long size );
void *alloc_mem_stack( long size );

int  _free_mem( void *mem_ptr );
int  free_mem( void *mem_ptr );
int  free_mem_null( void **ptr );
int  free_mem_stack( void *ptr );

void init_memory_stack( void );
WORD size_to_paragr( long  size );


char *make_file_name( FILE_LIST *fl, char *name);
char *make_full_file_name( char *full_name, const char *path, const char *name );
char *change_file_extension( char *new_name, const char *base_name, const char *extension );
void SplitFileName( FILE_LIST *fl, char *filename);

int  get_current_driver( void );
int  set_default_drive( char drive_code );
int  get_current_dir( char *path, char drive );
int  check_drive( char driver_code );
int  get_full_path( char *path_buff, char drive );

        /* Option File Name functions */
void set_option_file_name( const char *name );
void put_option( void );

        /* tolerance functions  */
void  set_tolerance( _real toler );
_real get_tolerance( void );
int   is_a_EQ_b( _real a, _real b );
int   Compare_A_and_B( _real a, _real b, _real tol );
_real round_to_tolerance( _real a );

long  real_to_long( _real a );
int   Fftoi( _real a );

char  * del_blanks( char *string );


/*  real function Angle( x1, x2, x3, y1, y2, y3 ) */
_real Angle( _real *x1, _real *x2, _real *x3,
			 _real *y1, _real *y2, _real *y3 );
/*  subroutine Newpt(x1,y1,x2,y2,fi1,rlong,x3,y3 ) */
void  Newpt( _real *x1, _real *y1, _real *x2, _real *y2,
			  _real *fi, _real *rlong, _real *x3, _real *y3 );
_real Distance( _real *x1, _real *y1, _real *x2, _real *y2 );
int   quadrt( _real *a, _real *root1, _real *root2, _real tol );

int   pause_wait( void);
void  DELAY( unsigned int );
void  OutTrace( char *TraceMessage);
void  PutPersent( char *lpszTitle, DWORD *lpdwCurrent,
                                                     DWORD *lpdwAll );


char *strupr( char *lpszString );
int        rmtmp( void );



#endif
