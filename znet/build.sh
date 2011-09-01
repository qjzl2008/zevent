#!/bin/sh
make 
gcc -g -o ./bin/testserver main.c -I ./src/  -lznet -L /home/zhoubug/dev/svn_work/zevent/znet/bin/ -l pthread -lrt

