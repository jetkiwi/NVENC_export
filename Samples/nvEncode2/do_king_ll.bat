time /t > do_king.txt

x64\Debug\nvEncode2.exe ^
 D:\shared\bd_lk_rip\cantwait2beking.m2ts ^
 -useMappedResources ^
 -outfile=king_ll_pre8.264 ^
 -kbitrate=60000 ^
 -maxkbitrate=150000 ^
 -adaptiveTransformMode=0 ^
 -rcmode=0 ^
 -qpALL=0 ^
 -preset=8 ^
 -profile=244 ^
 -num_b_frames=0 ^
 -goplength=24 ^
 -level_idc=51 ^
 -cabacenable ^
 -maxNumRefFrames=2 ^
 -enableAQ ^
 -qpPrimeYZeroTransformBypassFlag ^


%%% -enableVFR ^
%%%%%-qpPrimeYZeroTransformBypassFlag ^
%%% -enableAQ ^
%%% -darwidth=16 ^
%%% -darheight=9 ^

type do_king.txt
time /t
time /t >> do_king.txt

% -rcmode=1 ^
