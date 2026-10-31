#ifndef _TYPDEF_H
  #define _TYPDEF_H

//#include <windows.h>

#ifndef _NDP2
 #define _NDP2
#endif


#ifndef _HIGH_ACCURACY
  #define _HIGH_ACCURACY
#endif

#ifndef _ELEM_VAL
 #define _ELEM_VAL _real
 #ifndef _ELEM_COMPLEX
  #define _ELEM_COMPLEX
 #endif
#endif

 #define _near
 #define  near

 #define _far
 #define  far

 #define _huge
 #define  huge

 #define _fortran
 #define  fortran

 #define _pascal
//  #define  pascal

 #define _BASE_CODE


 #define _static_ static

#include <float.h>


typedef unsigned char Uchar;
typedef unsigned char BYTE;

typedef unsigned int   Uint;
typedef unsigned short WORD;


typedef unsigned long Ulong;
typedef unsigned long DWORD;

#ifndef FILENAME
 #define FILENAME           13
#endif
#ifndef FILENAME_MAX
 #define FILENAME_MAX      63
#endif

typedef struct { char name[FILENAME];
                 char path[FILENAME_MAX];
                 char drive;
                 } FILE_LIST;

  #define REAL_DIG  DBL_DIG    /* # of decimal digits of precision */
  #define REAL_MAX_10_EXP  DBL_MAX_10_EXP    /* max decimal exponent */
  #define REAL_MIN_10_EXP  DBL_MIN_10_EXP    /* max decimal exponent */
  #define REAL_MAX  DBL_MAX    /* max value */
  #define REAL_MIN  DBL_MIN    /* min positive value */
  #define REAL_EPSILON DBL_EPSILON /* smalest such that 1 + REAL_EPSILON != 1 */
  #ifndef _real
    typedef double _real;
  #endif

// typedef _real _far * r_array;

#ifndef __cplusplus
 #ifndef __COMPLEX_DEFINED
   #define __COMPLEX_DEFINED

  typedef struct tag_complex { _real x;
                               _real y; }_complex;/* real and imaginary parts */

 // typedef _complex _far * c_array;
 #endif
#else
 #ifndef __COMPLEX_DEFINED_TMC
   #define __COMPLEX_DEFINED_TMC

	#include <math.h>

#endif
#endif


#define MU_NUL               (_real)12.56637e-7
#define EPSILON_NUL          (_real)8.85418757e-12
#define LIGHT_VEL            (_real)2.99792458e8

#ifndef PI
 #define PI                   (_real)3.141592653589793
#endif

int isnumber( char );
#define isnumber(_c) (isdigit( _c )||( _c == '+' )||( _c == '-' )||( _c == '.' ))

int rsign( _real );
#define rsign( rn )  ( (rn >= 0)?1:-1 )

#define INT_1 short

#endif
