# -*- coding: utf-8 -*- 
import os
import threading
import random
import time
from Database import Account, DatabaseDriver
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from PlayerManager import PlayerManager
from GSManager import GSManager
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *

class ClientManager(threading.Thread):
    def __init__(self, threadname, nserver):
	threading.Thread.__init__(self, name=threadname)
	self.nserver = nserver
	self.gconfig = GlobalConfig.instance()

    def run(self):
	"""
            message = (peerid,msg,len,void_pointer)
	"""
	while(True):
	    sleep = True
	    message = self.nserver.ns_recvmsg(0)
	    if message:
		sleep = False
		self.processmsg(message)
		self.nserver.ns_free(message[3])
	    continue

    def Init(self,gsmanager):
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

	self.playermanager = PlayerManager.instance(self,self.Database)
	self.gsmanager = gsmanager
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
	elif obj['cmd'] == Packets.MSGID_REQUEST_BINDGS:
	    self.playermanager.ProcessClientRequestBindGS(peerid,obj)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj[0], len(message[1][4:]),
		repr(message[1][4:])), Logfile.PACKETMS)
	    return

    def Send2Clients(self,obj):
	self.playermanager.Send2Clients(obj)

