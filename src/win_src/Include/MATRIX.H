/*
**  TAMIC_soft(R) 1992 vR1.02
**
**  Matrix functions declarations
**
**  FILE : mic\matrix.h
**
*/

#ifndef _MATRIX_H
 #define _MATRIX_H


int Invert_Sym_by_Gauss( _real *hpA, _real *lpAux, int *lpnAux, int nDim);
int Invert_cSym_by_Gauss( _complex *hpxA, _complex *lpxAux, int *lpnAux, int nDim);
int Invert_cdSym_by_Gauss( _real *hprA_r, _real *hprA_i, _complex *lpxAux, int *lpnAux, int nDim);
int Factor_Sym_by_Cholesky( _real *hpA, int nDim, double *Det );
int Solve_sym_factored( _real *lpV, int nDim, _real *hpLD );
void Change_Matrix_Sign( _real *hpMatrix, int nDim, int nStorage_flag );
void Change_cMatrix_Sign( _complex *lpxMatrix, int nDim, int nStorage_flag );
void Cut_the_Bolls( _complex *lpxMatrix, int nDim, int nStorage_flag );
void Add_cScalar_to_diag( _complex *lpxMatrix, _complex  xScalar, int nDim, int nStorage_flag );
int Copy_cMatrix_to_cBlock( _complex *lpxA, _complex *lpxB, int nDimA, int nDimB );
int Sub_cMatrix_from_cBlock( _complex *lpxA, _complex *lpxB, int nDimA, int nDimB );
void Conv_cGen_to_cSym( _complex *lpxA, _complex *lpxB, int nDim );
void Conv_cSym_to_cGen( _complex *lpxA, _complex *lpxB, int nDim );

int EigenVV_Real( _real *lprA, _real *lprB, _real *lprX, _real *lprXL, int nDim );
int EigenVV_of_Real_Sym( _real *lprA, _real *lprR, int nDim, int nWhat );

int Mul_cSym_by_cSym_Gen(_complex *lpxA, _complex *lpxB, _complex *lpxC, int nDim);
int Multiply_General( _real *lprA, _real *lprB, _real *lprC, int nN, int nM, int nL );
int Multiply_cGeneral( _complex *lpxA, _complex *lpxB, _complex *lpxC, int nN, int nM, int nL );
void Transpose_N_by_N( _real *lprA, int nN );
void Transpose_N_by_N_c( _complex *lpxA, int nN );
void Transpose_M_by_L_int( int *lpnA, int *lpnB, int nM, int nL );
int Mul_2Sym_Err_Norm(_real *hpA, _real *hpB, int nDim, double *Norm);
double Vector_Eucl_Norm( double *lpVector, WORD wLength, WORD wInc );
void INVGDC( _complex *A, int N, _complex *D, int *L, int *M );
void  Hilbert_matrix( _real *lpH, int nDim );
int Ext_Precision_Add( double dNumber, double *lpAcc);
int Ext_Precision_Mul( double dNum1, double dNum2, double *lpAcc);
double Get_Acc_Value( double *lpAcc );

int  Write_matrix(char *szTitle, _real *hpMatrix, int nDim, int nS_mode );
void Write_complex_matrix( char *szTitle, _complex *hpxMatrix, int nDim, int nS_mode, int nPart );
void Write_vector(char *szTitle, _real *Vector, WORD wLength, int nNum_flag);

int From_ZY_to_S( _complex *lpxS, _complex *lpxImmit, int nDim, int nMat_flag);
int From_S_to_Y( _complex *lpxS, _complex *lpxY, int nDim);
int Check_S_Unitary( _complex *lpxS, int nDim, _real *rMax );

#define MATRIX_REAL_MIN (1e-1)

#endif

