/*
**
**  defmis.       : 3 function for includer/definer
**
**  function      :
**                  def_macro - return pointer on string '#define' in buffer
**                  def_include - include file in buffer and delete comment '\\' in buffer
**                  def_sl - delete comment '\\' in buffer
**  HEDER FILE'S  :
**                  <stdio.h>
**                  <stdlib.h>
**                  <string.h>
**                  <bufm.h>
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
#include <TMC_LIB.H>
#include <Error1.h>
#include <MAINWNDW.H>
#include <TPL_FILE.H>
#include <INDAN.H>

#include "Prepr1.h"

static char  szDefine1[] = "#define ";
static char  szDefine2[] = "#define\t";
                   /*  return pointer on string '#define' in buffer*/
int  def_macro( BUFFER *buf )
 {
  int p, p1;

  p = buf->inp_p;

  while( (buf_cmpn2( buf, 8, szDefine1, szDefine2 ) != BUF_NULL )&&( buf->inp_p >= 0) )
   {
    buf->inp_p = buf->ilex[ buf->inp_p ];
    }

  p1 = buf->inp_p;
  buf->inp_p = p;
  return( p1 );
  }

[[maybe_unused]] static void  put_inc_line_number( Uint line_number );
                   /*  include file in buffer and delete comment '\\' in buffer*/
static char  szInclude1[] = "#include ";
static char  szInclude2[] = "#include\t";

int def_include( BUFFER *buf )
 {
  FILE *in;

  char		*str;
  char		*str1;
  char		*inf;

  if( (str = new char[ BUF_STR_N_MAX ]) == NULL )
  {
	  return -1;
  };
  if( (str1 = new char[ BUF_STR_N_MAX ]) == NULL )
  {
	  delete []str;
	  return -1;
  };
  if( (inf = new char[ BUF_STR_N_MAX ]) == NULL )
  {
	  delete []str1;
	  delete []str;
	  return -1;
  };

//  char  *lpTmp;

  int        p, p1, n1, i;

  p = buf->inp_p;

  buf->inp_p=0;
  def_sl( buf );                                  /*  delete  coment*/

  buf->inp_p = p;

  while(  buf->inp_p >= 0 )                       /*  find include file name*/
   {
    if(buf_cmpn2( buf, 9, szInclude1, szInclude2 ) == BUF_NULL )       /*  if find '#include'*/
     {
      p1 = buf->inp_p;
      if( buf_gets( str, BUF_STR_N_MAX, buf) != BUF_NULL )
	  {
		  delete []inf;
		  delete []str1;
		  delete []str;
		  return -1;
	  };
      n1 = strlen(str);
      buf->inp_p = p1;

      buf_dels( buf, n1);                        /*  delete string with '#include'*/

      str[n1-1] = '\0';

	  for( i = 9; (i < BUF_STR_N_MAX)&&( str[i] == ' '); i++ )
	  {
		  ;
	  };

      sprintf( inf, "%s\\%s", GetCurrentPath(), str + i);

      if( strlen( inf ) > FILENAME_MAX  )
        {
		  delete []inf;
		  delete []str1;
		  delete []str;
         put_error_messege( "FileName is long" );
         return -1;
        }

      if( (in = fopen( inf, "r")) == NULL)       /*  open including file*/
       {
		  delete []inf;
		  delete []str1;
		  delete []str;
        put_error_messege( "open file" );
		return(DEF_ERROR_INFILE);
	   }                                       /*  read including file in buffer*/


      while( fgets( str1, BUF_STR_N_MAX, in) != NULL )
       {
		   DeleteComment(str1);
        /* comments skipping */

	if( buf->inp_p_nul < 0 )
	 {
          put_error_messege( "Buffer is small" );
          fclose( in );
		  delete []inf;
		  delete []str1;
		  delete []str;
          return(DEF_ERROR_MEMORY);
          }

        if( buf_puts3( str1, buf) != BUF_NULL )
         {
          fclose( in );
		  delete []inf;
		  delete []str1;
		  delete []str;
          return -1;
          }
       }

      fclose( in );

      buf->inp_p=0;
      def_sl( buf );                              /* delete  coment*/

      buf->inp_p = p1;
      }
    else
     {
      buf->inp_p = buf->ilex[ buf->inp_p ];
      }

    }

  delete []inf;
  delete []str1;
  delete []str;
  buf->inp_p = p;
  return( 0 );
  }


static char  szFmt[] = "Incl.line:%3d";

[[maybe_unused]] static void  put_inc_line_number( Uint line_number )
 {
  char buff[20];

  sprintf( buff, szFmt, line_number );
//  put_trace( buff );
  }


                   /*  delete comment '\\' in buffer*/
static char  mac[]  = "\\\\";
static char  str1[] = " \n";

int  def_sl( BUFFER *buf )
 {
  char *str;
  if( (str = new char[ BUF_STR_N_MAX ]) == NULL )
  {
	  return -1;
  };

  int  p, p1, n1;

  p = buf->inp_p;

  while(  buf->inp_p >= 0 )       /*  loop buffer*/
   {
    if(buf_cmpn( buf, 2, mac) == BUF_NULL )     /*  if find '\\'coment*/
     {
      p1 = buf->inp_p;
      if( buf_gets( str, BUF_STR_N_MAX, buf) != BUF_NULL )
	  {
		  delete []str;
		  return -1;
	  };
      buf->inp_p = p1;
      n1 = strlen(str);
      buf_dels( buf, n1+1);                    /*  delete coment*/
      if( buf_puts2(str1, buf) != BUF_NULL ) 
	  {
		  delete []str;
		  return -1;
	  };
      }
    else
     {
      buf->inp_p = buf->ilex[ buf->inp_p ];
      }

    }

  buf->inp_p = p;
  delete []str;
  return(0);
  }

char  *DefineIncludeName( char *str, char * /*str1*/ )
  {
return str;
  }


