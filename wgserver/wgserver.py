# -*- coding: utf-8 -*- 
import os
import threading
from net import msg
import random
import time
from Database import Account, Character, Item, Skill, DatabaseDriver
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from playermanager import PlayerManager
from scenemanager import SceneManager
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *

# Consumer thread

class WGServer:
    def __init__(self, nserver):
	self.nserver = nserver
        self.MaxTotalUsers = 1000
        self.WorldServerName = "WS1"
	self.gconfig = GlobalConfig.instance()

    def run(self):
	"""
            message = (peerid,msg,len,void_pointer)
	"""
	while(True):
	    sleep = True
	    message = self.nserver.ns_recvmsg()
	    if message:
		sleep = False
		self.processmsg(message)
		self.nserver.ns_free(message[3])
	    #gs logic
	    #self.MainLogic()
	    #if sleep:
	#	time.sleep(0.01)
	    continue

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
	"""
           message = (peerid,msg,len,void_pointer)
	   void_pointer ns_free的参数 释放消息到底层网络库内存池
	"""
	#self.nserver.ns_sendmsg(message[0],message[1],message[2])
	#return
	try:
	    obj = json.loads(message[1][4:])
	except:
	    PutLogFileList("Packet len: %d b * %s" % (len(message[1][4:]),
		repr(message[1][4:])), Logfile.PACKETMS)
	    return

        peerid = message[0]
	if obj['cmd'] == Packets.MSGID_REQUEST_LOGIN:
	    self.playermanager.ProcessClientLogin(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_ENTERGAME:
	    self.playermanager.ProcessClientRequestEnterGame(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_LEAVEGAME:
	    self.playermanager.ProcessLeaveGame(peerid)
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWACCOUNT:
	    self.playermanager.CreateNewAccount(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWCHARACTER:
	    self.playermanager.CreateNewCharacter(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_GETCHARLIST:
	    self.playermanager.ProcessGetCharList(peerid)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj[0], len(message[1][4:]),
		repr(message[1][4:])), Logfile.PACKETMS)
	    return

	    #self.nserver.sendmsg(msg.peerid,msg.data)
	    #print msg.peerid,msg.data,len(msg.data)
	pass
    def MainLogic(self):
	self.scmanager.MainLogic()
	pass

