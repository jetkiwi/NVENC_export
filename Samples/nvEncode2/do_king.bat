time /t > do_king.txt

x64\Debug\nvEncode2.exe ^
 D:\shared\bd_lk_rip\cantwait2beking.m2ts ^
 -useMappedResources ^
 -outfile=king_3500_qp25_pre4.264 ^
 -darwidth=16 ^
 -darheight=9 ^
 -kbitrate=3500 ^
 -maxkbitrate=25000 ^
 -adaptiveTransformMode=0 ^
 -qpALL=25 ^
 -rcmode=8 ^
 -preset=4 ^
 -level=40 ^
 -profile=100 ^
 -cabacenable ^
 -num_b_frames=2 ^
 -enableAQ ^
 -goplength=24 ^
 -maxNumRefFrames=2 ^

type do_king.txt
time /t
time /t >> do_king.txt

% -rcmode=1 ^
