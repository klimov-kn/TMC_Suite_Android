/*
**  TAMIC_soft(R) 1990,1991 v1.00
**
**  data_const_list.h
**
**  FILE : datacnst.h
**
**  LIB  :
**
**  OVERLAY:
**
*/

#ifndef _DATACNST_H
 #define _DATACNST_H


struct data_const_list
         {
          int num_step;
          _real delta;
          _real eps, mu;
          _real height;
          _real acuracy;
          };

typedef struct data_const_list DATA_CONST_LIST;

void  set_data_const_delta( _real delta );
_real get_data_const_delta( _real *delta );
_real get_data_const_delta_nrm( _real *delta );

_real GetGlobalEpsilon( void );
_real GetGlobalMu( void );

void  set_data_const_height_width( _real height );
_real get_data_const_height_width( _real *height );
_real get_data_const_height_width_nrm( _real *height );

void  set_data_const_acuracy( _real acuracy );
_real get_data_const_acuracy( _real *acuracy );

void get_dx( _real *dx );
void set_dx( _real *dx );

void set_dz( _real *dz );
void get_dz( _real *dz );

#endif
