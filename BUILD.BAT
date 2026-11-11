@echo off

call addpath

tcc -ml -Ic:\tc\include -Lc:\tc\lib CRUZKAN2.C BUG.C AUDIO.C
