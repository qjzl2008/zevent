#include "ring.h"

typedef struct res_t  res_t ;
struct res_t
{
	 int a;
	 void * res;
	 RING_ENTRY(res_t) link;
};


RING_HEAD(res_ring_t,res_t);
typedef struct res_ring_t res_ring_t;


int main(void)
{
	res_ring_t avail_list;
	res_t res;

	RING_INIT(&avail_list,res_t,link);
	RING_INSERT_HEAD( &avail_list,  &res, res_t , link );
	return 0;
}
