/*
 *  TAMIC_soft(R) 1990,1991,1992 vR1.02
 *
 *  put_S_table - print out multimode scattering matrix to listing
 *                in the form of table
 *
 *  Usage -  put_S_table( lpxS, int inpnb, int _far *modnb );
 *
 *  Arguments - lpxS - scattering matrix
 *             inpnb - number of inputs of multiport
 *             modnb - array of number of modes at all inputs
 *
 *  Remark - scattering matrix is assumed to be stored
 *           rowwise in general storage mode
 *
 *  Reqd. routines - cabsv, cphase ( in cmplx library )
 *
 *  FILE : \source\tamic\post_pro\s_table.c
 *
 *  LIB  : ooutpXY.lib
 *
 *  OVERLAY: YES
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <Typedef.h>
#include <FRQDATA.H>
#include <LISTING.H>
#include <COMPLEX1.H>
#include <COMPFILE.H>
#include <MAINWNDW.H>


#define ONE       1.0f
#define ZERO      0.0f
#define GEN_THR   1.000001f

static char  _BASE_CODE title[] = "\n      S-matrix table in form \"magnitude-phase\" at frequency: %#10.4g Hz.";
static char  _BASE_CODE fmt1[] = "     \311\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\321\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\321\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\273";
static char  _BASE_CODE fmt2[] = "     \272     Number of     \263       S-matrix elements       \263   L or VSWR     \272";
static char  _BASE_CODE fmt3[] = "     \307\304\304\304\304\304\304\304\304\304\302\304\304\304\304\304\304\304\304\304\305\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\302\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\305\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\266";
static char  _BASE_CODE fmt4[] = "     \272  inputs \263  modes  \263 'S'-magnitude \263'S'-phase(rad.)\263   L in  dB      \272";
static char  _BASE_CODE fmt5[] = "     \314\315\315\315\315\321\315\315\315\315\330\315\315\315\315\321\315\315\315\315\330\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\330\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\330\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\271";
static char  _BASE_CODE fmt6[] = "     \272 %2d \263 %2d \263 %2d \263 %2d \263  %#12.6g \263 %#13.6g \263 %5s%-11.4g\272";
static char  _BASE_CODE fmt7[] = "     \307\304\304\304\304\305\304\304\304\304\305\304\304\304\304\305\304\304\304\304\305\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\305\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\305\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\266";
static char  _BASE_CODE fmt8[] = "     \310\315\315\315\315\317\315\315\315\315\317\315\315\315\315\317\315\315\315\315\317\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\317\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\317\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\315\274";
static char * _BASE_CODE hood[] = { fmt1, fmt2, fmt3, fmt4, fmt5 };
static char _BASE_CODE trace[] = "Prt. S-matr.";

void put_S_table( _complex *lpxS, int inpnb, int *modnb )
  {
   int i, i1, j, j1, kol, kol1;
   _real f11, r11, rkstu, rl;

   if( get_comp_file_flag() != 0 ) return ;

   put_trace( (char *)trace );

   open_listing_file( WR_END );
   l_printf( title, get_current_frq());
   for( i = 0; i < 5; i++ ) l_printf( hood[i] );  /*Red Riding Hood*/

   /* first loop over inputs */
   for( i = 0; i < inpnb; i++ )
     {
      /* first loop over modes */
      for( j = 0, kol1 = modnb[i]; j < kol1; j++ )
        {
         /* second loop over inputs */
         for( i1 = 0; i1 < inpnb; i1++ )
           {
            /* second loop over modes */
            for( j1 = 0, kol = modnb[i1]; j1 < kol; j1++ )
              {
               r11 = cabsv( (*lpxS) );
               f11 = cphase( (*lpxS) );
               lpxS++;

               if( (i == i1) && (j == j1) )
                {
                 /* reflection case */
                 if( ( r11 >= ONE )&&( r11 < GEN_THR ) )  r11 = (_real)(ONE - REAL_EPSILON);
                 if( r11 < ONE )
                   {
                    rkstu = (ONE + r11)/(ONE - r11);
                    l_printf( fmt6, i+1, i1+1, j+1, j1+1, r11, f11, "VSWR=", rkstu );
                   }
                  else
                   {
                    rl = (_real)20.0*log10( (double)ONE/r11 );
                    l_printf( fmt6, i+1, i1+1, j+1, j1+1, r11, f11, " L = ", rl );
                   }
                }
               else
                {
                 /* propagation case */
		 if( ( r11 > ONE )&&( r11 < GEN_THR ) ) r11 = ONE;
                 if( r11 == ZERO ) rl = REAL_MAX;
                 else  rl = (_real)20.0*log10( (double)ONE/r11 );
                 l_printf( fmt6, i+1, i1+1, j+1, j1+1, r11, f11, " L = ", rl );
                }

               l_printf( ( ((i+1) == inpnb) && ((j+1) == kol1) &&
                         ((i1+1) == inpnb) && ((j1+1) == kol) )? fmt8:fmt7 );

              } /*end of second loop over modes*/
           } /*end of second loop over inputs*/

        } /*end of first loop over modes*/
     } /*end of first loop over inputs*/

   close_listing_file();

  } /* end of put_S_table routine*/
