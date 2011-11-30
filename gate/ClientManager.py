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
            message = (rv,peerid,msg,len,void_pointer)
	"""
	while(True):
	    sleep = True
	    message = self.nserver.ns_recvmsg(0)
	    if message:
		if message[0] == 1:
		    PutLogList("(*) peer(ID:%d) disconnected" % message[1])
		    #self.playermanager.ProcessLeaveGame(message[1])
		else:
		    self.ProcessMsg(message)
		    self.nserver.ns_free(message[4])
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
   
    def ProcessMsg(self,message):
	"""
           message = (rv,peerid,msg,len,void_pointer)
	   void_pointer ns_free的参数 释放消息到底层网络库内存池
	"""
	#self.nserver.ns_sendmsg(message[0],message[1],message[2])
	#return
	try:
	    obj = json.loads(message[2][4:])
	except:
	    PutLogFileList("Packet len: %d b * %s" % (len(message[2][4:]),
		repr(message[2][4:])), Logfile.PACKETMS)
	    return

        peerid = message[1]
	if obj['cmd'] == Packets.MSGID_REQUEST_LOGIN:
	    self.playermanager.ProcessClientLogin(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_BINDGS:
	    self.playermanager.ProcessClientRequestBindGS(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWACCOUNT:
	    self.playermanager.CreateNewAccount(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_DATA2GS:
	    self.playermanager.ProcessClientData2GS(peerid,obj)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[2][4:]),
		repr(message[2][4:])), Logfile.PACKETMS)
	    return

    def Send2Clients(self,obj):
	self.playermanager.Send2Clients(obj)

