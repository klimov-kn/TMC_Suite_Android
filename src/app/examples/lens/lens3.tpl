#TMC_RT_H
! ---- LENS3: линза Люнеберга (ядро H) ----
! Три облучателя одновременно -- три плоских луча (многолучевая антенна).
! Линза Люнеберга превращает точечный источник на своей поверхности в плоскую волну,
! уходящую через центр линзы. Профиль eps = 2 - (r/R)^2, в файл -- добавка eps - 1.
! Частота 10 ГГц, радиус линзы 110 мм (3,7 длины волны), область открыта (поглотитель).
#STEP
#PARM
 ANGLE_UNIT radian;
 FREQ_UNIT  GHz;
 LONG_UNIT  mm;
 TIME_UNIT  ns;
 DELTA  1.0;
 TIME   0.; 3.0;
 X_MIN  0.0;
 X_MAX  560.0;
 Y_MIN  0.0;
 Y_MAX  480.0;
 FREQ   10.0;
#END_PARM
#TOPOLOGY
 BLOCK 1;   ! поглотитель справа
  RECT_STAT ABSORBER; 554.0; 559.0; 1.0; 479.0;
 END_B
 BLOCK 2;   ! поглотитель снизу
  RECT_STAT ABSORBER; 1.0; 559.0; 1.0; 6.0;
 END_B
 BLOCK 3;   ! поглотитель сверху
  RECT_STAT ABSORBER; 1.0; 559.0; 474.0; 479.0;
 END_B
 BLOCK 6;   ! поглотитель слева
  RECT_STAT ABSORBER; 1.0; 6.0; 1.0; 479.0;
 END_B
 BLOCK 4;   ! линза
  CIRCLE_STAT (2-(r/110.0)*(r/110.0))-1; 0.0; 110.0; 0.0; 6.28;
 END_B
 BLOCK 10;   ! облучатель на краю линзы, +0 градусов
  INPUT_X MAGNETIC; 235.0; 245.0; 0; 1000; 1; 0;
 END_B
 BLOCK 11;   ! облучатель на краю линзы, +30 градусов
  INPUT_X MAGNETIC; 290.0; 300.0; 0; 1000; 1; 0;
 END_B
 BLOCK 12;   ! облучатель на краю линзы, -30 градусов
  INPUT_X MAGNETIC; 180.0; 190.0; 0; 1000; 1; 0;
 END_B
#END_TOPOLOGY
#LINK_LIST
 T 1; 0.0; 0.0;
 T 2; 0.0; 0.0;
 T 3; 0.0; 0.0;
 T 6; 0.0; 0.0;
 T 4; 190.0; 240.0;
 T 10; 78.0; 0.0;
 T 11; 92.7; 0.0;
 T 12; 92.7; 0.0;
#END_LINK
#OUTPUT
 FILE lens3;
 FIELDS;
 TOPOLOGY;
#END_OUTPUT
#END_STEP
#EOF
