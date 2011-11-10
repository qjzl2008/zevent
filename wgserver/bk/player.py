#!/usr/bin/env python
"""
"""
class  player(object):
    def __init__(self):
	self.uid = 0
	self.uname = ""
	self.pwd = ""
	self.sex = 0


class playermanager(object):
    def __init__(self):
	self.umap ={}

    @classmethod
    def instance(cls):
	if not hasattr(cls, "_instance"):
	    cls._instance = cls()
	    return cls._instance

    @classmethod
    def initialized(cls):
	return hasattr(cls, "_instance")

    def joinplayer(self,player):
	umap[player.self.uid] = player
    
    def leaveplayer(self,player):
	umap[player.self.uid] = None
    
    def 

