/*
**
**  Load preprocessor data base
**
**  function      :
**                 load_preproc_buff - load includer/definer
**                 buf_init - init buffer buf , n - number byte in buffer
**                 unload_prep_buf - unload buffer buf
**
**  LIB           : NO
**
**  OVERLAY       : YES
**
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <Typedef.h>
#include <TMCLIMIT.H>
#include <MAINWNDW.H>
#include <TMC_LIB.H>
#include <RUN_FLAG.H>
#include <Error1.h>
#include <TPL_FILE.H>

#include "Prepr1.h"




int  buf_init( BUFFER *buf, int n)
 {
  int i;

  if( (buf->lsplex = ( char * ) alloc_mem_err( (long)sizeof(char)*n )) == NULL) return( ERROR_INIT );
  if( (buf->ilex = ( int * ) alloc_mem_err( (long)sizeof(int)*n )) == NULL)
   {
    free_mem( buf->lsplex );
    put_error_messege( "Init" );
    return(ERROR_INIT);
    };

  for( i = 0; i < n; i++) buf->ilex[i] = i+1;
  buf->ilex[0] = -1; buf->inp_p = 0;
  buf->ilex[n-1] = -1; buf->inp_p_nul = 1;

  return( BUF_NULL );
  }


