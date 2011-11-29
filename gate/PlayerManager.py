# -*- coding: utf-8 -*- 
import socket, os, sys, select, struct, time, re, random, operator, datetime
import threading
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from Database import Account, DatabaseDriver
from collections import namedtuple
import simplejson as json 
from log import *
from sqlalchemy.exc import *
from NetMessages import Packets   
from uuid import uuid
from GlobalConfig import GlobalConfig

#reload(sys)
#sys.setdefaultencoding('utf-8')

fillzeros = lambda txt, count: (txt + ("\x00" * (count-len(txt))))[:count] 

class  Player(object):

    INIT_STATE = 0x00
    LOGINED_STATE = 0x01
    ENTERED_STATE = 0x02

    def __init__(self,accountinfo = None):
	self.account = accountinfo
	#0 1 logined 2 entered
	self.state = self.INIT_STATE
	self.gsid = 0

    def setaccount(self,accountinfo):
	self.account = accountinfo
    def setstate(self,state):
	self.state = state

class PlayerManager(object):
	def __init__(self):
		"""
			Initializing login server
		"""
		self.clients = {}
		self.mutex_clients = threading.Lock()

		self.player2peer = {}
		self.mutex_peers = threading.Lock()

	        self.gconfig = GlobalConfig.instance()
		self.serverid = self.gconfig.GetValue('CONFIG','server-id')
	        self.dbaddress = self.gconfig.GetValue('CONFIG','db')
		self.uuid = uuid.instance()

        @classmethod
        def instance(cls,clientmanager,db):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
		cls._instance.clientmanager = clientmanager
		cls._instance.nserver = clientmanager.nserver
	        cls.Database = db
		cls._instance.dbsession = db.session()
		cls._instance.Init()
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")
		
	def Init(self):
	    return True
	def obj2dict(self,obj):
	    memberlist = [m for m in dir(obj)]
	    _dict = {}
	    for m in memberlist:
		if m[0] != "_" and not callable(m):
		    _dict[m] = getattr(obj,m)
            return _dict
		
	def ProcessClientLogin(self, sender, jsobj):
	    account = Account.ByName(self.dbsession, jsobj['cnm'])
	    if not account:
		PutLogList("(!) Account does not exists: %s" % jsobj['cnm'],
			Logfile.ERROR)
		msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_LOGIN,
		Packets.DEF_LOGRESMSGTYPE_NOTEXISTINGACCOUNT)
		fmt = '>i%ds' % (len(msg))
		SendData = struct.pack(fmt,len(msg),msg)
	        self.nserver.ns_sendmsg(sender,SendData,len(SendData))

	    elif account.Password != jsobj['pwd']:
		PutLogList("(!) Wrong Password: %s" % jsobj['cnm'], Logfile.ERROR)
		msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_LOG,
		Packets.DEF_LOGRESMSGTYPE_PASSWORDMISMATCH)
		fmt = '>i%ds' % (len(msg))
		SendData = struct.pack(fmt,len(msg),msg)
	        self.nserver.ns_sendmsg(sender,SendData,len(SendData))
	    else:
		PutLogList("(*) Login success: %s" % jsobj['cnm'])
		msg = '{"cmd":%d,"code":%d,"veru":%d,"verl":%d}' % (Packets.MSGID_RESPONSE_LOGIN,
		Packets.DEF_MSGTYPE_CONFIRM,Version.UPPER, Version.LOWER)
		fmt = '>i%ds' % (len(msg))
		SendData = struct.pack(fmt,len(msg),msg)
	        self.nserver.ns_sendmsg(sender,SendData,len(SendData))
		player = Player(account)
		player.setstate(Player.LOGINED_STATE)

		self.mutex_clients.acquire()
		try:
		    self.clients[sender] = player
		finally:
		    self.mutex_clients.release()

		self.mutex_peers.acquire()
		try:
		    self.player2peer[account.AccountID] = sender
		finally:
		    self.mutex_peers.release()

	def ChangePassword(self, sender, buffer):
	    pass
	
	def CreateNewAccount(self, sender, jsobj):
	    address = self.nserver.ns_getpeeraddr(sender)
	    uuid = self.uuid.gen_uuid(self.serverid,UUID_Type.ACCOUNT)
            new_account = Account(uuid,jsobj['name'],jsobj['pwd'],
		    jsobj['mail'],address[0])
	    self.dbsession.add(new_account)
	    try:
		    self.dbsession.commit()
	    except IntegrityError, errstr:
		    #print errstr
		    self.dbsession.rollback()
		    if "Duplicate entry" in str(errstr):
			self.SendRes2Request(sender,Packets.MSGID_RESPONSE_NEWACCOUNT,\
				Packets.DEF_MSGTYPE_REJECT)
			PutLogList("(!) Create account fails [ %s ]. Account already exists" % new_account.Name, Logfile.HACK)
			return
		    else:
			PutLogFileList(str(errstr), Logfile.MYSQL)
	    except OperationalError, errstr:
		    #print errstr
		    self.dbsession.rollback()
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_NEWACCOUNT,\
			    Packets.DEF_MSGTYPE_REJECT)
		    PutLogList("(!) Create account fails [ %s ]. Unknown error occured!" % new_account.Name, Logfile.HACK)
		    return

	    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_NEWACCOUNT,\
		    Packets.DEF_MSGTYPE_CONFIRM)
	    PutLogList("(*) Create account success [ %s/%s ]." % \
		    (new_account.Name, new_account.Mail))

	def SendRes2Request(self,sender,cmd,code):
	    msg = '{"cmd":%d,"code":%d}' % (cmd,code)
	    fmt = '>i%ds' % (len(msg))
	    SendData = struct.pack(fmt,len(msg),msg)
	    self.nserver.ns_sendmsg(sender,SendData,len(SendData))

	def ProcessClientRequestBindGS(self, sender, jsobj):
	    gsid = jsobj['gsid']
	    self.clientmanager.gsmanager.mutex_gsmanager.acquire()
	    gspeerid = -1;
	    try:
		gspeerid = self.clientmanager.gsmanager.gs2peer[gsid]
	    except:
		PutLogList("(!) gs ID:%d does not exists!" % gsid,
			Logfile.ERROR)
	    finally:
		self.clientmanager.gsmanager.mutex_gsmanager.release()
	    if gspeerid == -1:
	        self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,
			Packets.DEF_MSGTYPE_REJECT)
	    else:
		self.clients[sender].gsid = gsid
	        self.SendRes2Request(sender,Packets.MSGID_RESPONSE_BINDGS,
			Packets.DEF_MSGTYPE_CONFIRM)
	    pass

	def IsAccountInUse(self, AccountName):
	    return False

	def Send2Player(self,uid,buf):
	    peerid = -1
	    self.mutex_peers.acquire()
	    try:
		peerid = self.player2peer[uid]
	    except :
		PutLogList("(!) Peer does not exists for uid: %d" % uid,
			Logfile.ERROR)
	    finally:
		self.mutex_peers.release()
	    if peerid == -1:
		return

            fmt = '>i%ds' % (len(buf))
	    SendData = struct.pack(fmt,len(buf),buf)
	    rv = self.nserver.ns_sendmsg(peerid,SendData,len(SendData))
	    print "rv:%d" % (rv)
	    pass

        def Send2Clients(self,obj):
	    for msg in obj['msgs']:
		buf = json.dumps(msg['msg'])
		uid = msg['uid']
		self.Send2Player(uid,buf)
	    pass
