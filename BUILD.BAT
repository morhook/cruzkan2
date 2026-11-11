@echo off

call addpath

tcc -ml -Ic:\tc\include -Lc:\tc\lib CRUZBUG.C AUDIO.C
