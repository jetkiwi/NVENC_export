time /t > do_king.txt

x64\Debug\nvEncode2.exe ^
 D:\shared\bd_lk_rip\cantwait2beking.m2ts ^
 -useMappedResources ^
 -outfile=king_adapt5.264 ^
 -darwidth=16 ^
 -darheight=9 ^
 -kbitrate=5000 ^
 -maxkbitrate=16000 ^
 -adaptiveTransformMode=0 ^
 -rcmode=1 ^
 -preset=6 ^
 -profile=100 ^
 -cabacenable ^
 -num_b_frames=2 ^
 -goplength=24 ^
 -maxNumRefFrames=4 ^

type do_king.txt
time /t
time /t >> do_king.txt

% -rcmode=1 ^
