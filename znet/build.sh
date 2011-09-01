#!/bin/sh
make 
gcc -DSTATISTICS -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -g -o ./bin/testserver main.c ./common/mdb/mpool.c -I ./src/ -I./common/mdb/ -lznet -L /home/zhoubug/dev/svn_work/zevent/znet/bin/ -l pthread -lrt

