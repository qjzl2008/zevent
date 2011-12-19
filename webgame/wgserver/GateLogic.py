# -*- coding: utf-8 -*- 
import socket, os, sys, select, struct, time, re, random, operator, datetime
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from collections import namedtuple
import simplejson as json 
from log import *
from NetMessages import Packets   
from scenemanager import SceneManager
from uuid import uuid
from GlobalConfig import GlobalConfig
from Player import Player

#reload(sys)
#sys.setdefaultencoding('utf-8')

fillzeros = lambda txt, count: (txt + ("\x00" * (count-len(txt))))[:count] 

class GateLogic(object):
	def __init__(self):
	    pass
        @classmethod
        def instance(cls):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")
		
	def Init(self):
	    from wgserver import WGServer
	    from StoreClient import StoreClient
            self.gconfig = GlobalConfig.instance()
	    self.serverid = self.gconfig.GetValue('CONFIG','gsid')

	    self.uuid = uuid.instance()
	    self.scmanager = SceneManager.instance()
	    self.nclient = WGServer.instance().nclient
	    self.storeclient = StoreClient.instance()
	    return True

	def obj2dict(self,obj):
	    memberlist = [m for m in dir(obj)]
	    _dict = {}
	    for m in memberlist:
		if m[0] != "_" and not callable(m):
		    _dict[m] = getattr(obj,m)
            return _dict
	
	def CreateNewCharacter(self,jsobj):
	    sender = jsobj['peerid']
	    hexaccountid = jsobj['accountid']
	    accountid = self.uuid.hex2uuid(hexaccountid)
	    uuid = self.uuid.gen_uuid(self.serverid,UUID_Type.CHARACTER)
	    sql = "call create_character(%d,%d,%d,'%s',%d,@rv)"\
		    % (accountid,uuid,jsobj['professionid'],
			    jsobj['name'],jsobj['gender'])

	    cmd1 = Packets.MSGID_REQUEST_EXECPROC
	    cmd2 = Packets.MSGID_REQUEST_NEWCHARACTER
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":"%s",\
		    "sql":"%s",\
		    "sqlout":["@rv"]}}'% (cmd1,cmd2,sender,
			    sql)
            msg = buf.encode('utf-8')
	    self.storeclient.Send2Store(msg)
	    return True

	def ProcessGetCharList(self,jsobj):
	    sender = jsobj['peerid']
	    hexaccountid = jsobj['accountid']
	    accountid = self.uuid.hex2uuid(hexaccountid)
	    sql = "select * from `profession` A inner join(select * from `character` where accountid = %d order by level desc) B on A.professionid = B.professionid" % (accountid)

	    cmd1 = Packets.MSGID_REQUEST_QUERY
	    cmd2 = Packets.MSGID_REQUEST_GETCHARLIST
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":"%s",\
		    "sql":"%s"}}'% (cmd1,cmd2,sender,sql)
            msg = buf.encode('utf-8')
	    self.storeclient.Send2Store(msg)
	    return True

	def DeleteCharacter(self, sender, buffer):
	    pass

	def SendData2Clients(self,msgs):
	    message = '{"cmd":%d,"msgs":%s}' % (Packets.MSGID_REQUEST_DATA2CLIENTS,
		    msgs)
	    fmt = '>i%ds' % (len(message))
	    SendData = struct.pack(fmt,len(message),message)
	    rv = self.nclient.nc_sendmsg(SendData,len(SendData))
		
	def SendRes2Request(self,sender,cmd,code):
	    msg = '[{"peerid":"%s","msg":{"cmd":%d,"code":%d}}]' % (sender,cmd,code)
	    self.SendData2Clients(msg)

	def Send2Gate(self,message):
	    fmt = '>i%ds' % (len(message))
	    SendData = struct.pack(fmt,len(message),message)
	    rv = self.nclient.nc_sendmsg(SendData,len(SendData))
	
	def ProcessClientDisconnect(self,obj):
	    cid = self.scmanager.ProcessClientDisconnect(obj)
	    if cid:
		hexsender = obj['peerid']
#		sender = self.uuid.hex2uuid(hexsender)
		sql = "call leavegame(%d,@rv,@cid)"\
			% (cid)

		cmd1 = Packets.MSGID_REQUEST_EXECPROC
		cmd2 = Packets.MSGID_REQUEST_LEAVEGAME
		buf = '{"cmd":%d,"msg":{"cmd":%d,\
			"peerid":"%s",\
			"sql":"%s",\
			"sqlout":["@rv,@cid"]}}'% (cmd1,cmd2,hexsender,
				sql)
		msg = buf.encode('utf-8')
		self.storeclient.Send2Store(msg)
		return True
	    else:
		return False

	def ProcessClientRequestEnterGame(self, jsobj):
	    sender = jsobj['peerid']
	    hexaccountid = jsobj['accountid']
	    accountid = self.uuid.hex2uuid(hexaccountid)
	    hexcid = jsobj['cid']
	    cid = self.uuid.hex2uuid(hexcid)
	    sql = "call entergame(%d,%d,@rv,@uid,@cid)"\
		    % (accountid,cid)

	    cmd1 = Packets.MSGID_REQUEST_EXECPROC
	    cmd2 = Packets.MSGID_REQUEST_ENTERGAME
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":"%s",\
		    "sql":"%s",\
		    "sqlout":["@rv,@uid,@cid"]}}'% (cmd1,cmd2,sender,
			    sql)
            msg = buf.encode('utf-8')
	    self.storeclient.Send2Store(msg)
	    return True

	def ProcessEcho(self, jsobj):
	    sender = jsobj['peerid']
	    msg = jsobj['data'].encode("UTF-8")
	    buf = '[{"peerid":"%s","msg":"%s"}]' % (sender,msg)
	    self.SendData2Clients(buf)

	def ProcessRequestPlayerData(self, sender, buffer, GS):
	    pass

	def GetCharacterInfo(self, AccountName, AccountPassword, CharName):
	    pass

		
	def DecodeSavePlayerDataContents(self, buffer):
	    pass
		
	def IsAccountInUse(self, AccName):
	    pass
		
	def SavePlayerData(self, Header, Data):
	    pass
		
	def ProcessClientLogout(self, buffer, GS, bSave):
	    pass
				
	def GuildHandler(self, MsgID, buffer, GS):
	    pass
