#TMC_RT_H
! ---- VOLN: volnovod s dielektricheskoy vstavkoy, 2 porta ----
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

#include VOLNSTEP.H
stepV 3.0; vs0; voln; 18; 20;
stepV 3.2; vs1; voln; 18; 20;
stepV 3.4; vs2; voln; 18; 20;
stepV 3.6; vs3; voln; 18; 20;
stepV 3.8; vs4; voln; 18; 20;
stepV 4.0; vs5; voln; 18; 20;
stepV 4.2; vs6; voln; 18; 20;
stepV 4.4; vs7; voln; 18; 20;
stepV 4.6; vs8; voln; 18; 20;
stepV 4.8; vs9; voln; 18; 20;
stepV 5.0; vs0; voln; 18; 20;
stepV 5.2; vs1; voln; 18; 20;
stepV 5.4; vs2; voln; 18; 20;
stepV 5.6; vs3; voln; 18; 20;
stepV 5.8; vs4; voln; 18; 20;
stepV 6.0; vs5; voln; 18; 20;
stepV 6.2; vs6; voln; 18; 20;
stepV 6.4; vs7; voln; 18; 20;
stepV 6.6; vs8; voln; 18; 20;
stepV 6.8; vs9; voln; 18; 20;
stepV 7.0; vs0; voln; 18; 20;

#EOF
