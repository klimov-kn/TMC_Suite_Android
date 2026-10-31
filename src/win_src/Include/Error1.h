#define FATAL_MEM_ALLOC_ERROR      0001

#define MEM_ALLOC_ERROR            1001

#define TMP_FILE_OPEN_ERROR        2001
#define TMP_FILE_WRITE_ERROR       2002
#define TMP_FILE_READ_ERROR        2003
#define TPL_FILE_OPEN_ERROR        2004
#define ERROR_OPEN_LISTING         2005
#define NO_NEED_SAVE               2006

#define OPEN_HELP_FILE_ERR         2100
#define BAD_HELP_FILE              2101
#define TO_MANY_STRING_HELP        2102
#define READ_DATA_FILE_ERR         2103
#define DATA_HELP_FILE_END         2104
#define HELP_PAGE_NOT_FOUND        2105
#define HELP_STACK_UNDERFLOW       2106
#define HELP_STACK_OVERFLOW        2107

                               /* TWP-file  error codes*/
#define ERROR_OPEN_TWP             2200
#define BAD_TWP_FILE               2201
#define TWP_FILE_NOT_OPEN          2210
#define BAD_TWP_FILE_VER           2211
#define BAD_TWP_FILE_MODEL         2221

#define ERROR_OPEN_Y               2202
#define BAD_Y_FILE_EXT             2203
#define BAD_Y_DELTA                2204
#define BAD_Y_FRQ_NUM              2205
#define BAD_Y_FRQ_VAL              2206
#define BAD_Y_FILE                 2207

             /* S-matrix file error codes */
#define ERROR_WRITE_S              2300
#define ERROR_READ_S               2301
#define ILLEGAL_CURVE_INDEX_CORR   2302
#define ILLEGAL_CURVE_INDEX        2303
#define BAD_S_FILE_NAME            2304
#define ERROR_OPEN_S               2305
#define SMT_LABEL_ERROR            2306
#define SMT_VERSION_ERROR          2307
#define SMT_FRQ_POINT              2308
#define SMT_FRQ_MEMORY             2309
#define INTERP_DIFF_MATRIX         2310
#define ERROR_WRITE_S_ITP          2311
#define INTER_ROOT_ERR             2312
#define SMT_INTER_TABLE            2313

                                /* expression error codes*/
#define ERROR_EXPR_OVERF_EXP       2401
#define ERROR_EXPR_ARG_LOG         2402
#define ERROR_EXPR_OVERF_SIN       2403
#define ERROR_EXPR_OVERF_COS       2404
#define ERROR_EXPR_OVERF_TAN       2405
#define ERROR_EXPR_OVERF           2406
#define ERROR_EXPR_ARG_SQRT        2407
#define ERROR_EXPR_OVERF_SINH      2408
#define ERROR_EXPR_OVERF_COSH      2409
#define ERROR_EXPR_OVERF_TANH      2410
#define ERROR_EXPR_ARG_ASIN        2411
#define ERROR_EXPR_ARG_ACOS        2412
#define ERROR_EXPR_OVERF1          2413
#define ERROR_EXPR_ARG_LOG10       2414
#define ERROR_EXPR_OVERF_X_Y       2415
#define ERROR_EXPR_FOR_X_Y         2416
#define ERROR_EXPR_BRACKET         2417
#define ERROR_EXPR_FUNCTION        2418
#define ERROR_EXPR_EXPR            2419
#define ERROR_EXPR_FLOAT_OVERF     2420

                                /* Preprocessor error codes*/
#define ERROR_PREPR_BUF_END        2501
#define ERROR_PREPR_ABN            2502
#define ERROR_PREPR_FACT_PARM      2505
#define ERROR_PREPR_MISSED_SEPAR   2506
#define ERROR_PREPR_OPEN_INC       2507
#define ERROR_PREPR_END_BUF_MEM    2501
#define ERROR_PREPR_MISSED_FORM_P  2509
#define ERROR_PREPR_REDEC          2510
#define ERROR_PREPR_INIT           2511
#define ERROR_PREPR_FOPEN_FILE     2512
#define ERROR_PREPR_LONG_STRING    2513
#define ERROR_PREPR_MIS_MAC_NAME   2514
#define PREPR_FILE_NAME_LONG       2515
#define ERROR_PREPR_MASKA_NUM      2516
#define ERROR_PREPR_FORMAL_NUM     2517

#define COMMAND_COM_ERROR          3001
#define DOS_ERROR_MESS             3002

#define BAD_DRIVE_ERR              4001
#define BAD_PATH_ERR               4002
#define PATH_TOO_LONG_ERR          4003
#define FILE_NAME_LONG_ERR         4004
#define FILE_NAME_EXIST_ERR        4005
#define END_OF_FL_ERR              4006
#define NO_PREV_FILE_ERR           4007

#define UNEXP_END_OF_FILE          5001
#define UNKNOWN_STRING_ERR         5002
#define BAD_BLOCK_DSCR_ERR         5003
#define UNEXP_EOF_NO_STEP          5004
#define MISSED_BLOCK_DESCR         5005
#define BAD_GROUP_DSCR_ERR         5006
#define ILLEGAL_NUM_OF_PARM        5007
#define ILLEGAL_INV_NUM            5008
#define TOO_MUCH_INP_TABL_INP      5009
#define ILLEGAL_INP_TABLE_INP      5010
#define ILLEGAL_INP_ADRESS         5011
#define ACUR_ILLEGAL_ORDER         5012
#define DUPLICATED_POINT           5013
#define MISSING_STEP_DSCR          5014
#define DUPLICATED_STEP_DSCR       5015
#define MISSING_MODEL_DSCR         5016
#define CONNECTED_INPUTS           5017


#define FRQ_BANDWIDTH_ERR          5101
#define BAD_F0_PARM                5102
#define BAD_DF_PARM                5103
#define BAD_NF_PARM                5104
#define BAD_ACURACY_PARM           5105
#define BAD_HEIGHT_PARM            5106
#define BAD_DELTA_PARM             5107
#define UNKNOWN_FRQ_UNIT           5108
#define UNKNOWN_LENGTH_UNIT        5109
#define ATTRIB_NAME_ERR            5110
#define ATTRIB_NOT_FOUND           5111
#define BAD_FREQUENCY              5112
#define EQUAL_FREQUENCIES          5113

#define OPEN_RGF_FILE              5400
#define WRITE_RGF_FILE             5401
#define RGF_READ_ERROR             5402
#define RGF_LABEL_ERROR            5403





#define TOO_MUCH_INP               6001
#define END_OF_DATA_LIST           6002
#define TOO_MUCH_GP                6003
#define ERR_ANS_MAXNUZ             6008
#define DATA_BASE_END              6010
#define LEFT_RIGHT_TYPE            6011

#define LINK_BLOCK_NOT_FOUND       6101
#define LINK_UNKNOWN_GP            6102
#define LINK_INPUT_NOT_FOUND       6103
#define LINK_INPUT_DEFERENCE       6104
#define LINK_INPUT_PARMS           6105
#define LINK_BAD_ORDER             6106
#define LINK_BLOCK_UNUSED          6107

#define INTERNAL_LINKER_ERR        6200

#define BAD_PRINT_FILE_NAME        6300
#define BAD_PRN_MARGIN             6301
#define BAD_EDITOR_FILE_NAME       6302


#define RUN_ANS_UNKNOWN_ERR        7001

#define LINE_NOT_FOUND             7501

#define POINT_DB_EMPTY             8001
#define POINT_DB_EXHAUSTED         8002
#define LINE_DB_EMPTY              8003
#define LINE_DB_EXHAUSTED          8004
#define POLYGON_DB_EMPTY           8005
#define POLYGON_DB_EXHAUSTED       8006
#define LINE_ATTR_DB_EMPTY         8007
#define LINE_ATTR_DB_EXHAUSTED     8008
#define LINE_P_LIST_DB_EMPTY       8009
#define LINE_P_LIST_DB_EXHAUSTED   8010

void SetErrorFileName( char *lpszFileName );
char * GetErrorFileName( void );

void SetErrorPathName( char *lpszPathName );
char * GetErrorPathName( void );

int  ErrorMessageMenu( char *menu_ptr[], struct messege_menu *msm_ptr );

#define put_error_message  put_error_messege

int put_error_messege( int err_num, ... );
int put_error_messege( const char *ch );



