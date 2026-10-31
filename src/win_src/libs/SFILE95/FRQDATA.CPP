/*
**  TAMIC_soft(R) 1991 v1.00 01/09/91 10:10am
**
**  frequency data table.
**
**  FILE : ..\tamic\indan\frqdata.c
**
**  LIB  : rindan_X.lib
**
**  OVERLAY: NO
**
*/

#include <stdio.h>

#include <Typedef.h>
#include <TMCLIMIT.H>
#include <FRQDATA.H>
#include <Error1.h>


static _real frq_buff[FREQ_MAX_NUM];  /*buffer for frequency */

static FREQUENCY frq = { 0,                    /* current frequency point  */
                         0,                    /* maximal frequency point  */
                         frq_buff,             /* frequency point array    */
                         3,                    /* current unit code        */
                         {(_real)1.0,          /* unit for Hz              */
                          (_real)1.0e3,        /* unit for KHz             */
                          (_real)1.0e6,        /* unit for MHz             */
                          (_real)1.0e9},       /* unit for GHz             */
                         { "_Hz",               /* unit items string for Hz */
                           "KHz",              /* unit items string for KHz*/
                           "MHz",              /* unit items string for MHz*/
                           "GHz"} };           /* unit items string for GHz*/

int set_frequency( _real f0, _real df, int nf )
 {
  int num;

  frq.max = nf;
  f0 *= frq.unit[frq.code];
  df *= frq.unit[frq.code];

  for( num = 1, frq.f[0] = f0; num < frq.max; num++ )
   frq.f[num] = frq.f[num-1] + df;

  return 0;
 }


int set_frequency_arr( _real *ftmp, int nf )
 {
  int  num;

  frq.max = nf;

  for( num = 0; num < frq.max; num++ )
   frq.f[num] = ftmp[num]*frq.unit[frq.code];

  return 0;
  }


_real get_frq_f0( int unit_flag )  /* if unit_flag == 1  then return frequency in current units */
 {
  _real f0 = frq.f[0];

   return( (unit_flag != 1)? f0:(f0/frq.unit[frq.code]) );
  }

_real get_frq_df( int unit_flag )  /* if unit_flag == 1  then return frequency in current units */
 {
  _real df;

  df = ( frq.max == 1 )? (_real)0.0: frq.f[1] - frq.f[0];
  return( ( unit_flag == 1 )? (df /= frq.unit[frq.code]):df );
  }

int   get_frq_nf( void ) { return( frq.max ); }
int   set_frq_nf( int nMax )
  {
   if( ( nMax <= 0 )||( nMax > FREQ_MAX_NUM ) )
    {
     put_error_messege( BAD_NF_PARM, nMax );
     return -1;
    }
   frq.max = nMax;
   return 0;
  }

const char  * get_frq_item( int num ) { return( frq.item[num] ); }
const char  * get_current_frq_item( void ) { return( frq.item[frq.code] ); }
_real get_frq_unit( int num ) { return frq.unit[num]; }
void  set_frq_item_num( int num ) { frq.code = num; }
int   get_frq_item_code( void ) { return frq.code; }
_real get_frq_element( int index ) { return frq.f[index]; }
void  set_frq_element( _real fValue, int index ) { frq.f[index] = fValue; }
void  set_frq_element_unit( _real fValue, int index ) { frq.f[index] = fValue*frq.unit[frq.code]; }
_real get_current_frq( void ) { return frq.f[frq.num]; }
double get_current_dbl_frq( void ) { return( (double)frq.f[frq.num] ); } /* dummy for writing */
int   get_current_frq_num( void ) { return frq.num; }
void  set_current_frq_num( int num ) { frq.num = num; }
FREQUENCY * GetPointerToFrq( void ) { return &frq; }

