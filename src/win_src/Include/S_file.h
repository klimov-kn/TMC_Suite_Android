/*
**  TAMIC_soft(R) 1991,1992 v1.00
**
**  s_file.
**
**  FILE : ..\include\s_file.h
**
**  LIB  :
**
**  OVERLAY:
**
*/

#include <Typedef.h>
#include <TMCLIMIT.H>

#ifndef _S_FILE_DEF
 #define _S_FILE_DEF

#ifndef _INPUT_DATA_DEF
 #include <Postpro.h>
#endif

#ifndef _OFFSET_DEF
 #define _OFFSET_DEF
typedef struct
         {
          _real frq;
//          int  lOffset;
          DWORD  lOffset;
          }_OFFSET;
#endif

typedef _OFFSET S_OFFSET;

#ifndef _S_HEADER_DEF
 #define _S_HEADER_DEF
typedef struct
         {
          WORD cPs;                 /* "pascal" string counter*/
          WORD cbSize;              /* offset table size*/
          WORD cnFrqNum;            /* frequency number*/
          }S_HEADER;
#endif

typedef struct
         {
          WORD           cbSize;              /* offset table size*/
          WORD           cnFrqNum;            /* frequency number*/
          S_OFFSET *lpOffset;           /* pointer to offset table*/
          }S_OFFSET_TABLE;

typedef struct
         {
          _complex *lpxC0;   /*pointer at function values*/
          _complex *lpxC1;   /*pointer at first  power spline coefficient*/
          _complex *lpxC2;   /*pointer at second power spline coefficient*/
          _complex *lpxC3;   /*pointer at third  power spline coefficient*/
         }INTERP_COEFF;

#define S_WR 0
#define S_RD 1
#define S_PS_NUMBER 7
#define OFF_TBL_MAIN 1
#define OFF_TBL_APPR 2
#define SMT_LABEL      "SMT"     /*scattering matrix file label*/
#define SMT_VERSION     102

#define S_UNKNOWN        -1
#define S_UNITARY         0
#define S_NOT_UNITARY     1
#define UNITARY_THRESH  (_real)1.0e-12

char * Get_S_file_name( void );
int  Set_S_file_name( void );
void make_S_matrix_name( char *S_filename, S_INDEX *si );
int  save_S_matrix( _complex *sss, INPUT_DATA *id,
                          char *S_filename, S_INDEX *si, int extrapol_flag );
int  read_S_matrix( char *filename, S_INDEX **si, int *extrapol_flag,
                          _complex ***sss, FREQUENCY *frq_out, INPUT_DATA **id);
int cut_S_matrix( _complex ***sss, FREQUENCY *frq, INPUT_DATA **id );
void set_interp_warning( int nWIFlag );
int  get_interp_warning( void );
int  read_S1_matrix( char *filename,  _complex **sss, INPUT_DATA *id, int *lpnUnit  );
int  copy_S_matrix( void );
void Free_GRID_Index( S_INDEX **si, int extrapol_flag );
void Free_S_read( _complex ***sss, int nFrqNum );
int  Interp_ODQCH_Cmplx( _real *lprX, _complex *lpxC0, _complex *lpxC1, _complex *lpxC2,
                                                          _complex *lpxC3, int nDim );
FILE * Get_S_FILE( void );
void   Set_PS_Number( int nNum );
int    Get_PS_Number( void );
int    Open_S_file( char *szFile, int nOpenMode );
void   Close_S_file( void );
int    update_offset_table( _real frq, int num, int nTBL_Flag );
int    skip_string( void );
int    write_S( const void *ptr, size_t size );
int    read_S( void *string, size_t size );
void   s_proc( char *name );

int read_S_matrix_element_HFSS( int *in1, int *mod1, int *in2, int *mod2, char *filename, _complex **sss, double **f1, int *nPoint);
int SMatrixElementHFSS_InpMod( int *in1, int *mod1, int *in2, int *mod2, char *filename );
int SMatrixElementHFSS_ModMax( char *filename, int *pnInp );
int SMatrixElementHFSS_InpMax( char *filename );
int ChooseString( char *pszBuf, const char *pszIdentifInput, const char *pszIdentifMode, int *pnInp, int *pnMod );
int SMatrixElementHFSS_FreqArray( int *in1, int *mod1, int *in2, int *mod2, char *filename, double **f1, _complex **sss, int *nPoint, int *nMatrixSize);
void SMatrixElementHFSS_FreqArrayCalculation( char *filename, double **f1, int *nPoint, int *nMatrixSize);
void SMatrixElementHFSS_FreqArrayAllocation( double **f1, _complex **sss, int *nPoint);
void SMatrixElementHFSS_FreqArrayRead( int *in1, int *mod1, int *in2, int *mod2, char *filename, double **f1, _complex **sss, int *nPoint, int *nMatrixSize);


int    put_S_matrix( _real *sss, INPUT_DATA *id,
                                   char *S_filename, S_INDEX *si, int extrapol_flag );

#endif
