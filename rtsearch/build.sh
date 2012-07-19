#!/bin/sh
make 
gcc -DSTATISTICS -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -g -o ./bin/testserver server.c ./common/mdb/mpool.c -I ./src/ -I./common/mdb/ -lznet -L /home/zhoubug/dev/svn_work/zevent/znet/bin/ -l pthread -lrt

gcc -DSTATISTICS -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -g -o ./bin/testclient client.c -I ./src/  -lznet -L /home/zhoubug/dev/svn_work/zevent/znet/bin/ -l pthread -lrt
