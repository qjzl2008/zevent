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
from scenemanager import Player

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
	    accountid = jsobj['accountid']
	    uuid = self.uuid.gen_uuid(self.serverid,UUID_Type.CHARACTER)
	    sql = "call create_character(%d,%d,%d,'%s',%d,@rv)"\
		    % (accountid,uuid,jsobj['professionid'],
			    jsobj['name'],jsobj['gender'])

	    cmd1 = Packets.MSGID_REQUEST_EXECPROC
	    cmd2 = Packets.MSGID_REQUEST_NEWCHARACTER
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":%d,\
		    "sql":"%s",\
		    "sqlout":["@rv"]}}'% (cmd1,cmd2,sender,
			    sql)
            msg = buf.encode('utf-8')
	    self.storeclient.Send2Store(msg)
	    return True

	def ProcessGetCharList(self,jsobj):
	    sender = jsobj['peerid']
	    accountid = jsobj['accountid']
	    sql = "select * from `character` where accountid = %d order by level desc" % (accountid)

	    cmd1 = Packets.MSGID_REQUEST_QUERY
	    cmd2 = Packets.MSGID_REQUEST_GETCHARLIST
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":%d,\
		    "sql":"%s"}}'% (cmd1,cmd2,sender,sql)
            msg = buf.encode('utf-8')
	    self.storeclient.Send2Store(msg)
	    return True
	    sender = jsobj['peerid']
	    CharList = Character.ByAccountID(self.dbsession,jsobj['accountid'])

	    num = len(CharList)
	    i = 0
	    chars = ''
	    for Char in CharList:
		chars += '{"uid":%d,"cid":%d,"cnm":"%s","level":%d}' % (Char.AccountID,
			Char.CharacterID,
			Char.CharName,Char.Level)
		i += 1
		if i < num:
		    chars += ','

            msg = '['
	    msg += '{"cmd":%d,"code":%d,"num":%d}' % (Packets.MSGID_RESPONSE_GETCHARLIST,
		    Packets.DEF_MSGTYPE_CONFIRM,num)

	    if chars != '':
		msg += ','
		msg += chars

            msg += ']'
	    msg = msg.encode("UTF-8")

	    buf = '[{"peerid":%d,"msg":%s}]' % (sender,msg)
	    self.SendData2Clients(buf)

	def DeleteCharacter(self, sender, buffer):
	    pass

	def SendData2Clients(self,msgs):
	    message = '{"cmd":%d,"msgs":%s}' % (Packets.MSGID_REQUEST_DATA2CLIENTS,
		    msgs)
	    fmt = '>i%ds' % (len(message))
	    SendData = struct.pack(fmt,len(message),message)
	    rv = self.nclient.nc_sendmsg(SendData,len(SendData))
		
	def SendRes2Request(self,sender,cmd,code):
	    msg = '[{"peerid":%d,"msg":{"cmd":%d,"code":%d}}]' % (sender,cmd,code)
	    self.SendData2Clients(msg)

	def Send2Gate(self,message):
	    fmt = '>i%ds' % (len(message))
	    SendData = struct.pack(fmt,len(message),message)
	    rv = self.nclient.nc_sendmsg(SendData,len(SendData))

	def ProcessLeaveGame(self, sender):
	    try:
		clientinfo = self.clients[sender]
	    except:
		self.SendRes2Reuest(sender,Packets.MSGID_RESPONSE_LEAVEGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    rv = self.scmanager.ProcessLeaveGame(clientinfo.characterid)
	    if not rv:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_LEAVEGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    character = Character.ByID(self.dbsession, clientinfo.characterid)
	    if not character:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_LEAVEGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False
	    else:
		try:
		    #更新数据库状态角色在线
		    character.State = Player.INIT_STATE
		    self.dbsession.commit()
		except:
		    self.dbsession.rollback()
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_LEAVEGAME,\
			    Packets.DEF_MSGTYPE_REJECT)
		    return False
		
	    del self.clients[sender]
	    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_LEAVEGAME,\
		    Packets.DEF_MSGTYPE_CONFIRM)


	def ProcessClientDisconnect(self,obj):
	    cid = self.scmanager.ProcessClientDisconnect(obj)
	    if cid:
		character = Character.ByID(self.dbsession, cid)
		if character and character.State == Player.ENTERED_STATE:
		    character.State = Player.INIT_STATE
		    try:
			self.dbsession.commit()
		    except:
			self.dbsession.rollback()
	    return True

	def ProcessClientRequestEnterGame(self, jsobj):
	    sender = jsobj['peerid']
	    accountid = jsobj['accountid']
	    cid = jsobj['cid']
	    sql = "call entergame(%d,%d,@rv,@uid,@cid)"\
		    % (accountid,cid)

	    cmd1 = Packets.MSGID_REQUEST_EXECPROC
	    cmd2 = Packets.MSGID_REQUEST_ENTERGAME
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":%d,\
		    "sql":"%s",\
		    "sqlout":["@rv,@uid,@cid"]}}'% (cmd1,cmd2,sender,
			    sql)
            msg = buf.encode('utf-8')
	    self.storeclient.Send2Store(msg)
	    return True

	    sender = jsobj['peerid']
	    accountid = jsobj['accountid']
	    character = Character.ByID(self.dbsession, jsobj['cid'])
	    if not character:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False
	    else:
		if character.AccountID != accountid:
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			    Packets.DEF_MSGTYPE_REJECT)
		    return False

		if character.State == Player.ENTERED_STATE:
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			    Packets.DEF_MSGTYPE_REJECT)
		    return False

		try:
		    character = self.scmanager.ProcessEnterGame(sender,character)
		    if character:
			self.dbsession.commit()
		except:
		    if character:
			self.dbsession.rollback()
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			    Packets.DEF_MSGTYPE_REJECT)
		    return False

		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			Packets.DEF_MSGTYPE_CONFIRM)
		#notify gate 
		#msg = '{"cmd":%d,"code":0,"peerid":%d}' % \
		#	(Packets.MSGID_RESPONSE_ENTERGAME,sender)
		#self.Send2Gate(msg)
		return True
	
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
