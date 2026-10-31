/*
**  TAMIC_soft(R) 1990,1991 v1.00
**
**  tmclimit.h
**
**  FILE : tmclimit.h
**
**  LIB  :
**
**  OVERLAY:
**
*/

#ifndef _TMCLIMIT_H
 #define _TMCLIMIT_H

  #define DATA_BASE_SIZE    0x1FFF


#define MAX_STRING_LEN    512
#define DAMP_LENGTH      2048
#define SCREEN_STRING_LEN 60

#define FREQ_MAX_NUM      100                    /* maximal frequency number*/

#define MEDEA_TABLE_SIZE   12
#define TOLER      (_real)1.0e-3

  #define MAX_INPUT_NUM     20


#ifndef MAX_WINDOW_NUM
 #define MAX_WINDOW_NUM    20
#endif

   #define MATRIX_BUFF_SIZE  0x1000

#define MAX_AGNV_NUM      200
#define GP_NUM_MAX        100
#define DATA_LINK_SIZE    2000
#define DATA_TRN_SIZE     3000
#define FE_ARRAY_DIM      10000
/*#define FE_ARRAY_DIM	    300 */

#define MAX_GPC1_POLYGON  500

#endif
