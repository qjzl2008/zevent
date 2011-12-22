# -*- coding: utf-8 -*- 
import threading 
import simplejson as json 
from GlobalConfig import GlobalConfig
from NetMessages import Packets   
from npcmanager import NPC,NPCManager
from quadtree import quadtree
from Player import Player
from uuid import uuid

class  Scene(object):
    """
        	
    """
    def __init__(self,scene_cfg):
	self.mutex_players = threading.Lock() 
	self.scene_cfg = scene_cfg
	self.scene = {}
	self.players = {}
	self.offline_players = []

	self.qobjects = {}
	self.npc_manager = NPCManager()

        self.gconfig = GlobalConfig.instance()
	self.qdtreelib = self.gconfig.GetValue('CONFIG','qdtree-lib')

	self.load_scene(scene_cfg)
	self.qdtree = quadtree.quadtree(self.qdtreelib);
	box = quadtree.quad_box_t()
	box._xmin = self.xmin
	box._xmax = self.xmax
	box._ymin = self.ymin
	box._ymax = self.ymax
	self.qdtree.quadtree_create(box,5,0.1)

	self.uuid = uuid.instance()

        from GateLogic import GateLogic
	self.gatelogic = GateLogic.instance()

    def load_scene(self,scene_cfg):
	self.scene_json = json.load(open(self.scene_cfg))
	self.sceneid = self.scene_json['id']
	self.xmin = self.scene_json['xmin']
	self.xmax = self.scene_json['xmax']
	self.ymin = self.scene_json['ymin']
	self.ymax = self.scene_json['ymax']

	self.startloc_x = self.scene_json['start']['loc_x']
	self.startloc_y = self.scene_json['start']['loc_y']
	self.startrange_x = self.scene_json['start']['range_x']
	self.startrange_y = self.scene_json['start']['range_y']
	self.npc_manager.load_npcs(self.scene_json)
    
    def add_player(self,sender,character):
	self.mutex_players.acquire()
	try:
	    if character.LocX <= 0:
		character.LocX = self.startloc_x
	    if character.LocY <= 0:
		character.LocY = self.startloc_y
	    objbox = quadtree.quad_box_t();

	    rv = True

	    objbox._xmin = character.LocX - character.XScale
	    objbox._xmax = character.LocX + character.XScale
	    objbox._ymin = character.LocY - character.YScale
	    objbox._ymax = character.LocY + character.YScale

            #print objbox._xmin,objbox._xmax,objbox._ymin,objbox._ymax
	    qobject = self.qdtree.quadtree_insert(character.CharacterID,objbox)
	    if not qobject:
		rv = False
	    else:
		player = Player()
		character.State = Player.ENTERED_STATE
		player.state = Player.ENTERED_STATE
		player.character = character
		player.peerid = sender
		player.qobject = qobject
		player.active = True
		self.players[character.CharacterID] = player
		rv = True
        finally:
	    self.mutex_players.release()
	    return rv

    def SetPlayerReady(self,cid):
	self.mutex_players.acquire()
	rv = True
	try:
	     if not self.players.has_key(cid):
		 rv = False
	     else:
		 player = self.players[cid]
		 player.state = Player.READY_STATE 
		 rv = True
	finally:
	    self.mutex_players.release()
	    return rv

    def update_pos(self,cid,x,y):
	self.mutex_players.acquire()
	rv = True
	try:
	     if not self.players.has_key(cid):
		 rv = False
	     else:
		 player = self.players[cid]
		 qobject = player.qobject
		 player.active = True
	         objbox = quadtree.quad_box_t()
		 objbox._xmin = x - player.character.XScale
	       	 objbox._xmax = x + player.character.XScale
		 objbox._ymin = y - player.character.YScale
		 objbox._ymax = y + player.character.YScale
		 print "update object->",objbox._xmin,objbox._xmax,objbox._ymin,objbox._ymax
		 rv = self.qdtree.quadtree_update(qobject,objbox);
		 if rv:
		     player.character.LocX = x
		     player.character.LocY = y
		     rv = True
		 else:
		     rv = False
	finally:
	    self.mutex_players.release()
	    return rv
	
    def get_player(self,cid):
	self.mutex_players.acquire()
	player = None
	try:
	    if self.players.has_key(cid):
		player = self.players[cid]
	finally:
	    self.mutex_players.release()
	    return player
		
    def del_player(self,cid):
	self.mutex_players.acquire()
	rv = True
	try:
	    if not self.players.has_key(cid):
		rv = False
	    else:
		player = self.players[cid]
		qobject = player.qobject
		if qobject:
		    print "del object!"
		    self.qdtree.quadtree_del_object(qobject)
		    qobject = None
                if player.state == Player.READY_STATE:
		    self.offline_players.append(player)
		del self.players[cid]
	finally:
		self.mutex_players.release()
		return rv

    def PackSynPosMsg(self,cid,splayer,dplayer):
	x = splayer.character.LocX
	y = splayer.character.LocY

	msg = '{"cmd":%d,"cid":"%s","pnm":"%s","x":%d,"y":%d}' % \
		(Packets.MSGID_NOTIFY_SYNPOS,
			    self.uuid.uuid2hex(cid),
			    splayer.character.PName,
			    x,y)

	dplayer.sendmsgs.append(msg)

    def PackLeaveAOIMsg(self,cid,splayer,dplayer):
	x = splayer.character.LocX
	y = splayer.character.LocY
	msg = '{"cmd":%d,"cid":"%s","pnm":"%s","x":%d,"y":%d}' % \
		(Packets.MSGID_NOTIFY_LEAVEAOI,
			    self.uuid.uuid2hex(cid),
			    splayer.character.PName,
			    x,y)
	dplayer.sendmsgs.append(msg)

    def PushSynPosMsgs(self):
	self.mutex_players.acquire()
	try:
	    for key in self.players.keys():
		player = self.players[key]
		if player.state != Player.READY_STATE:
		    continue
		if player.active:
		    player.active = False
		    qobject = player.qobject
		    x = player.character.LocX
		    y = player.character.LocY
		    box = quadtree.quad_box_t();
		    objs = []
		    box._xmin = x-1000.0
		    box._xmax = x+1000.0
		    box._ymin = y-1000.0
		    box._ymax = y+1000.0

		    self.qdtree.quadtree_search(box,objs,1000)
		    oldaoilist = player.aoilist

		    player.aoilist = objs
		    #交集 需要同步
		    interaoilist = list(set(oldaoilist).intersection(set(player.aoilist)))
		    #离开的集合 需要发离开消息
		    leaveaoilist =list(set(oldaoilist) - (set(oldaoilist).intersection(set(player.aoilist))))
		    #新增的集合 需要发创建消息
		    newaoilist =list(set(player.aoilist) - (set(oldaoilist).intersection(set(player.aoilist))))
		    buf = '['
		    for cid in interaoilist:
			if cid == key:
			    continue
		        one_player = self.players[cid]
			#告诉对方
			self.PackSynPosMsg(key,player,one_player)
			print "interaoilist"

		    for cid in newaoilist:
			if cid == key:
			    continue
		        one_player = self.players[cid]
			#告诉对方
			self.PackSynPosMsg(key,player,one_player)
			#告诉自己一玩家入视野
			self.PackSynPosMsg(cid,one_player,player)
			#将自己加入一玩家兴趣列表
			one_player.aoilist.append(key)
			print "newaoilist"

                    #离开某些人的兴趣区域告知她们
		    for cid in leaveaoilist:
			if cid == key:
			    continue
			if self.players.has_key(cid):
			    one_player = self.players[cid]
			    self.PackLeaveAOIMsg(key,player,one_player)
			    print "leaveaoilist"
            #处理离线玩家的同步信息
	    for player in self.offline_players:
		offline_cid = player.character.CharacterID
		for cid in set(player.aoilist):
		    if self.players.has_key(cid):
			one_player = self.players[cid]
			self.PackLeaveAOIMsg(offline_cid,player,one_player)
		del player.aoilist[:]
	    del self.offline_players[:]
	finally:
	    self.mutex_players.release()
    
    def SendSceneFrame(self):
	self.mutex_players.acquire()
	try:
	    buf = '['
	    i = 0
	    for key in self.players.keys():
		player = self.players[key]
		if len(player.sendmsgs) > 0:
		    if i>0:
			buf += ','
		    msgs = ','.join(player.sendmsgs)
		    buf += '{"peerid":"%s","msg":{"cmd":%d,"msgs":[%s]}}' % (player.peerid,Packets.MSGID_SCENE_FRAME,msgs)
		    i += 1
		    del player.sendmsgs[:] 
            buf += ']'
	    if buf != '[]':
		buf = buf.encode("UTF-8")
		print buf
		self.gatelogic.SendData2Clients(buf)
	finally:
	    self.mutex_players.release()

    def MainLogic(self):
	self.PushSynPosMsgs()
	self.SendSceneFrame()

