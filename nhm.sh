#!/bin/sh

PATH=$PATH:../iaca/bin
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../iaca/lib

iaca -64 -arch NHM build/opt/libhandmade.so | less -M
