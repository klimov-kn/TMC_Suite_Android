#TMC_RT_H
! ---- XR_OFF: рупор и X-образная решётка плазменных трубок (ядро X) ----
! Оба ряда выключены -- волна должна пройти прямо.
! Частота 10 ГГц (lambda = 30 мм). Трубки -- газоразрядные цилиндры диаметром 10 мм,
! шаг 13 мм вдоль ряда. Включённая трубка: концентрация 2e19 м^-3, плазменная частота
! ~40 ГГц > 10 ГГц -- eps = 1 - (fp/f)^2 < 0, трубка отражает как металл.
! Выключенная трубка: N = 0 -- воздух. Ряды наклонены на 45 градусов:
! ряд «вверх» поворачивает волну на 90 градусов вверх, ряд «вниз» -- вниз.
#define L_box   @ ( 340.0 )
#define W_box   @ ( 320.0 )
#define W_delta @ (   1.0 )
#define W_chan  @ (  22.0 )
#define L_feed  @ (  40.0 )
#define L_flare @ (  50.0 )
#define D_aper  @ (  90.0 )
#define F       @ (  10.0 )
#define TM      @ (   2.4 )
#define yc      @ ( W_box/2 )
#define ylo     @ ( yc - W_chan/2 )
#define yhi     @ ( yc + W_chan/2 )

#STEP
#PARM
 ANGLE_UNIT radian;
 FREQ_UNIT  GHz;
 LONG_UNIT  mm;
 TIME_UNIT  ns;
 DELTA  W_delta;
 TIME   0.; TM;
 X_MIN  0.0;
 X_MAX  L_box;
 Y_MIN  0.0;
 Y_MAX  W_box;
 FREQ   F;
#END_PARM

#TOPOLOGY
 BLOCK 1;
  RECT_STAT METAL; 0.0; L_feed; ylo-W_delta; ylo;
 END_B
 BLOCK 2;
  RECT_STAT METAL; 0.0; L_feed; yhi; yhi+W_delta;
 END_B
 BLOCK 3;
  POLYGON_STAT METAL;
   L L_feed;          ylo;
   L L_feed+L_flare;  yc-D_aper/2;
   L L_feed+L_flare;  yc-D_aper/2;
 END_B
 BLOCK 4;
  POLYGON_STAT METAL;
   L L_feed;          yhi;
   L L_feed+L_flare;  yc+D_aper/2;
   L L_feed+L_flare;  yc+D_aper/2;
 END_B
 BLOCK 5;
  INPUT_X ; ylo; yhi; 0; 1000;
 END_B
! ---- рамка-поглотитель: открытое пространство справа, снизу и сверху ----
 BLOCK 6;
  RECT_STAT ABSORBER; L_box-5*W_delta; L_box-2*W_delta; 2*W_delta; W_box-2*W_delta;
 END_B
 BLOCK 7;
  RECT_STAT ABSORBER; 2*W_delta; L_box-2*W_delta; 2*W_delta; 5*W_delta;
 END_B
 BLOCK 8;
  RECT_STAT ABSORBER; 2*W_delta; L_box-2*W_delta; W_box-5*W_delta; W_box-2*W_delta;
 END_B
! ---- плазменные трубки ----
 BLOCK 20;                                   ! центральная трубка (общая для обоих рядов)
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 21;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 22;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 23;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 24;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 25;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 26;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 27;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 28;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 29;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 30;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 31;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 32;                                   ! ряд «вверх»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 33;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 34;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 35;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 36;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 37;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 38;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 39;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 40;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 41;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 42;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 43;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
 BLOCK 44;                                   ! ряд «вниз»
  CIRCLE_STAT_N 0.0; 0.0; 5.0; 0.0; 6.28;
 END_B
#END_TOPOLOGY

#LINK_LIST
 T 1; 0.0; 0.0;
 T 2; 0.0; 0.0;
 T 3; 0.0; 0.0;
 T 4; 0.0; 0.0;
 T 5; 0.0; 0.0;
 T 6; 0.0; 0.0;
 T 7; 0.0; 0.0;
 T 8; 0.0; 0.0;
 T 20; 230.0000; 160.0000;
 T 21; 220.8076; 150.8076;
 T 22; 239.1924; 169.1924;
 T 23; 211.6152; 141.6152;
 T 24; 248.3848; 178.3848;
 T 25; 202.4228; 132.4228;
 T 26; 257.5772; 187.5772;
 T 27; 193.2304; 123.2304;
 T 28; 266.7696; 196.7696;
 T 29; 184.0381; 114.0381;
 T 30; 275.9619; 205.9619;
 T 31; 174.8457; 104.8457;
 T 32; 285.1543; 215.1543;
 T 33; 220.8076; 169.1924;
 T 34; 239.1924; 150.8076;
 T 35; 211.6152; 178.3848;
 T 36; 248.3848; 141.6152;
 T 37; 202.4228; 187.5772;
 T 38; 257.5772; 132.4228;
 T 39; 193.2304; 196.7696;
 T 40; 266.7696; 123.2304;
 T 41; 184.0381; 205.9619;
 T 42; 275.9619; 114.0381;
 T 43; 174.8457; 215.1543;
 T 44; 285.1543; 104.8457;
#END_LINK

#OUTPUT
 FILE xr_off;
 FIELDS;
 TOPOLOGY;
#END_OUTPUT
#END_STEP
#EOF
