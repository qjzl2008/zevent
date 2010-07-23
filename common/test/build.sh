gcc -g -DHAS_THREADS testmpool.c ../memory/allocator.c ../locks/unix/thread_mutex.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testcond.c ../locks/unix/thread_cond.c ../locks/unix/thread_mutex.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testring.c ../locks/unix/thread_mutex.c ../locks/unix/thread_cond.c ../misc/reslist.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testreslist.c ../locks/unix/thread_mutex.c ../locks/unix/thread_cond.c ../misc/reslist.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testconnpool.c ../locks/unix/thread_mutex.c ../locks/unix/thread_cond.c ../misc/reslist.c ../pool/conn_pool.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testmysqlpool.c ../locks/unix/thread_mutex.c ../locks/unix/thread_cond.c ../misc/reslist.c ../pool/mysql_pool.c -I../include/ -I../include/arch/unix -I/usr/include/mysql -lpthread -L/usr/lib/mysql -lmysqlclient

gcc -g -DHAS_THREADS testqueue.c ../locks/unix/thread_mutex.c ../locks/unix/thread_cond.c ../misc/queue.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testrwlock.c ../locks/unix/thread_rwlock.c -I../include/ -I../include/arch/unix -lpthread

gcc -g -DHAS_THREADS testhash.c ../misc/hash.c -I../include/ -I../include/arch/unix 
