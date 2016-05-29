%
% NVENC transcode example
% -----------------------
% Input file: 4k H264 (3840x2160 24.00fps YUV4:2:0)
%
% Output file: HEVC  (-codec=5)
%
% Encode rate_control: VBR (1-pass)
%    Even though the rate-control mode is set to VBR,
% we still need to enable the min_QP and max_QP control flags.
% Otherwise, NVENC seems to ignore our rate_control parameter.
% 
%   As of Jan 2015:
%      NVidia NVENC HEVC encoder has the following restrictions:
%        no BFrames (bframes must be set to 0)
%        no VUI block inserted
%        no user SEI insertion
%        maxCUsize = up to 32x32 (documentation says 64x64 is not supported)

x64\Debug\nvEncode2.exe ^
 D:\\shared\\4kvideo\\Interstellar-TLR_1b-5.1ch-4K-HDTN.mp4 ^
 -useMappedResources ^
 -outfile=stellar_265_vbr15.265 ^
 -codec=5 ^
 -min_qpALL=0 ^
 -max_qpALL=51 ^
 -rcmode=32 ^
 -kbitrate=15000 ^
 -maxkbitrate=80000 ^
 -preset=4 ^
 -num_b_frames=0 ^
 -goplength=24 ^
 -sliceMode=3 ^
 -sliceModeData=4 ^
 -maxNumRefFrames=4 ^
 -enableAQ


% -profile=300 ^
% -enableVFR ^
% -rcmode=1 ^
% -qpALL=25 ^
% -min_qpALL=21 ^
% -max_qpALL=23 ^
% -numerator=24 ^
% -denominator=1 ^
%