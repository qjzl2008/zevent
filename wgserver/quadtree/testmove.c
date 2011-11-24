#include <stdio.h>
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
        }

}

void dumpallnpc()
{
    quad_travel(map_tree->_root, printnpc_v3, NULL);
}

int addnpc(char *name, int id, int pos_x, int pos_y)
{
        quadbox_t pos_box = {pos_x, pos_y, pos_x + 2, pos_y + 2};
        npc *p = (npc *)malloc(sizeof(npc));
        if (!p)
                return (-1);

        p->id = id;
        p->pos_x = pos_x;
        p->pos_y = pos_y;
        strncpy(p->name, name, sizeof(p->name));
        p->name[sizeof(p->name)-1] = '\0';

        if (quadtree_insert(map_tree, &p->quad_lst, &pos_box) == NULL)
        {
                printf("add to map fail\n");
        }

        return (0);
}

void searchnpc()
{
    char name[32];
    int x, y, xmax, ymax;
    char c;
    struct list_head *quad_ret[100];
    int index = 0;
    quadbox_t box;
    printf("c x xmax y ymax: search by quad_tree\n");

    scanf("%d %d %d %d", &x, &xmax, &y, &ymax);
    box._xmin = x; box._xmax = xmax; box._ymin = y; box._ymax = ymax;
    quadtree_search(map_tree, &box, quad_ret, &index, 100);
    for (x = 0; x < index; ++x)
    {
	printnpc_v3(quad_ret[x], NULL);
    }
}

void addnpc_bytxt(char *name)
{
        char npcname[32];
        int x, y;
        FILE *f = fopen(name, "r");
        if (!name) {
                printf("can not open file %s\n", name);
                return;
        }
        while (fscanf(f, "%s %d %d", npcname, &x, &y) == 3) {
                addnpc(npcname, ++id_index, x, y);
        }
}

void usage()
{
        printf("i: add npc from txt\n");
        printf("a: add npc\n");
        printf("s: search npc\n");
        printf("q: exit\n");
        printf("d: dump all npc info\n");
        printf("m: move npc pos\n");
}

int main(int argc, char *argv[])
{
        char c;
        char name[32];
        int id;
        int pos_x, pos_y;

        quadbox_t mapbox = {0,0,100,100};
        map_tree = quadtree_create(mapbox, 5, 0.1);

        while (c = getchar()) {
                switch(c)
                {
                        case 'i':
                                printf("input txt = ?\n");
                                scanf("%s", name);
                                addnpc_bytxt(name);
                                break;
                        case 'a':
                                printf("name = ?, pos_x = ?, pos_y = ?\n");
                                scanf("%s %d %d", name, &pos_x, &pos_y);
                                addnpc(name, ++id_index, pos_x, pos_y);
                                break;
                        case 's':
                                searchnpc();
                                break;
                        case 'q':
                                printf("exit\n");
                                exit(0);
                        case 'd':
                                dumpallnpc();
                                break;
                        case 'm':
                                break;
                        case '\n':
                                break;
                        default:
                                usage();
                        break;
                }
        }
        return (0);
}
