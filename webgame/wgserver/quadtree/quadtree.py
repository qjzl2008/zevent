# -*- coding: utf-8 -*- 
from ctypes import *
import time
class quad_box_t(Structure):
    pass
quad_box_t._fields_ = [('_xmin',c_double),
	('_ymin',c_double),
	('_xmax',c_double),
	('_ymax',c_double)
	]

class quadtree_t(Structure):
    pass

class list_head_t(Structure):
    pass

list_head_t._fields_ = [('next',c_void_p),
	('prev',c_void_p)]

class quadtree_object_t(Structure):
    pass

quadtree_object_t._fields_ = [('quad_lst',list_head_t),
	('object',c_void_p),
	('node',c_void_p),
	('_box',quad_box_t),
	('objectid',c_ulonglong)]

class quadtree(object):

    def __init__(self,libname):
	self.lib = cdll.LoadLibrary(libname)

    def quadtree_create(self,box,depth,overlap):
	self.lib.quadtree_create.restype = POINTER(quadtree_t)
	self.lib.quadtree_create.argtypes = [quad_box_t,
		c_int,c_float]
	self.quadtree = self.lib.quadtree_create(box,depth,overlap)
	if bool(self.quadtree) == False:
	    return False
	return True
    
    def quadtree_insert(self,objectID,box):
	self.lib.quadtree_insert.restype = POINTER(quadtree_object_t)
	self.lib.quadtree_insert.argtypes = [POINTER(quadtree_t),
		c_void_p,c_ulonglong,POINTER(quad_box_t)]
        null_ptr = c_void_p()
	pbox = pointer(box)
	qobject = self.lib.quadtree_insert(self.quadtree,null_ptr,objectID,pbox)
	#空指针判断
	if bool(qobject) == False:
	    return False
	return qobject

    def quadtree_search(self,box,objects,maxnum):
	self.lib.quadtree_search.argtypes = [POINTER(quadtree_t),
		POINTER(quad_box_t),
		POINTER(quadtree_object_t) * maxnum,
		c_int,POINTER(c_int)]

	num = c_int(0)
        num = pointer(num)
	pbox = pointer(box)
	pobjs = (POINTER(quadtree_object_t) * maxnum)()
        self.lib.quadtree_search(self.quadtree,pbox,pobjs,maxnum,num)
	i = 0
	while i < num[0]:
	    objects.append(pobjs[i].contents.objectid)
	    i+=1

    def quadtree_del_object(self,qobject):
	self.lib.quadtree_search.argtypes = [POINTER(quadtree_object_t)]
	self.lib.quadtree_del_object(qobject)

    def quadtree_update(self,qobject,box):
	self.lib.quadtree_update.argtypes = [POINTER(quadtree_t),
		POINTER(quadtree_object_t),POINTER(quad_box_t)]
	pbox = pointer(box)
	self.lib.quadtree_update(self.quadtree,qobject,pbox)

if __name__ == "__main__":
    import random
    quadtree = quadtree("libquadtree.so")
    box = quad_box_t();
    box._xmin = 0.0
    box._xmax = 5000.0
    box._ymin = 0.0
    box._ymax = 5000.0
    tree = quadtree.quadtree_create(box,5,0.1)
    objbox = quad_box_t()
    
    count = 0
    while count < 1:
#	print "id(a):%d,id(count):%d" % (a,count)
	objbox._xmin = random.randint(0, 400)
	objbox._xmax = objbox._xmin+20
	objbox._ymin = random.randint(0, 600)
	objbox._ymax = objbox._ymin+20
	print "x1:%f,x2:%f,y1:%f,y2:%f" % (objbox._xmin,objbox._xmax,
		objbox._ymin,objbox._ymax)
	pobject = quadtree.quadtree_insert(count,objbox)
#	quadtree.quadtree_del_object(pobject)
        objbox._xmin = 1000.0
	objbox._xmax = 1100.0
        objbox._ymin = 1000.0
	objbox._ymax = 1100.0
        quadtree.quadtree_update(pobject,objbox)
	count+=1

    objs = []
    objbox._xmin = 0.0
    objbox._xmax = 400.0
    objbox._ymin = 0.0
    objbox._ymax = 600.0

    objbox._xmin = 1130.0
    objbox._xmax = 1200.0
    objbox._ymin = 1130.0
    objbox._ymax = 1200.0


    quadtree.quadtree_search(objbox,objs,1000)
    print objs

