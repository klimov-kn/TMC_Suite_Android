/*
**  TAMIC_soft(R) 1989,1990,1991,1992 v1.10
**
**  postpro.
**
**  FILE : postpro.h
**
**  LIB  :
**
**  OVERLAY:
**
*/

#include <Typedef.h>
#include <COMPLEX1.H>
#include <FRQDATA.H>
#include <TMCGRAPH.H>


#define NX_SC             10
#define NY_SC             10

#define X_MAX_SCALE        5
#define Y_MAX_SCALE        5

#define MAX_GRAPH_NUM     10

#define LEFT_SIDE          0
#define RIGHT_SIDE         1

#define OLD                0
#define NEW                1
#define LAST               2
#define ALL_CURVE          3

#define _LINE              0

#define LINEAR_INTERPOL    0


#define ITEMS_NUM            9
#define ITEMS_MAX_LENGTH     8
#define ITEMS_SECOND_LENGTH 14

#ifndef _INPUT_DATA_DEF
 #define _INPUT_DATA_DEF
 typedef struct
          {
           INT_1 num;
           INT_1 array[MAX_INPUT_NUM];
           INT_1 dim;
           }INPUT_DATA;
#endif


#ifndef _S_INDEX_DEF
 #define _S_INDEX_DEF
   typedef struct
	   {
	    char *tpl_name;
	    char *S_name;
	    double delta1;
	    double delta2;
	    double delta3;
	    } S_INDEX;
#endif

#ifndef _MARKER
  #define _MARKER

typedef struct
            {
             int type;
             int width;
             int color;
             int size;
             } MARKER;
#endif

#ifndef _CURVE_INDEX
 #define _CURVE_INDEX

typedef struct
            {
             int style;
             int color;
             int width;
             } CURVE;

typedef struct
          {
           int input1;       /* input 1 number in device    */
           int mode1;        /* mode 1 number in this input */
           int input2;       /* input 2 number in device    */
           int mode2;        /* mode 2 number in this input */
           } CURVE_INDEX;
#endif

#ifndef GRAPH_CURVE
struct graph_curve
            {
             int    code;        /* -1 - deleted, 0 - not used, >0 current used*/
             MARKER m;
             CURVE  crv;
             CURVE_INDEX ci;
             INPUT_DATA id;
             char   filename[FILENAME_MAX];
             struct graph_curve *next;
             struct graph_curve *prev;
             };
typedef struct graph_curve GRAPH_CURVE;
#endif

struct _real_point { _real x, y; };
typedef struct _real_point FLOAT_POINT;



struct curve_data
            {
             int num;                   /* number of point on curve*/
             FLOAT_POINT *p;
             };
typedef struct curve_data CURVE_DATA;


struct graph_wnd_config
            {
             int x_scale, y_scale;       /* F, Wl, for x, Kctu, L .. for y*/
             int x_style, y_style;       /* graph net style line or log*/
             int x_type, y_type;         /* graph net line type*/
             int x_width, y_width;       /* graph net width*/
             int x_color, y_color;       /* graph net color*/
             int x_sc_color, y_sc_color; /* graph scale color*/
             int border_color, border_style, border_width;  /* border color*/
             int general_back_color, graph_back_color;      /* graph feald color*/
             WINDOW w;                                      /* graph int window*/
             struct float_window fw;                        /* graph float window*/
             int curve_num;                   /* number of graph curve*/
             GRAPH_CURVE *gc;            /* pointer to graph curve list*/
             GRAPH_CURVE *zerro;         /* pointer to graph curve zerro list*/
             S_INDEX si[3];                   /* s-matrix index for extrapolation*/
             struct set_char_size *ss;   /* scale char size*/
             };
typedef struct graph_wnd_config GRAPH_WND_CONFIG;

void  draw_all_curves( GRAPH_WND_CONFIG *gwc );
void  set_default_graph_wn( _complex *sss, INPUT_DATA *id, FREQUENCY *frq, GRAPH_CURVE *gc, GRAPH_WND_CONFIG *gwc );
void  draw_graph_wn( GRAPH_WND_CONFIG *gwc );

void  add_curve_to_graph ( CURVE_DATA *c_p, GRAPH_WND_CONFIG *gwc );

GRAPH_CURVE *add_curve_to_list( char *filename, GRAPH_WND_CONFIG *gwc,
                                            MARKER *mt, CURVE *ct,
                                            CURVE_INDEX *ci );
void  delete_curve( GRAPH_CURVE *gc, GRAPH_WND_CONFIG *gwc );

void  put_marker_to_graph( _complex *sss, INPUT_DATA *id, FREQUENCY *frq,
                               GRAPH_CURVE *gc, GRAPH_WND_CONFIG *gwc );
void  output_graphic( CURVE_DATA *c_ptr, GRAPH_WND_CONFIG *gwc );
_real get_point_x( int num, FREQUENCY *frq, int scale );
_real get_point_y( _complex *sss, INPUT_DATA *id, CURVE_INDEX *ci, int scale );


void  init_float_graph_wn( void );
int   corr_vert_region( _real y, struct float_window *fw, int scale);
int   corr_horiz_region( _real x, struct float_window *fw );
void  set_vert_region( _real bottom, _real tom, struct float_window *fw);
void  set_horiz_region( _real left, _real right, struct float_window *fw);
void  set_marker( _real x, _real y );
void  set_curve_type( CURVE *crv );

int   out_proc( void );
void  output_menu( GRAPH_WND_CONFIG *gwc );
void  put_out_menu_item( WINDOW *wn, char *item, struct alt_colors *fbc);
void  make_input_mode_cnfg( char *item, CURVE_INDEX *im );

int   change_y_scale( WINDOW *mw, GRAPH_WND_CONFIG *gwc );
int   file_out_menu( void );

void  legend_out( GRAPH_WND_CONFIG *gwc );
void  init_menu_window_0( WINDOW *mw_0 );

void  print_first_window( void );

void  output_menu_change( GRAPH_WND_CONFIG *gwc );
void  output_menu_add( GRAPH_WND_CONFIG *gwc );
void  output_menu_del( GRAPH_WND_CONFIG *gwc );
void  _draw_lgnd_curve( WINDOW *mw, GRAPH_CURVE *cv_ptr );

_real _Kctu( _complex *sss );
_real _Loss( _complex *sss );

void MtNrm( _real *matr, int *i, _real *norm_array, int *j,  int *ndimt);

void put_S_table( _complex *lpxS, int nInp_num, int *modnb );

