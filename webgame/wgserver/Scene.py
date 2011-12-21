import threading 
import simplejson as json 
from GlobalConfig import GlobalConfig
from npcmanager import NPC,NPCManager
from quadtree import quadtree
from Player import Player

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

    def load_scene(self,scene_cfg):
	self.scene_json = json.load(open(self.scene_cfg))
	self.sceneid = self.scene_json['id']
	self.xmin = self.scene_json['xmin']
	self.xmax = self.scene_json['xmax']
	self.ymin = self.scene_json['ymin']
	self.ymax = self.scene_json['ymax']
	self.npc_manager.load_npcs(self.scene_json)
	pass
    
    def add_player(self,sender,character):
	self.mutex_players.acquire()
	try:
	    objbox = quadtree.quad_box_t();

	    rv = True

	    objbox._xmin = character.LocX - character.XScale
	    objbox._xmax = character.LocX + character.XScale
	    objbox._ymin = character.LocY - character.YScale
	    objbox._ymax = character.LocY + character.YScale
	    qobject = self.qdtree.quadtree_insert(character.CharacterID,objbox)
	    if not qobject:
		rv = False
	    else:
		player = Player()
		character.State = Player.ENTERED_STATE
		player.character = character
		player.peerid = sender
		player.qobject = qobject
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
		 player.character.LocX = x
		 player.character.LocY = y
	         objbox = quadtree.quad_box_t()
		 objbox._xmin = x - player.character.XScale
	       	 objbox._xmax = x + player.character.XScale
		 objbox._ymin = y - player.character.YScale
		 objbox._ymax = y + player.YScale
                 quadtree.quadtree_update(qobject,objbox)
		 rv = True
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
		    self.qdtree.quadtree_del_object(qobject)
		del self.players[cid]
	finally:
		self.mutex_players.release()
		return rv

    def MainLogic(self):
	box = quadtree.quad_box_t();
	objs = []
	box._xmin = 0.0
	box._xmax = 400.0
	box._ymin = 0.0
	box._ymax = 600.0

	self.qdtree.quadtree_search(box,objs,1000)
	for cid in objs:
	    player = self.get_player(cid)
	    print player.character.LocX
	    print player.character.LocY

