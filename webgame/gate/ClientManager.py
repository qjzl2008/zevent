# -*- coding: utf-8 -*- 
import os
import threading
import random
import time
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from PlayerManager import PlayerManager
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *
from nserver import ns_arg_t,net_server_t,net_server

class ClientManager(threading.Thread):
    def __init__(self):
	threading.Thread.__init__(self, name="clientmanager")

    @classmethod
    def instance(cls):
	if not hasattr(cls, "_instance"):
	    cls._instance = cls()
	return cls._instance

    @classmethod
    def initialized(cls):
	return hasattr(cls, "_instance")

    def Init(self):
        """
        """
        from GSManager import GSManager
	self.gconfig = GlobalConfig.instance()

        #start server
	znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	csip = self.gconfig.GetValue('CONFIG','clients-server-address')
	csport = self.gconfig.GetValue('CONFIG','clients-server-port')
	self.nserver = net_server(znetlib)
	ns_arg = ns_arg_t()
	ns_arg.ip = csip
	ns_arg.port = csport
	self.nserver.ns_start(ns_arg)

	self.playermanager = PlayerManager.instance()
	self.playermanager.Init()
        self.gsmanager = GSManager.instance()
	return True
 
    def run(self):
	"""
            message = (rv,peerid,msg,len,void_pointer)
	"""
	while(True):
	    sleep = True
	    message = self.nserver.ns_recvmsg(0)
	    if message:
		if message[0] == 1:
		    PutLogList("(*) Client(ID:%d) disconnected" % message[1])
		    self.playermanager.ProcessClientDisConnect(message[1])
		else:
		    self.ProcessMsg(message)
		    self.nserver.ns_free(message[4])
	    continue

  
    def ProcessMsg(self,message):
	"""
           message = (rv,peerid,msg,len,void_pointer)
	   void_pointer ns_free的参数 释放消息到底层网络库内存池
	"""
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
	elif obj['cmd'] == Packets.MSGID_REQUEST_DATA2GS:
	    self.playermanager.ProcessClientData2GS(peerid,obj)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[2][4:]),
		repr(message[2][4:])), Logfile.PACKETMS)
	    return

    def Send2Player(self,peerid,buf):
	self.playermanager.Send2Player(peerid,buf)

    def Send2Clients(self,obj):
	self.playermanager.Send2Clients(obj)

