#TMC_RT_H
! ---- EPS4: волновод 60 мм с длинной диэлектрической вставкой eps = 4 (поле дл€ проверки) ----
! „астота 5 √√ц, критическа€ частота волны H10 -- 2,5 √√ц.
! ƒлина волны в волноводе: в воздухе 60/sqrt(1-0.25) = 69 мм,
! во вставке 60/sqrt(4-0.25) = 31 мм -- волна во вставке в 2,2 раза короче и медленнее.
#define W   @ (  60.0 )
#define L   @ ( 420.0 )
#define x1  @ ( 160.0 )
#define x2  @ ( 320.0 )
#define d   @ (   1.0 )
#define EPS @ (   4.0 )
#define F   @ (   5.0 )
#define TM  @ (   6.0 )

#STEP
#PARM
 ANGLE_UNIT radian;
 FREQ_UNIT  GHz;
 LONG_UNIT  mm;
 TIME_UNIT  ns;
 DELTA d;
 TIME 0.; TM;
 X_MIN 0.0;
 X_MAX L;
 Y_MIN 0.0;
 Y_MAX W+2*d;
 FREQ F;
#END_PARM

#TOPOLOGY
 BLOCK 14;
  RECT_STAT EPS-1; x1; x2; d; W+d;
 END_B
 BLOCK 1;
  RECT_STAT METAL; 0; L; 0; d;
 END_B
 BLOCK 2;
  RECT_STAT METAL; 0; L; W+d; W+2*d;
 END_B
 BLOCK 7;
  INPUT_X ; d; W+d; 0; TM;
 END_B
 BLOCK 12;
  INPUT_X ; d; W+d; TM+100; TM+100.1;
 END_B
#END_TOPOLOGY

#LINK_LIST
 T 14; 0.0; 0.0;
 T 1;  0.0; 0.0;
 T 2;  0.0; 0.0;
 T 7;  0.0; 0.0;
 T 12; L;   0.0;
#END_LINK

#OUTPUT
 FILE eps4;
 FIELDS;
 TOPOLOGY;
#END_OUTPUT
#END_STEP
#EOF
