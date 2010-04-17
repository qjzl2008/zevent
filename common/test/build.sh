gcc -g -DHAS_THREADS testmpool.c ../memory/allocator.c ../locks/unix/thread_mutex.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testcond.c ../locks/unix/thread_cond.c ../locks/unix/thread_mutex.c -I../include/ -I../include/arch/unix -lpthread

