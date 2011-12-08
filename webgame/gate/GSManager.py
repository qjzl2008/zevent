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

class GSManager(threading.Thread):
    def __init__(self):
	threading.Thread.__init__(self, name="GSManager")
	self.mutex_gsmanager = threading.Lock()
	self.gs2peer = {}
	self.peer2gs = {}

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
        from ClientManager import ClientManager
	self.gconfig = GlobalConfig.instance()

        #start server
	znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	gsip = self.gconfig.GetValue('CONFIG','gs-server-address')
	gsport = self.gconfig.GetValue('CONFIG','gs-server-port')
	self.nserver = net_server(znetlib)
	ns_arg = ns_arg_t()
	ns_arg.ip = gsip
	ns_arg.port = gsport
	self.nserver.ns_start(ns_arg)

	self.client_manager = ClientManager.instance()
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
		    PutLogList("(*) peer(ID:%d) disconnected" % message[1])
		    self.ProcessGSUnRegister(message[1])
		else:
		    self.ProcessMsg(message)
		    self.nserver.ns_free(message[4])
	    continue

  
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

	if obj['cmd'] == Packets.MSGID_REQUEST_REGGS:
	    self.ProcessGSRegister(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_DATA2CLIENTS:
	    self.ProcessData2Clients(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_RESPONSE_ENTERGAME:
	    self.ProcessEnterGameResponse(peerid,obj)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[2][4:]),
		repr(message[2][4:])), Logfile.PACKETMS)
	    return
	pass
    def SendRes2Request(self,sender,cmd,code):
	msg = '{"cmd":%d,"code":%d}' % (cmd,code)
	fmt = '>i%ds' % (len(msg))
	SendData = struct.pack(fmt,len(msg),msg)
	self.nserver.ns_sendmsg(sender,SendData,len(SendData))

    def ProcessGSRegister(self,sender,msg):
	gsid = msg['gsid']
	self.mutex_gsmanager.acquire()
	try:
	    if self.gs2peer.has_key(gsid):
		self.nserver.ns_disconnect(self.gs2peer[gsid])
	    self.gs2peer[gsid] = sender
	    self.peer2gs[sender] = gsid
	finally:
	    self.mutex_gsmanager.release()
	self.SendRes2Request(sender,Packets.MSGID_RESPONSE_REGGS,
		Packets.DEF_MSGTYPE_CONFIRM)

    def ProcessGSUnRegister(self,sender):
	self.mutex_gsmanager.acquire()
	try:
	    if self.peer2gs.has_key(sender):
		gsid = self.peer2gs[sender]
		del self.gs2peer[gsid] 
		del self.peer2gs[sender]
	finally:
	    self.mutex_gsmanager.release()

    def ProcessData2Clients(self,sender,msg):
	self.client_manager.Send2Clients(msg)

    def GetPeerIDByGsID(self,gsid):
	peerid = -1
	self.mutex_gsmanager.acquire()
	try:
	    if self.gs2peer.has_key(gsid):
		peerid = self.gs2peer[gsid]
	finally:
	    self.mutex_gsmanager.release()

	if peerid == -1:
	    return False
	else:
	    return peerid

    def Send2GS(self,peerid,buf):
	fmt = '>i%ds' % (len(buf))
	SendData = struct.pack(fmt,len(buf),buf)
	rv = self.nserver.ns_sendmsg(peerid,SendData,len(SendData))
	if not rv:
	    PutLogList("(*) Send2GS gspeerid: %d failed,msg:%s !" % (peerid,buf),'',False)
	    return False
	return True

    def ProcessEnterGameResponse(self,sender,jsobj):
	pass
