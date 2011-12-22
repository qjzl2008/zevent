/*
 * quadtree.c
 *        Quad tree implementation -- for spatial quick searching
 *        cheungmine
 */
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include "quadtree.h"

/*=============================================================================
  Private Functions
  =============================================================================*/
static void printbox(quadnode_t *node)
{
   printf("box:minx:%f,miny:%f,maxx:%f,maxy:%f\n",node->_box._xmin,
	    node->_box._ymin,node->_box._xmax,node->_box._ymax);
}


static int quadbox_is_valid (quadbox_t    *qb)
{
    return (qb->_xmin < qb->_xmax && qb->_ymin < qb->_ymax)? 1 : 0;
}

static double quadbox_width (const quadbox_t* qb)
{
    return (qb->_xmax - qb->_xmin);
}

static double quadbox_height (const quadbox_t* qb)
{
    return (qb->_ymax - qb->_ymin);
}

static void quadbox_init (quadbox_t    *qb,
        double xmin, double ymin, double xmax, double ymax)
{
    qb->_xmin = xmin; qb->_ymin = ymin; qb->_xmax = xmax; qb->_ymax = ymax;
    assert (quadbox_is_valid (qb));
}

static void quadbox_inflate(quadbox_t* qb, double dx, double dy)
{
    assert (dx > 0 && dy > 0);
    qb->_xmin -= (dx/2);
    qb->_xmax += (dx/2);
    qb->_ymin -= (dy/2);
    qb->_ymax += (dy/2);
}

/* splits the quadrant such as below:
   nw(0010)  |  ne(0001)
   ----------|----------
   sw(0100)  |  se(1000)
*/
static void quadbox_split(const quadbox_t* qb,
        quadbox_t* ne, quadbox_t* nw, quadbox_t* se, quadbox_t* sw,
        float overlap)
{
    double dx = quadbox_width(qb) * (1.0 + overlap)/2;
    double dy = quadbox_height(qb) * (1.0 + overlap)/2;

    assert (overlap >= QBOX_OVERLAP_MIN-0.0001);
    assert (overlap <= QBOX_OVERLAP_MAX+0.0001);


    quadbox_init (ne, qb->_xmax-dx, qb->_ymax-dy, qb->_xmax,    qb->_ymax);
    quadbox_init (nw, qb->_xmin,    qb->_ymax-dy, qb->_xmin+dx, qb->_ymax);
    quadbox_init (sw, qb->_xmin,    qb->_ymin,    qb->_xmin+dx, qb->_ymin+dy);
    quadbox_init (se, qb->_xmax-dx, qb->_ymin,    qb->_xmax,    qb->_ymin+dy);
    
}

/* returns TRUE if the first is inside the senond */
static int quadbox_is_inside(const quadbox_t* _first, const quadbox_t* _second)
{
    return (_second->_xmin < _first->_xmin && _second->_xmax > _first->_xmax &&
        _second->_ymin < _first->_ymin && _second->_ymax > _first->_ymax)? 1 : 0;
}

/* returns TRUE if two quad_box is overlapped */
static int quadbox_is_overlapped(const quadbox_t* _first, const quadbox_t* _second)
{
    return (_first->_xmin > _second->_xmax || _first->_xmax < _second->_xmin ||
        _first->_ymin > _second->_ymax || _first->_ymax < _second->_ymin)? 0 : 1;
}

static quadnode_t* quadnode_create (const quadbox_t* box)
{
    quadnode_t *node = (quadnode_t*)malloc(sizeof(quadnode_t));
    if (node) {
                memset( node->_sub, 0, sizeof(node->_sub));
                memcpy (&(node->_box), box, sizeof(quadbox_t));
                INIT_LIST_HEAD(&node->_lst);
        }
    return node;
}

static void quadnode_destroy(quadnode_t* node)
{
    if (node->_sub[NE] != 0){
        quadnode_destroy (node->_sub[NE]);
        quadnode_destroy (node->_sub[NW]);
        quadnode_destroy (node->_sub[SE]);
        quadnode_destroy (node->_sub[SW]);
    }
//I think we do not need care about it
//      list_del(&node->_lst);
//    if (node->_lst)
//        list_destroy(node->_lst, NULL);

    free (node);
}

static void quadnode_create_child(quadnode_t* node, float overlap, int depth)
{
    quadbox_t ne, nw, se, sw;

    assert (node);
    quadbox_split ( &(node->_box), &ne, &nw, &se, &sw, overlap );

    node->_sub[NE] = quadnode_create (&ne);
    node->_sub[NW] = quadnode_create (&nw);
    node->_sub[SW] = quadnode_create (&sw);
    node->_sub[SE] = quadnode_create (&se);
}

static int quadnode_has_child(const quadnode_t* node)
{
    return (node->_sub[NE] != 0) ? 1 : 0;
}

static int quadnode_has_data (const quadnode_t* node)
{
    return (!list_empty((struct list_head *)&node->_lst))? 1:0;
}

static void quadnode_add_data ( quadnode_t* node, struct list_head *node_lst)
{
        list_add(node_lst, &node->_lst);
}


/* inserts a node to parent node of tree. returns pointer to node */
static quadnode_t*  quadtree_insert_node ( quadtree_t *tree,
        quadnode_t    *parent,
        struct list_head *node_lst,
        quadbox_t    *node_box,
        int            *depth
                                                                                   )
{
    if ( quadbox_is_inside (node_box, &(parent->_box)) )
    {
        if ( ++(*depth) < tree->_depth )
        {
	    //printbox(parent);
            if ( !quadnode_has_child ( parent ) )
                quadnode_create_child (parent, tree->_overlap, (*depth));

            if  ( quadbox_is_inside (node_box, &(parent->_sub[NE]->_box) ) )
                return quadtree_insert_node (tree, parent->_sub[NE], node_lst, node_box, depth);

            if ( quadbox_is_inside (node_box, &(parent->_sub[NW]->_box) ) )
                return quadtree_insert_node (tree, parent->_sub[NW], node_lst, node_box, depth);

            if ( quadbox_is_inside (node_box, &(parent->_sub[SW]->_box) ) )
                return quadtree_insert_node (tree, parent->_sub[SW], node_lst, node_box, depth);

            if ( quadbox_is_inside (node_box, &(parent->_sub[SE]->_box) ) )
                return quadtree_insert_node (tree, parent->_sub[SE], node_lst, node_box, depth);
        }

        /* inserts into this node since it can NOT be included in any subnodes */
        quadnode_add_data (parent, node_lst);
        return parent;
    }

    return NULL;
}

/* searched tree nodes */
static void  quadtree_search_nodes (quadnode_t    *current_node,
        quadbox_t     *search_box,
        list_t        *results_list[],
        int *index,
        int max_index
                                    )
{
        if (*index >= max_index)
	    return;

        if ( quadbox_is_overlapped ( &(current_node->_box), search_box ) )
    {
        if ( quadnode_has_data (current_node) ) {
//            list_append_node (results_list, list_node_create(current_node));
//                      list_add(results_list, &current_node->_lst);
//                      list_join(&current_node->_lst, results_list);
//                      memcpy(&results_list[*index], &current_node->_lst, sizeof(struct list_head));
                        results_list[*index] = &current_node->_lst;
                        ++(*index);
                       //printbox(current_node);
                }

        if ( quadnode_has_child (current_node) )
        {
            quadtree_search_nodes (current_node->_sub[NE], search_box, results_list, index, max_index);
            quadtree_search_nodes (current_node->_sub[NW], search_box, results_list, index, max_index);
            quadtree_search_nodes (current_node->_sub[SW], search_box, results_list, index, max_index);
            quadtree_search_nodes (current_node->_sub[SE], search_box, results_list, index, max_index);
        }
    }
}


/*=============================================================================
  Public Functions
  =============================================================================*/
quadtree_t*
quadtree_create (quadbox_t    box,
        int        depth,    /* 4~8 */
        float        overlap /* 0.02 ~ 0.4 */
                 )
{
    quadtree_t* qt = NULL;

    assert (depth>=QTREE_DEPTH_MIN-0.0001 && depth<=QTREE_DEPTH_MAX+0.0001);
    assert (overlap>=QBOX_OVERLAP_MIN-0.0001 && overlap<=QBOX_OVERLAP_MAX+0.0001);

    qt = (quadtree_t*)malloc(sizeof(quadtree_t));

    if (qt)
    {
        qt->_depth = depth;
        qt->_overlap = overlap;

        quadbox_inflate (&box, quadbox_width(&box)*overlap, quadbox_height(&box)*overlap);
	//printf("quadtree_create box minx:%f,maxx:%f,miny:%f,maxy:%f\n",box._xmin,
	//	box._xmax,box._ymin,box._ymax);

        qt->_root = quadnode_create (&box);
        assert (qt->_root);
        if (!qt->_root)
        {
            free (qt);
            qt = NULL;
        }
    }

    assert (qt);
    return qt;
}
/* destroys a quad tree */
void
quadtree_destroy (IN  quadtree_t    *qtree)
{
    assert (qtree && qtree->_root);
    quadnode_destroy (qtree->_root);
    free (qtree);
}

/* inserts a node into quadtree and return pointer to new object */
quadtree_object_t *
quadtree_insert (IN  quadtree_t *qtree,
	IN  void *object,
	IN  uint64_t objectid,
        IN  quadbox_t        *node_box
                 )
{
    int        depth = -1;
    quadtree_object_t *qobject = (quadtree_object_t *)malloc(sizeof(quadtree_object_t));
    qobject->object = object;
    qobject->objectid = objectid;

    quadnode_t *node = NULL;
    node = quadtree_insert_node (qtree, qtree->_root, &qobject->quad_lst, 
	    node_box, &depth);
    if(!node)
    {
	free(qobject);

	return NULL;
    }
    else
    {
	qobject->node = node;
	qobject->_box = *node_box;
	return qobject;
    }
}

/* searches nodes inside search_box */
#define MAX_LIST_INDEX (1000)
void
quadtree_search (IN  const quadtree_t    *qtree,
        IN  quadbox_t            *search_box,
        OUT quadtree_object_t                *objects[],
	IN  int max,
	OUT int *num
                 )
{
    int max_index = MAX_LIST_INDEX;
    struct list_head *results_list[max_index];
    int index = 0;
    quadtree_search_nodes (qtree->_root, search_box, results_list, &index, max_index);

    int x = 0,count = 0;
    for (x = 0; x < index && count < max; ++x)
    {
        struct list_head *l = results_list[x];
        struct list_head *n = results_list[x]->next;
        quadtree_object_t *object;
        if (!l)
                continue;
        while (n != l && count < max) {
                object = (quadtree_object_t *)((char *)n - offsetof(quadtree_object_t,
			    quad_lst) );
		objects[count++] = object;
                n = n->next;
        }
    }
    *num = count;
}

void quadtree_del_object (quadtree_object_t *object)
{
	list_del(&object->quad_lst);
	free(object);
}

int
quadtree_update (IN  quadtree_t            *qtree,
        IN  quadtree_object_t *object,
        IN  quadbox_t            *node_box
                 )
{
    quadbox_t rootbox = qtree->_root->_box;
    /*printf("node box xmin:%f,xmax:%f,ymin:%f,ymax:%f root xmin:%f,xmax:%f,\
	    ymin:%f,ymax:%f\n",node_box->_xmin,node_box->_xmax,node_box->_ymin,
	    node_box->_ymax,rootbox._xmin,rootbox._xmax,rootbox._ymin,
	    rootbox._ymax);*/
    //object->_box = *node_box;
    if ( !quadbox_is_inside (node_box, &(qtree->_root->_box)/*&(object->node->_box)*/) )
    {
	return -1;
    }
    void *obj = object->object;
    quadtree_del_object(object);
    quadtree_object_t *pobject = quadtree_insert(qtree,obj,object->objectid,node_box);
    return 0;
}

void quad_travel(quadnode_t *current_node, quad_travel_func f, void *param)
{
        if (quadnode_has_data (current_node) )
                f(&current_node->_lst, param);

        if ( quadnode_has_child (current_node) )
        {
                quad_travel(current_node->_sub[NE], f, param);
                quad_travel(current_node->_sub[NW], f, param);
                quad_travel(current_node->_sub[SW], f, param);
                quad_travel(current_node->_sub[SE], f, param);
        }
}
