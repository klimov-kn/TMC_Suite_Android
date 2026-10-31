/*
**  TAMIC_soft(R) 1991 v1.00
**
**  run_flag.h
**
**  FILE : run_flag.c
**
**  LIB  :
**
**  OVERLAY:
**
*/

#ifndef _RUN_FLAG_H
 #define _RUN_FLAG_H

struct run_flags
           {
            int load;
            int topology;
            int kernel;
            int out_proc;
            };

#define FACTOR     0
#define INVERT     1

void set_tpl_step( int step );
int  get_tpl_step( void );
int  decr_tpl_step( void );

void set_all_flag( int flag );
int  get_all_flag( void );

void set_need_rf_load_flag( int flag );
int  get_need_rf_load_flag( void );

void set_need_rf_topology_flag( int flag );
int  get_need_rf_topology_flag( void );

void set_need_rf_kernel_flag( int flag );
int  get_need_rf_kernel_flag( void );

void set_need_rf_out_proc_flag( int flag );
int  get_need_rf_out_proc_flag( void );

void set_able_rf_load_flag( int flag );
int  get_able_rf_load_flag( void );

void set_able_rf_topology_flag( int flag );
int  get_able_rf_topology_flag( void );

void set_able_rf_kernel_flag( int flag );
int  get_able_rf_kernel_flag( void );

void set_able_rf_out_proc_flag( int flag );
int  get_able_rf_out_proc_flag( void );

void set_auto_run_flag( int flag );
int  get_auto_run_data_flag( void );

void Set_Sound_effects_flag( int flag );
int  Get_Sound_effects_flag( void );

void Set_sol_system_flag( int flag );
int  Get_sol_system_flag( void );

void set_accepted_data_flag( int flag );
int  get_accepted_data_flag( void );

void Set_TRACE_mode( int mode );
int  Get_TRACE_mode( void );

void Set_WARNING_mode( int mode );
int  Get_WARNING_mode( void );

#endif
