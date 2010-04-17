gcc -g -DHAS_THREADS testmpool.c allocator.c ./locks/unix/thread_mutex.c -I./include/ -I./include/arch/unix -lpthread

