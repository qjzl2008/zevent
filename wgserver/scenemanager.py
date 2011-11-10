# -*- coding: utf-8 -*- 
import os
import simplejson as json 
from log import *

class  Scene(object):
    """
        	
    """
    def __init__(self,scene_cfg):
	self.scene_cfg = scene_cfg
	self.scene = {}
	self.players = {}
	self.load_scene(scene_cfg)

    def load_scene(self,scene_cfg):
	scene = json.load(open(self.scene_cfg))
	self.sceneid = scene['id']
	pass

    def get_object(self,key,objectid):
	pass

class SceneManager(object):
    def __init__(self):
	self.scenes = {}
	self.c2scene = {}

    @classmethod
    def instance(cls):
	if not hasattr(cls, "_instance"):
	    cls._instance = cls()
	    cls._instance.Init()
	    return cls._instance

    @classmethod
    def initialized(cls):
	 return hasattr(cls, "_instance")
    
    def get_scene(sceneid):
	scene = None
	try:
	    scene = self.scenes[sceneid]
	finally:
	    return scene

    def Init(self):
	self.scenescfg = "./data/scenescfg.json"
        if not os.path.exists(self.scenescfg) and not os.path.isfile(self.scenescfg):
	    PutLogList("(!) Cannot open configuration file:%s" % self.scenescfg)
	    return False

	scenes = json.load(open(self.scenescfg))
	path = scenes['path']
	scenelist = scenes['scenes'].split(',')
	start_scene = scenes['start_scene']
	for scenecfg in scenelist:
	    scenecfg = path + os.sep + scenecfg
	    if not os.path.exists(scenecfg) and not os.path.isfile(scenecfg):
		PutLogList("(!) Cannot open configuration file:%s" % scenecfg)
		continue
	    scene = Scene(scenecfg)
	    self.scenes[scene.sceneid] = scene

	    start_scene = path + os.sep + start_scene
	    if start_scene == scenecfg:
		self.newbie_scene = scene.sceneid
	return True

    def ProcessLeaveGame(self,characterid):
	try:
	    sceneid = self.c2scene[characterid]
	    scene = self.scenes[sceneid]
	    del scene.players[characterid]
	    del self.c2scene[characterid]
	    return True
	except:
	    return False

    def ProcessEnterGame(self,character):
        from playermanager import Player
	try:
	    if character.Scene == 0:
		character.Scene = self.newbie_scene
	    scene = self.scenes[character.Scene]
	    character.State = Player.ENTERED_STATE
	    scene.players[character.CharacterID] = character
	    self.c2scene[character.CharacterID] = scene.sceneid
	    print "enchid:%d" % (character.CharacterID)
	except:
	    character = None
	finally:
	    return character


