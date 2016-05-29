time /t > do_lg4k.txt

x64\Debug\nvEncode2.exe ^
 C:\TEMP\lg_norway_4k.mp4 ^
 -useMappedResources ^
 -outfile=recode_lg4k.m4v ^
 -kbitrate=25000 ^
 -maxkbitrate=40000 ^
 -adaptiveTransformMode=0 ^
 -rcmode=0 ^
 -qpALL=24 ^
 -preset=6 ^
 -profile=100 ^
 -cabacenable ^
 -num_b_frames=2 ^
 -goplength=29 ^
 -maxNumRefFrames=2 ^

type do_lg4k.txt
time /t
time /t >> do_lg4k.txt