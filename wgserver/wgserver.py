# -*- coding: utf-8 -*- 
import os
import threading
from net import msg
import random
import time
from Database import Account, Character, Item, Skill, DatabaseDriver
from playermanager import PlayerManager
from scenemanager import SceneManager
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *

# Consumer thread

class WGServer(threading.Thread):
    def __init__(self, threadname,nserver):
	threading.Thread.__init__(self, name = threadname)
	self.nserver = nserver
        self.MaxTotalUsers = 1000
        self.WorldServerName = "WS1"
	self.gconfig = GlobalConfig.instance()

    def run(self):
	while(True):
	    message = self.nserver.recvmsg()
	    if message == None:
	    	continue
	    self.processmsg(message)

    def Init(self):
        """
        Loading main configuration, and initializing Database Driver
        (For now, its MySQL)
        """
	self.dbaddress = self.gconfig.GetValue('CONFIG','db')
	if not self.dbaddress:
	    return False

	PutLogList("(*) DB address : %s" % self.dbaddress,'',False)
	self.Database = DatabaseDriver.instance(self.dbaddress)
        if not self.Database:
	    PutLogList("(!) DatabaseDriver initialization fails!")
	    return False

	self.playermanager = PlayerManager.instance(self.nserver,self.Database)
	self.scmanager = SceneManager.instance(self.nserver)
	return True
   
    def processmsg(self,message):
	print "Recv msg:%s" % (message.data)
	try:
	    obj = json.loads(message.data)
	except:
	    PutLogFileList("Packet len: %d b * %s" % (len(message.data),
		 repr(message.data)), Logfile.PACKETMS)
	    return

	if obj['cmd'] == Packets.MSGID_REQUEST_LOGIN:
	    self.playermanager.ProcessClientLogin(message.peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_ENTERGAME:
	    self.playermanager.ProcessClientRequestEnterGame(message.peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_LEAVEGAME:
	    self.playermanager.ProcessLeaveGame(message.peerid)
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWACCOUNT:
	    self.playermanager.CreateNewAccount(message.peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWCHARACTER:
	    self.playermanager.CreateNewCharacter(message.peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_GETCHARLIST:
	    self.playermanager.ProcessGetCharList(message.peerid)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj[0], len(message.data),
		 repr(message.data)), Logfile.PACKETMS)
	    return;

	    #self.nserver.sendmsg(msg.peerid,msg.data)
	    #print msg.peerid,msg.data,len(msg.data)
	pass

