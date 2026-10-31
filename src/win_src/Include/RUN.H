/*
**  TAMIC_soft(R) 1990,1991 v1.20
**
** Macros and decloration for run  programms.
**
** FILE: run.h
*/

#ifndef _RUN_H
 #define _RUN_H

 #ifndef _ELEM_VAL
  #ifdef _ELEM_COMPLEX
   #define _ELEM_VAL _complex
  #else
   #define _ELEM_VAL _real
  #endif
 #endif

#ifndef _INP_ADRESS
 #define _INP_ADRESS

 typedef struct
         {
          short block;
          short input;
          } INP_ADRESS;

#endif

#ifndef _Y_MATRIX_DEF
 #define _Y_MATRIX_DEF

typedef struct
        {
         short        num;                                       /* number of input*/
         short        norm_ptr[MAX_INPUT_NUM];                   /* number of mode in input*/
         _real      norm_array[MAX_INPUT_NUM][MAX_INPUT_NUM];  /* norm. coeff.*/
         short        flag[MAX_INPUT_NUM];                       /* +1 - no norm., -1 yes norm.*/
         INP_ADRESS ia[MAX_INPUT_NUM];                         /* input adress*/
         } INPUT;

typedef struct
         {
          _ELEM_VAL *matr;               /* pointer to Y-matrix buffer*/
//          float       _far *matr;               /* pointer to Y-matrix buffer*/
          short             norm_flag;          /* norm_flag: 0 - norm. not available*/
                                              /*            1 - available but not norm*/
                                              /*            2 - norm*/
          INPUT           inp;                /* input data*/
          } Y_MATRIX;
#endif
#include <LINKLIST.H>
struct point_trn_adress
         {
          float *skx,   *sky;
          int   *ktpgr, *nomuz, maxnuz;
          int   koltr,       *nadr1, *nadr2, *nadr3;
          float *epstr, *rmutr;
          };

typedef struct
          { _real omga;       /* 2.0*PI*F(i)*/
            _real pi;         /* PI = 3.14....*/
            _real um0;        /* mu0 constant*/
            _real eps0;       /* eps0 constant*/
           }CONST_BLOCK;

#ifndef _MODUL_GP
 #define _MODUL_GP

struct modul_GP
	{
         int                   code;        /* entity's code*/
         int                   num;         /* block number*/
         void            *gp;          /* pointer to this entity*/
         struct modul_GP *next;        /* pointer to next entity*/
	 };
typedef struct modul_GP MODUL_GP;
#endif

int  run_step( void );
int  curnel( Y_MATRIX *matr );
int  step_out( Y_MATRIX *matr );

int  modul( int modul_num, Y_MATRIX *matr, MODUL_GP * ptr );
int  modul_inp( MODUL_GP *gp_ptr, float *matr, INPUT *inp );
int  modul_wvgd( MODUL_GP *gp_ptr, Y_MATRIX *matr );
int  modul_opstb( MODUL_GP *gp_ptr, float *matr, INPUT *inp );
int  modul_file( MODUL_GP *gp_ptr, Y_MATRIX *matr );
int  trn_modul_fe( struct modul_GP *gp_ptr );
int  fill_data_trn_fe( struct modul_GP *gp,
                           float *skx, float *sky, float *epsgr, float *rmugr,
                           int *nsbsp, int *ktpgr, int *inzsp,
                           int *ngp, int *msinsp, float *epsrg, float *rmurg, int ngp_max,
                           int *kolin, int *numod, int in_max);
void get_input_length( int ngp, float *skx, float *sky, int *nsbsp,
                           int *ktpgr, int *msinsp, float *rinlong);
void get_input_type( int ngp, int *nsbsp, int *ktpgr, int *msinsp,
                         int *inp_type);
int  init_list( int *nsbsp, int list_size);
int  ansmbl_modul_fe( float *matr, INPUT *inp );
void Trangl( int *ngp, float *skx, float *sky, int *nsbsp,
                         int *ktpgr, int *nomuz, float *epsgr, float *rmugr, int *nd1,
                         int *inzsp, int *msinsp, float *epsrg, float *rmurg, int *nd2,
                         float *delta, int *nstep,
                         int *koltr, int *nadr1, int *nadr2, int *nadr3,
                         float *epstr, float *rmutr, int *nd3,
                         int *koluzl, int *maxnuz, int *ifail);
void set_trn_data_file_name( int block_num );
int  write_trn( float *skx, float *sky, int *nomuz, int *ktpgr, int maxnuz,
                    int *nadr1, int *nadr2, int *nadr3, float *epstr, float *rmutr, int koltr,
                    int *numod, int kolin, float *rinlng, int *inp_type );
int  read_trn( struct point_trn_adress *pta, int *kolin, int *numod,
                   int *inp_type, float *rinlng);
void GnAnsm( float *skx, float *sky, int *nomuz, int *ktpgr, int *nd1,
                         int *na1, int *na2, int *na3, float *epstr, float *rmutr, int *nd2, int *koltr,
                         float *sa, int *nsbspr, int *nsbspl, int *indxsr, int *indxsl, int *nd4,
                         float *sad, int *nadrsr, int *nadrsl, int *minst, int *nd5, int *ndim,
                         int *inzsp, int *ret_code);
void FEMat( float *sa, float *sad, int *nd4, int *nd5,
                        int *nsbspr, int *nsbspl, int *nadrsr, int *nadrsl,
                        int *indxsr, int *indxsl, int *inzsp,
                        int *minst, int *ndim,
                        float *avektr, int *nvektr, int *nd6,
                        int *kuzbx, int *numod, int *inp_type, float *rinlng, int *nd3,
                        float *szg, int *nd7, int *nd9, INPUT *inp,
                        float *delta, float *height, int *ret_code);

int  neladr( int *out_dim, int *i, int *j );



/*             SUBROUTINE TRNSK(AC, B, X1, X2, NL, NMDL, NMAX, NMOD, NMOD1,
                                UM, E, SZG, ND2, NSBX, NUZL, PN, ND4,
                                SZGLM1, SZGLM2, xin, XIN1, nd6, nx1, nx2, nx12, keyl,
                                scob, nd7, KEY1, KEY2, KEY3, KEY5, KEY6, ifail) */
void TRNSK( float *AC, float *B, float *X1, float *X2,
                        int *NL, int *NMDL, int *NMAX, int *NMOD, int *NMOD1,
                        float *UM, float *E, float *SZG, int *ND2,
                        int *NSBX, int *NUZL, float *nrmrk, int *ND4,
                        double *SZGLM1, double *SZGLM2, double *xin, double *XIN1, int *nd6,
                        int *nx1, int *nx2, int *nx12, int *keyl,
                        float *scob, int *nd7,
                        int *KEY1, int *KEY2, int *KEY3, int *KEY5, int *KEY6, int *ifail);
/*          SUBROUTINE MODNK(AC, B, X1, X2, X3, X4, ND1, NL, NPR, NMDL, NMDP, NMAX,
                             UM, E, SZG, ND2, NSBX, NUZL, ND3, szglm1, szglm2, scob1, nd4,
                             scob2, nd5, nx1, nx2, nx3, nx4, nx12, nx34, keyl, keyp,
                             KEY1, key2, KEY3, KEY5, KEY6, ifail) */
void MODNK(float *AC, float *B, float *X1, float *X2, float *X3, float *X4,
                       int *ND1, int *NL, int *NPR, int *NMDL, int *NMDP, int *NMAX,
                       float *UM, float *E, float *SZG, int *ND2,
                       int *NSBX, int *NUZL, int *ND3,
                       double *szglm1, double *szglm2, float *scob1, int *nd4,
                       float *scob2, int *nd5,
                       int *nx1, int *nx2, int *nx3, int *nx4, int *nx12, int *nx34,
                       int *keyl, int *keyp, int *KEY1, int *key2, int *KEY3, int *KEY5,
                       int *KEY6, int *ifail);
void STRGG1( float *matr1, int *dim1, int *in2,
                         float *matr2, int *dim2, int *_in2,
                         int *dim3, int *_in3, int *_out3,
                         int *KEY1, float *fbuff1, float *fbuff2, int *nd1, int *errcode);
void DEARNG( float *matr1, float *matr2, int *_in3, int *_out3, int *flag);

int  reduction_1( Y_MATRIX *matr, LINK_LIST *ll_ptr );
int  reduction_2( Y_MATRIX *matr_1, Y_MATRIX *matr_2, LINK_LIST **ll_ptr );

int  save_Y_matrix( Y_MATRIX *ym );
int  read_Y_matrix( Y_MATRIX *ym );
int  replace_up( float *matr, int n1, int n2, int n3 );
int  replace_matrix_up( Y_MATRIX *matr, int *nzam, INPUT *inp_buff, int inp_num );
int  replace_doun( float *matr, int n1, int n2, int n3 );
int  replace_matrix_doun( Y_MATRIX *matr, int *nzam, INPUT *inp_buff, int inp_num );

int  oder_input( Y_MATRIX *matr );


void set_filter_flag( int flag );
int  get_filter_flag( void );

#endif
