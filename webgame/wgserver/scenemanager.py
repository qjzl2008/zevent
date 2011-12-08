# -*- coding: utf-8 -*- 
import os
import threading 
import simplejson as json 
from log import *
from npcmanager import NPC,NPCManager

class  Player(object):

    INIT_STATE = 0x00
    ENTERED_STATE = 0x01

    def __init__(self):
	#0 1 logined 2 entered
	self.state = self.INIT_STATE
	self.peerid = -1
	self.character = None

class  Scene(object):
    """
        	
    """
    def __init__(self,scene_cfg):
	self.scene_cfg = scene_cfg
	self.scene = {}
	self.players = {}
	self.npc_manager = NPCManager()

	self.load_scene(scene_cfg)

    def load_scene(self,scene_cfg):
	self.scene_json = json.load(open(self.scene_cfg))
	self.sceneid = self.scene_json['id']
	self.npc_manager.load_npcs(self.scene_json)
	pass

    def get_object(self,key,objectid):
	pass

class SceneManager(object):
    def __init__(self):
	self.mutex = threading.Lock() 
	self.scenes = {}
	self.c2scene = {}
	self.peer2cid = {}

    @classmethod
    def instance(cls):
	if not hasattr(cls, "_instance"):
	    cls._instance = cls()
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

    def ProcessClientDisconnect(self,obj):
	self.mutex.acquire()
	peerid = obj['peerid']
	if not self.peer2cid.has_key(peerid):
	    self.mutex.release()
	    return False
	cid = self.peer2cid[peerid]
	del self.peer2cid[peerid]

	if not self.c2scene.has_key(cid):
	    self.mutex.release()
	    return False
	sceneid = self.c2scene[cid]
	del self.c2scene[cid]

	if not self.scenes.has_key(sceneid):
	    self.mutex.release()
	    return False
	scene = self.scenes[sceneid]
	
	if not scene.players.has_key(cid):
	    self.mutex.release()
	    return False
	del scene.players[cid]
	self.mutex.release()
	return cid

    def ProcessEnterGame(self,sender,character):
        self.mutex.acquire()   
	try:
	    if character.Scene == 0:
		character.Scene = self.newbie_scene
	    scene = self.scenes[character.Scene]

	    player = Player()
	    character.State = Player.ENTERED_STATE
	    player.character = character
	    player.peerid = sender

	    scene.players[character.CharacterID] = player
	    self.c2scene[character.CharacterID] = scene.sceneid
	    self.peer2cid[sender] = character.CharacterID
	except:
	    character = None
	finally:
	    self.mutex.release()
	    return character
    
    def MainLogic(self):
	for key in self.scenes.keys():
	    scene = self.scenes[key]
	    for cid in scene.players.keys():
		print "cid:%d" % (cid)
	pass

