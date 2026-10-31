#define stepV Freq__; File__; SFile__; Tmin__; Tmax__; @\\
#STEP\\
#PARM\\
 ANGLE_UNIT radian;\\
 FREQ_UNIT GHz;\\
 LONG_UNIT mm;\\
 TIME_UNIT ns;\\
 DELTA d;\\
 TIME 0.; Tmax__;\\
 X_MIN  0.0;\\
 X_MAX Lt;\\
 Y_MIN  0.0;\\
 Y_MAX W;\\
 FREQ  Freq__ ;\\
#END_PARM\\
#TOPOLOGY\\
 BLOCK 14;\\
  RECT_STAT EPS-1; L1; L1+L2; ylo; yhi;\\
 END_B\\
 BLOCK 1;\\
  RECT_STAT METAL; 0; Lt; ylo-d; ylo;\\
 END_B\\
 BLOCK 2;\\
  RECT_STAT METAL; 0; Lt; yhi; yhi+d;\\
 END_B\\
 BLOCK 7;\\
  INPUT_X ; ylo; yhi; 0; Tmax__;\\
 END_B\\
 BLOCK 12;\\
  INPUT_X ; ylo; yhi; Tmax__+100; Tmax__+100.1;\\
 END_B\\
#END_TOPOLOGY\\
#LINK_LIST\\
 T 14; 0.0; 0.0;\\
 T 1;  0.0; 0.0;\\
 T 2;  0.0; 0.0;\\
 T 7;  0.0; 0.0;\\
 T 12; Lt;  0.0;\\
#END_LINK\\
#OUTPUT\\
 FILE File__;\\
 SMATRIX SFile__; Freq__; Tmin__; Tmax__;\\
#END_OUTPUT\\
#END_STEP\\
