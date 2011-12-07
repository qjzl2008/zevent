# -*- coding: utf-8 -*- 
import socket, os, sys, select, struct, time, re, random, operator, datetime
import threading
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from collections import namedtuple
import simplejson as json 
from log import *
from sqlalchemy.exc import *
from NetMessages import Packets   
from uuid import uuid
from GlobalConfig import GlobalConfig
from StoreClient import StoreClient

fillzeros = lambda txt, count: (txt + ("\x00" * (count-len(txt))))[:count] 

class  Player(object):

    INIT_STATE = 0x00
    LOGINED_STATE = 0x01
    ENTERED_STATE = 0x02

    def __init__(self,accountid):
	self.accountid = accountid
	#0 1 logined 2 entered
	self.state = self.INIT_STATE
	self.gspeerid = -1

    def setstate(self,state):
	self.state = state

class PlayerManager(object):
	def __init__(self):
	    self.clients = {}
	    self.mutex_clients = threading.Lock()

	@classmethod
        def instance(cls):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")
		
	def Init(self):
            from ClientManager import ClientManager
	    self.gconfig = GlobalConfig.instance()
	    self.serverid = self.gconfig.GetValue('CONFIG','server-id')
	    self.uuid = uuid.instance()

	    self.clientmanager = ClientManager.instance()
	    self.nserver = self.clientmanager.nserver
	    self.storeclient = StoreClient.instance()
	    return True

	def obj2dict(self,obj):
	    memberlist = [m for m in dir(obj)]
	    _dict = {}
	    for m in memberlist:
		if m[0] != "_" and not callable(m):
		    _dict[m] = getattr(obj,m)
            return _dict
		
	def ProcessClientLogin(self, sender, jsobj):
	    sql = "call login('%s','%s',@rv,@accountid)"\
		    % (jsobj['cnm'],jsobj['pwd'])
	    cmd1 = Packets.MSGID_REQUEST_EXECPROC
	    cmd2 = Packets.MSGID_REQUEST_LOGIN
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "cid":%d,"sql":"%s",\
		    "sqlout":["@rv","@accountid"]}}'% (cmd1,cmd2,sender,
			    sql)
	    self.storeclient.Send2Store(buf)
	    return

	def ChangePassword(self, sender, buffer):
	    pass

	def CreateNewAccount(self, sender, jsobj):
	    uuid = self.uuid.gen_uuid(self.serverid,UUID_Type.ACCOUNT)

	    sql = "call create_account(%d,'%s','%s','%s',@rv)"\
		    % (uuid,jsobj['name'],jsobj['mail'],jsobj['pwd'])
	    cmd1 = Packets.MSGID_REQUEST_EXECPROC
	    cmd2 = Packets.MSGID_REQUEST_NEWACCOUNT
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "cid":%d,"sql":"%s",\
		    "sqlout":["@rv"]}}'% (cmd1,cmd2,sender,
			    sql)
	    self.storeclient.Send2Store(buf)
	    return
	
	def SendRes2Request(self,sender,cmd,code):
	    msg = '{"cmd":%d,"code":%d}' % (cmd,code)
	    fmt = '>i%ds' % (len(msg))
	    SendData = struct.pack(fmt,len(msg),msg)
	    self.nserver.ns_sendmsg(sender,SendData,len(SendData))

	def ProcessClientRequestBindGS(self,sender,jsobj):
	    gsid = jsobj['gsid']
            try:
		clientinfo = self.clients[sender]
	    except:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    if not clientinfo or self.clients[sender].state != \
		    Player.LOGINED_STATE:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    if clientinfo.gspeerid != -1:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

            rv = self.clientmanager.gsmanager.GetPeerIDByGsID(gsid)
	    if not rv:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,\
			Packets.DEF_MSGTYPE_REJECT)
		return False
	    else:
		clientinfo.gspeerid = rv
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,\
		    Packets.DEF_MSGTYPE_CONFIRM)
		return True

	def ProcessClientData2GS(self,sender,obj):
            try:
		clientinfo = self.clients[sender]
	    except:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_DATA2GS,\
		    Packets.DEF_MSGTYPE_REJECT)
		return False

	    if not clientinfo or self.clients[sender].state != \
		    Player.LOGINED_STATE:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_DATA2GS,\
		    Packets.DEF_MSGTYPE_REJECT)
		return False

	    gspeerid = clientinfo.gspeerid
	    if gspeerid == -1:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_DATA2GS,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    for msg in obj['msgs']:
		msg['msg']['peerid'] = sender
		msg['msg']['accountid'] = clientinfo.accountid
		buf = json.dumps(msg['msg'])
		self.clientmanager.gsmanager.Send2GS(gspeerid,buf)

	def ProcessClientDisConnect(self,sender):
            try:
		clientinfo = self.clients[sender]
	    except:
		return False

	    if not clientinfo or clientinfo.gspeerid == -1 or \
		    self.clients[sender].state != Player.LOGINED_STATE:
		return False
	    #notify gs
	    gspeerid = clientinfo.gspeerid
            msg = '{"cmd":%d,"peerid":%d}' % (Packets.MSGID_NOTIFY_DISCONNECT,sender)
	    self.clientmanager.gsmanager.Send2GS(gspeerid,msg)
	    return True

	def IsAccountInUse(self, AccountName):
	    return False

	def Send2Player(self,peerid,buf):
            fmt = '>i%ds' % (len(buf))
	    SendData = struct.pack(fmt,len(buf),buf)
	    rv = self.nserver.ns_sendmsg(peerid,SendData,len(SendData))

        def Send2Clients(self,obj):
	    for msg in obj['msgs']:
		buf = json.dumps(msg['msg'])
		peerid = msg['peerid']
		self.Send2Player(peerid,buf)
	    pass
	
	def JoinNewPlayer(self,cid,accountid):
	    player = Player(accountid)
            player.setstate(Player.LOGINED_STATE)

	    self.mutex_clients.acquire()
	    try:
		self.clients[cid] = player
	    finally:
		self.mutex_clients.release()


