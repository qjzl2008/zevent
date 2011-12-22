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
		player.character = character
		player.peerid = sender
		player.qobject = qobject
		player.active = True
		self.players[character.CharacterID] = player
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

	self.PushMsg2PlayerBuf(dplayer,msg)

    def PackLeaveAOIMsg(self,cid,splayer,dplayer):
	x = splayer.character.LocX
	y = splayer.character.LocY
	msg = '{"cmd":%d,"cid":"%s","pnm":"%s","x":%d,"y":%d}' % \
		(Packets.MSGID_NOTIFY_LEAVEAOI,
			    self.uuid.uuid2hex(cid),
			    splayer.character.PName,
			    x,y)

	self.PushMsg2PlayerBuf(dplayer,msg)

    def PushMsg2PlayerBuf(self,player,msg):
	if player.buf != '':
	    player.buf += ','
	    player.buf += msg
	else:
	    player.buf = msg

    def MainLogic(self):
	self.mutex_players.acquire()
	try:
	    for key in self.players.keys():
		player = self.players[key]
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
		    diffaoilist =list(set(oldaoilist) - (set(oldaoilist).intersection(set(player.aoilist))))
               	    num = len(objs)
		    buf = '['
		    for cid in objs:
			#if cid == key:
			#    continue
		        one_player = self.players[cid]
			self.PackSynPosMsg(cid,player,one_player)

		    for cid in diffaoilist:
		        one_player = self.players[cid]
			self.PackLeaveAOIMsg(cid,player,one_player)

	    buf = '['
	    i = 0
	    for key in self.players.keys():
		player = self.players[key]
		if player.buf != '':
		    if i > 0:
			buf += ','
		    buf += '{"peerid":"%s","msg":{"cmd":%d,"msgs":[%s]}}' % (one_player.peerid,Packets.MSGID_SCENE_FRAME,player.buf)
		    player.buf = ''
		    i += 1
            buf += ']'
	    if buf != '[]':
		buf = buf.encode("UTF-8")
		print buf
		self.gatelogic.SendData2Clients(buf)
	finally:
	    self.mutex_players.release()

