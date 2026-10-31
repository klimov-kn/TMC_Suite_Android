/*
**
**  def_str.      : 12 function for includer/definer
**
**  function      :
**                  read_buf - gets string from buffer
**
**  HEDER FILE'S  :
**                  <stdio.h>
**                  <bufm.h>
**
**  LIB           : oindtX.lib
**
**  OVERLAY       : YES
**
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <TMCLIMIT.H>
#include <Typedef.h>
#include <TPL_FILE.H>
#include <MAINWNDW.H>
#include <Error1.h>
#include <DESCRPTR.H>

#include "Prepr1.h"





int read_buf( char *str, int n, BUFFER *buf)
 {
  int  i, j1;

  j1 = buf->inp_p;
  if( buf->inp_p >= 0) buf->inp_p = buf->ilex[buf->inp_p];  /*  skeep input pointer*/

  for( i = 0; ( i < n )&&( buf->inp_p >= 0 )&&( BUF_LEX != '\0' )&&( BUF_LEX != '\n' ); i++)
   {
    str[i] = BUF_LEX;
    buf->inp_p = buf->ilex[buf->inp_p];         /*  skeep input pointer*/
    };

  if( buf->lsplex[buf->ilex[buf->inp_p]] == '\0')  buf->inp_p = buf->ilex[buf->inp_p];
  str[i] = '\0';

  if( (BUF_LEX == '\0') || ( BUF_LEX == '\n') )
   {
    if( buf->inp_p < 0 ) buf->inp_p = j1;

    return( BUF_NULL );
    }

  if( buf->inp_p < 0 )
   {
    buf->inp_p = j1;
    return( BUF_EOF );
    };

  put_error_messege( "very long string" );
  return( BUF_ABN );
  }


