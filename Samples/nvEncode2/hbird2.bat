time /t > hbird2.txt

x64\Debug\nvEncode2.exe ^
 C:\temp\hbird_00002.mts ^
 -useMappedResources ^
 -fieldmode=2 ^
 -outfile=recode_hbird2.264 ^
 -darwidth=16 ^
 -darheight=9 ^
 -kbitrate=10000 ^
 -maxkbitrate=16000 ^
 -adaptiveTransformMode=0 ^
 -rcmode=1 ^
 -preset=4 ^
 -profile=100 ^
 -cabacenable ^
 -num_b_frames=2 ^
 -goplength=15 ^
 -maxNumRefFrames=2 ^

type hbird2.txt
time /t
time /t >> hbird2.txt

% -rcmode=1 ^
%  king_adapt5.gpu0.ts ^
% C:\temp\hbird_00002.mts ^
% king_adapt5.gpu0.ts ^
%