# -*- coding: utf-8 -*- 
import os
import threading 
import simplejson as json 
from log import *

from GlobalConfig import GlobalConfig
from Scene import Scene

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
	self.scenelist = scenes['scenes'].split(',')
	start_scene = scenes['start_scene']
	for scenename in self.scenelist:
	    scenecfg = path + os.sep + scenename
	    if not os.path.exists(scenecfg) and not os.path.isfile(scenecfg):
		PutLogList("(!) Cannot open configuration file:%s" % scenecfg)
		self.scenelist.remove(scenename)
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
	self.mutex.release()
	scene.del_player(cid)
	
	return cid

    def ProcessEnterGame(self,sender,character):
	if character.Scene == 0:
	    character.Scene = self.newbie_scene
	scene = self.scenes[character.Scene]
	if not scene.add_player(sender,character):
	    return False

        self.mutex.acquire()   
	rv = True
	try:
	    if character.Scene == 0:
		character.Scene = self.newbie_scene
	    scene = self.scenes[character.Scene]

	    self.c2scene[character.CharacterID] = scene.sceneid
	    self.peer2cid[sender] = character.CharacterID
	except:
	    rv = False
	finally:
	    self.mutex.release()
	    return rv

    def GetSceneByPID(self,hexpeerid):
        self.mutex.acquire()   
	scene = None
	cid = None
	try:
	    if self.peer2cid.has_key(hexpeerid):
		cid = self.peer2cid[hexpeerid];
		if self.c2scene.has_key(cid):
		    sceneid = self.c2scene[cid]
		    if self.scenes.has_key(sceneid):
			scene = self.scenes[sceneid]
	finally:
	    self.mutex.release()
	    return (scene,cid)

    def ProcessSynPos(self,obj):
        hexpeerid = obj['peerid']	
	(scene,cid) = self.GetSceneByPID(hexpeerid)
	if scene:
	    x = obj['x']
	    y = obj['y']
	    scene.update_pos(cid,x,y)
    
    def MainLogic(self):
	for key in self.scenes.keys():
	    scene = self.scenes[key]
	    scene.MainLogic()
	

