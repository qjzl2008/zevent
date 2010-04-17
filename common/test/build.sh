gcc -g -DHAS_THREADS testmpool.c ../memory/allocator.c ../locks/unix/thread_mutex.c -I../include/ -I../include/arch/unix -lpthread

