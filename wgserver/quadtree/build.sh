gcc -shared -fpic -o libquadtree.so quadtree.c -I./
gcc -g -o testquadtree bench.c quadtree.c -I./
