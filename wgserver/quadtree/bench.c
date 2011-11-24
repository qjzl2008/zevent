#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "quadtree.h"

typedef struct npc_
{
        struct list_head quad_lst;
        int pos_x, pos_y;
        char name[32];
        int id;
} npc;

static quadtree_t *map_tree;
static int id_index;

void printnpc(npc *t, void *param)
{
        if (!t)
                return;
        printf("name[%s] id[%d] x[%d] y[%d]\n",
                ((npc *)t)->name, ((npc *)t)->id, ((npc *)t)->pos_x, ((npc *)t)->pos_y);
}

static int count = 0;
void printnpc_v3(struct list_head *l, void *param)
{
        struct list_head *n = l->next;
        npc *data;
        if (!l)
                return;
        while (n != l) {
                data = (npc *)((char *)n - offsetof(npc,quad_lst) );
                printnpc(data, NULL);
                n = n->next;
		++count;
        }

}

void dumpallnpc()
{
    quad_travel(map_tree->_root, printnpc_v3, NULL);
}

int addnpc(char *name, int id, int pos_x, int pos_y)
{
        quadbox_t pos_box = {pos_x, pos_y, pos_x + 32, pos_y + 32};
        npc *p = (npc *)malloc(sizeof(npc));
        if (!p)
                return (-1);

        p->id = id;
        p->pos_x = pos_x;
        p->pos_y = pos_y;
        strncpy(p->name, name, sizeof(p->name));
        p->name[sizeof(p->name)-1] = '\0';

/*	printf("add new npc name:%s,x_min:%f,y_min:%f,x_max:%f,y_max:%f\n",
		p->name,
		pos_box._xmin,pos_box._ymin,
		pos_box._xmax,pos_box._ymax);
*/
        if (quadtree_insert(map_tree, &p->quad_lst, &pos_box) == NULL)
        {
                printf("add to map fail\n");
        }

        return (0);
}

void searchnpc(double min_x,double max_x,double min_y,double max_y)
{
    char name[32];
    struct list_head *quad_ret[1000];
    int index = 0;
    quadbox_t box;
    printf("min_x:%f max_x:%f min_y:%f max_y:%f: search by quad_tree\n",
	    min_x,max_x,min_y,max_y);

    box._xmin = min_x; box._xmax = max_x; box._ymin = min_y; box._ymax = max_y;
    quadtree_search(map_tree, &box, quad_ret, &index, 1000);
    printf("index:%d\n",index);
    int x = 0;
    for (x = 0; x < index; ++x)
    {
	printnpc_v3(quad_ret[x], NULL);
    }
}

void addmultinpc()
{
        char npcname[32];
        int x=0, y=0;
	int count = 0;
	for(count = 0; count < 5000; ++count) {
	    x = rand()%400;
	    y = rand()%600;
	    sprintf(npcname,"%s-%d","zhoubug",count);
	    addnpc(npcname,count,x,y);
	}
}


int main(int argc, char *argv[])
{
        char c;
        char name[32];
        int id;
        int pos_x, pos_y;

	srand(time(NULL));

        quadbox_t mapbox = {0.0,0.0,5000.0,5000.0};
        map_tree = quadtree_create(mapbox, 5, 0.1);
	addmultinpc();
        searchnpc(0.0,400.0,0.0,600.0);
	printf("total in region:%d\n",count);

        return (0);
}
