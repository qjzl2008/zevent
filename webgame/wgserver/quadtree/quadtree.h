/*
 * quadtree.h
 *        Quad tree structure -- for spatial quick searching
 */
#ifndef QUADTREE_H_INCLUDED
#define QUADTREE_H_INCLUDED

#define IN
#define OUT

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <list.h>

#define QUAD_SUBNODES        4

#define QBOX_OVERLAP_MAX    0.4
#define QBOX_OVERLAP_MIN    0.02

#define QTREE_DEPTH_MAX        8
#define QTREE_DEPTH_MIN        4

#define QUADRANT_BITS        3

/* a quadrant defined below:

        NW(1)   |    NE(0)
        -----------|-----------
        SW(2)   |    SE(3)
*/
typedef enum
{
    NE = 0,
    NW = 1,
    SW = 2,
    SE = 3
}QuadrantEnum;

/* a box defined below:
           _____max
          |__|__|
          |__|__|
   min
*/
typedef struct _quadbox_t
{
    double    _xmin,
            _ymin,
            _xmax,
            _ymax;
}quadbox_t;

/* quad node */
typedef struct _quadnode_t
{
    quadbox_t             _box;                        /* node bound box */
    struct list_head        _lst;                              /* node data list*/
    struct _quadnode_t    *_sub[QUAD_SUBNODES];        /* pointer to subnodes of this node */
}quadnode_t;

/* quad tree */
typedef struct _quadtree_t
{
    quadnode_t        *_root;
    int                 _depth;                   /* max depth of tree: 0-based */
    float             _overlap;                   /* overlapped ratio of quanbox */
} quadtree_t;

/* quad object*/
typedef struct quadtree_object {
     struct list_head quad_lst;
     void *object;
     quadnode_t *node;
     quadbox_t _box;
     uint64_t objectid;
}quadtree_object_t;

/*=============================================================================
                        Public Functions
=============================================================================*/
/* creates a quadtree and returns a pointer to it */
extern  quadtree_t*
quadtree_create (quadbox_t    box,
                 int        depth,  /* 4~8 */
                 float        overlap /* 0.02 ~ 0.4 */
                 );

/* destroys a quad tree and free all memory */
extern  void
quadtree_destroy (IN  quadtree_t        *qtree
                  );

/* inserts a node identified by node_key into a quadtree, returns the node quadtree encoding */
extern  quadtree_object_t *
quadtree_insert (IN  quadtree_t            *qtree,
	IN  void *qobject,
	IN  uint64_t objectid,
        IN  quadbox_t            *node_box
                 );

/* searches objects inside search_box */
extern void
quadtree_search (IN  const quadtree_t    *qtree,
        IN  quadbox_t            *search_box,
        OUT quadtree_object_t                *objects[],
	IN  int max,
	OUT int *num
	);

extern  int
quadtree_update (IN  quadtree_t            *qtree,
        IN  quadtree_object_t *object,
        IN  quadbox_t            *node_box
                 );

extern void quadtree_del_object (quadtree_object_t *object);

typedef void (*quad_travel_func)(struct list_head *, void *);
void quad_travel(quadnode_t *current_node, quad_travel_func f, void *param);

#endif // QUADTREE_H_INCLUDED
