# -*- coding: utf-8 -*- 

class  NPC(object):
    def __init__(self, npcid):
	self.npcid = npcid

    def setloc(loc_x,loc_y):
	self.loc_x = loc_x
	self.loc_y = loc_y

class NPCManager(object):
    def __init__(self):
	self.npcs = {}

    def load_npcs(self,scene_jsobj):
	for npc in scene_jsobj['npcs']:
	    self.npcs[npc['id']] = npc
	pass


