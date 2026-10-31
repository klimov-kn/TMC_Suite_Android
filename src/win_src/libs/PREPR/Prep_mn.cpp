/*
**
**  def_str.      : 2 function for includer/definer
**
**  function      :
**                 main_preproc - worker includer/definer
**  HEDER FILE'S  :
**                  <stdio.h>
**                  <stdlib.h>
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
#include <malloc.h>
#include <stdlib.h>

#include <Typedef.h>
#include <TMCLIMIT.H>
#include <MAINWNDW.H>
#include <TPL_FILE.H>
#include <TMC_LIB.H>
#include <RUN_FLAG.H>
#include <Error1.h>

#include "Prepr1.h"



BUFFER buf;
char *szError;
char *szPathName;

                                          /*  worker definer/includer*/

                                          /*  worker definer/includer*/

/* M15: the expanded task is kept in memory.
   csTplFileNamePrepr == NULL - no temporary file <task>~$Prepr$~, the text goes
   to the in-memory sink below (see MainPreprMem). */
static char  *pszMemOut = NULL;
static size_t nMemOut = 0;
static size_t nMemCap = 0;

static int MemPuts( const char *str )
{
	size_t n = strlen( str );
	if( nMemOut + n + 1 > nMemCap )
	{
		size_t nNew = ( nMemCap == 0 ) ? 65536 : nMemCap;
		while( nMemOut + n + 1 > nNew ) nNew *= 2;
		char *p = (char *)realloc( pszMemOut, nNew );
		if( p == NULL ) return -1;
		pszMemOut = p;
		nMemCap = nNew;
	};
	memcpy( pszMemOut + nMemOut, str, n + 1 );
	nMemOut += n;
	return 0;
}

/* Expands csTplFileName into a buffer allocated with malloc (caller frees it with
   free()). *ppszPrepr == NULL - nothing was produced (as the former "no file"). */
void MainPreprMem( char **ppszPrepr, char *csTplFileName, char *csCurrentPath, char *csError )
{
	pszMemOut = NULL;
	nMemOut = 0;
	nMemCap = 0;
	MainPrepr1( NULL, csTplFileName, csCurrentPath, csError );
	*ppszPrepr = pszMemOut;
	pszMemOut = NULL;
	nMemOut = 0;
	nMemCap = 0;
	return;
}

void MainPrepr1( char *csTplFileNamePrepr,	char *csTplFileName, char *csCurrentPath, char *csError )
{
	
	sprintf( csError, "%s", "");

	if( (szError = (char *)alloc_mem_err( sizeof(char)*BUF_STR_N_MAX )) == NULL )
	{
		printf( csError, "%s", "Memory allocation");
		return;
	};
	if( (szPathName = (char *)alloc_mem_err( sizeof(char)*BUF_STR_N_MAX )) == NULL )
	{
		free_mem( szError );
		printf( csError, "%s", "Memory allocation");
		return;
	};

	char *ch;
	char szSourceFileName[512];
	char szTargetFileName[512];
	
	strcpy( szSourceFileName, csTplFileName);
	if( csTplFileNamePrepr != NULL ) strcpy( szTargetFileName, csTplFileNamePrepr);
	strcpy( szPathName, csCurrentPath);
	
	ch = main_preproc( ( csTplFileNamePrepr != NULL ) ? szTargetFileName : NULL, szSourceFileName );
	unload_prep_buf();
	dest_prep();

	if( strlen(ch) != 0 )
	{
		sprintf( csError, "%s", ch );
	};
	
	free_mem( szPathName );
	free_mem( szError );

	return;
}



char * main_preproc( char *szTargetFileName, char *szSourceFileName )
 {
  int i_err;
  char *str;
  FILE *pfIn, *pfOut;

  szError[0] = '\0';

  if( (str = new char[ BUF_STR_INLINE_MAX ]) == NULL )
  {
	  sprintf( szError, "memory allocation" );
	  return szError;
  };

  if( (pfIn = fopen( szSourceFileName, "r" )) == NULL ) 
  {
	  sprintf( szError, "file %s isn't open", szSourceFileName );
	  delete []str;
	  return szError;
  }

  buf_init( &buf, NMAX);                               /*  init buffer*/
  buf.inp_p = 0;
  while( fgets( str, BUF_STR_INLINE_MAX, pfIn ) != NULL )				  /* read data in buffer */
   {
    if( buf.inp_p < 0 )
     {
	  sprintf( szError, "Init" );
	  fclose( pfIn );
	  delete []str;
      return szError;
      }
	DeleteComment( str );
    if( buf_puts3( str, &buf ) != BUF_NULL ) 
	{
		delete []str;
		return szError;             /*  put string in buffer*/
	};
//    if( strstr( str, str1 ) != NULL ) return 0;
    }
  fclose( pfIn );

  buf.inp_p = 0;

  if( ( i_err = def_include( &buf ) ) == 0 )                          /* include file*/
   {
    buf.inp_p = 0;
    if( ( i_err = load_vr_file( &buf ) ) != BUF_NULL) 
	{
		delete []str;
		return szError;
	};

    buf.inp_p = 0;
    if( ( i_err = def_err_dec( &buf ) ) != BUF_NULL)
	{
		delete []str;
		return szError; /*  is redefine in buffer?*/
	};

    buf.inp_p=0;                                                      /* macro define*/
    if( ( i_err = def_main_macro( &buf )) != BUF_NULL ) 
	{
		delete []str;
		return szError;
	};
    }

  buf.inp_p = 0;
  if( szTargetFileName == NULL )                      /* M15: output to memory */
  {
	  if( MemPuts( "" ) != 0 )
	  {
		  sprintf( szError, "memory allocation" );
		  delete []str;
		  return szError;
	  };
	  while( buf_gets( str, BUF_STR_N_MAX, &buf ) != BUF_EOF )
	  {
		  if( MemPuts( str ) != 0 )
		  {
			  sprintf( szError, "memory allocation" );
			  delete []str;
			  return szError;
		  };
	  };
	  delete []str;
	  return szError;
  };
  if( (pfOut = fopen( szTargetFileName, "w" )) == NULL ) 
  {
	  sprintf( szError, "file %s isn't open", szTargetFileName );
	  delete []str;
	  return szError;
  }										   
  while( buf_gets( str, BUF_STR_N_MAX, &buf ) != BUF_EOF )
  {
     fputs( str, pfOut);
   };
  fclose( pfOut );


  delete []str;
  return szError;
  }


int  unload_prep_buf( void )
 {
  if( buf.lsplex != NULL ) free_mem( buf.lsplex );
  if( buf.ilex != NULL ) free_mem( buf.ilex );
  return( 0 );
  }

int    put_error_messege( const char *szError1 )
{
	strcpy( szError, szError1 );
	return 0;
}


char *GetCurrentPath( void )
{
	return szPathName;
}


void DeleteComment( char *pszStr )
{
  int i;
  for( i = 0; i < BUF_STR_N_MAX; i++ )
  {
	  switch( pszStr[i] )
	  {
		case '!':
//			pszStr[i] = '\n';
//			pszStr[i+1] = '\0';
			strcpy( (pszStr + i) , " \n");
//			pszStr[i] = 13;
//			pszStr[i+1] = 10;
//			pszStr[i+2] = '\0';
			return;
			break;
		case '\0':
			return;
			break;
	  };
  };
  return;
}


int InitCharArray( char **ch, int n )
{
	if( *ch != NULL )
	{
		free( *ch );
		*ch = NULL;
	};
	if( ((*ch) = ( char *)malloc( n )) == NULL ) return -1;
	return 0;
}

int FreeCharArray( char **ch )
{
	if( *ch != NULL )
	{
		free( *ch );
		*ch = NULL;
	};
	return 0;
}
