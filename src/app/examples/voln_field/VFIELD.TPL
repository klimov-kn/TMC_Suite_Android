#TMC_RT_H
! ---- VFIELD: VOLN na odnoy chastote s polem (FIELDS, TOPOLOGY) dlya ekrana Pole ----
! ---- Chastotnyy svip S-matricy po obrazcu rabochego TPL/TEST_EPS.TPL + STEP1.H ----
! ---- Aktiven PORT 1 (blok 7): ego okno vozbuzhdeniya [0;Tmax] vnutri TIME. ----
! ---- Port 2 (blok 12) VYKLYUCHEN: okno [Tmax+100;...] za predelami TIME. ----
! ---- Chtoby izmerit port 2 - pomenyat okna portov 7 i 12 mestami v VOLNSTEP.H. ----
#define L1  @ ( 60.0 )
#define L2  @ ( 40.0 )
#define L3  @ ( 60.0 )
#define W   @ ( 80.0 )
#define a   @ ( 60.0 )
#define d   @ (  1.0 )
#define EPS @ (  2.0 )
#define Lt  @ ( L1+L2+L3 )
#define ylo @ ( (W-a)/2 )
#define yhi @ ( (W+a)/2 )

#include VFIELD.H
stepV 5.0; vfield; vfield; 18; 20;

#EOF
