/*
 *  TAMIC_soft(R) 1990,1991,1992 vR1.01
 *
 *  Declarations and definitions for complex mathematics.
 *
 * Define the macro switch: INLN_COMPLEX,
 * if you wish to use macros expanding to inline code (instead of
 * functions) for the complex conversion functions.
 * Beware of duplicate names!
 *
 *  FILE : \source\include\complex.h
 *
*/

#ifndef _COMPLEX_H
  #define _COMPLEX_H

// #ifndef _COMPLEX_DEFINED
//   #define _COMPLEX_DEFINED
//
//  typedef struct tag_complex { _real x;
//                               _real y; } _complex;/* real and imaginary parts */
//
// #endif

  /* pseudo functions */

#define itocx( i, j ) ftoc((_real)i, (_real)j)
#define ltocx( i, j ) ftoc((_real)i, (_real)j)



_real    aimag( _complex );  /* imaginary component of complex number */
_real    ctof( _complex );   /* complex (real component) to float conversion */
_complex ftoc(_real, _real); /* convert floats to complex */
_complex conjg( _complex );  /* complex conjugate */
_complex cneg( _complex );           /* negation of a complex number */



  /* functions supporting complex expressions */

_complex cadd( _complex, _complex ); /* sum of two complex numbers */
_complex csub( _complex, _complex ); /* difference of two complex numbers */
_complex cmul( _complex, _complex ); /* product of two complex numbers */
_complex cdiv( _complex, _complex ); /* quotient of two complex numbers */
_complex cpow( _complex, _complex ); /* general complex power */
_complex cpowf( _complex, _real );   /* general complex raised to float power */
int      ceq( _complex, _complex );  /* return TRUE if complex numbers equal */

  /* complex library functions */

_real    arg_cpx( _complex ); /* direct angle (in radians) of complex number */
_real    cphase( _complex ); /* angle (in radians) of complex number from 0 to -2*PI*/
_real    cabsv( _complex );   /* complex absolute value */
_real cabsm( _complex z );    /* sqrt from difference of squares */
_real csqsum( _complex z );
_real csqsub( _complex z );
_complex csqrt( _complex );  /* (positive) sq root of complex number */
_complex clog( _complex );   /* complex natural log */
_complex cexp( _complex );   /* complex exponential */
_complex csin( _complex );   /* sine of complex number */
_complex ccos( _complex );   /* cosine of complex number */
_complex ccosh( _complex z );
_complex csinh( _complex z );


#endif
