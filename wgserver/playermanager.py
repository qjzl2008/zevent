# -*- coding: utf-8 -*- 
import socket, os, sys, select, struct, time, re, random, operator, datetime
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from Database import Account, Character, Profession, Item, Skill, DatabaseDriver
from collections import namedtuple
import simplejson as json 
from log import *
from sqlalchemy.exc import *
from NetMessages import Packets   
from scenemanager import SceneManager
from uuid import uuid
from GlobalConfig import GlobalConfig

#reload(sys)
#sys.setdefaultencoding('utf-8')

fillzeros = lambda txt, count: (txt + ("\x00" * (count-len(txt))))[:count] 

class  Player(object):

    INIT_STATE = 0x00
    LOGINED_STATE = 0x01
    ENTERED_STATE = 0x02

    def __init__(self,accountinfo = None,characterinfo = None):
	self.account = accountinfo
	#0 1 logined 2 entered
	self.state = self.INIT_STATE
	self.characterid = 0

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

	        self.gconfig = GlobalConfig.instance()
		self.serverid = self.gconfig.GetValue('CONFIG','server-id')
	        self.dbaddress = self.gconfig.GetValue('CONFIG','db')
		self.uuid = uuid.instance()

        @classmethod
        def instance(cls,nserver,db):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
		cls._instance.nserver = nserver
	        cls.Database = db
		cls._instance.dbsession = db.session()
		cls._instance.Init()
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")
		
	def Init(self):
	    self.scmanager = SceneManager.instance(self.nserver)
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
	        self.nserver.sendmsg(sender,SendData)

	    elif account.Password != jsobj['pwd']:
		PutLogList("(!) Wrong Password: %s" % jsobj['cnm'], Logfile.ERROR)
		msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_LOG,
		Packets.DEF_LOGRESMSGTYPE_PASSWORDMISMATCH)
		fmt = '>i%ds' % (len(msg))
		SendData = struct.pack(fmt,len(msg),msg)
	        self.nserver.sendmsg(sender,SendData)
	    else:
		PutLogList("(*) Login success: %s" % jsobj['cnm'])
		msg = '{"cmd":%d,"code":%d,"veru":%d,"verl":%d}' % (Packets.MSGID_RESPONSE_LOGIN,
		Packets.DEF_MSGTYPE_CONFIRM,Version.UPPER, Version.LOWER)
		fmt = '>i%ds' % (len(msg))
		SendData = struct.pack(fmt,len(msg),msg)
	        self.nserver.sendmsg(sender,SendData)
		player = Player(account)
		player.setstate(Player.LOGINED_STATE)
		self.clients[sender] = player

	def ChangePassword(self, sender, buffer):
	    pass
	
	def CreateNewCharacter(self, sender, jsobj):
	    ProfessionID = jsobj['professionid']
	    profession= Profession.ByID(self.dbsession, ProfessionID)
	    if not profession:
		PutLogList("(!) Profession does not exists: %d" % ProfessionID,
			Logfile.ERROR)
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_NEWCHARACTER,\
			Packets.DEF_MSGTYPE_REJECT)
	    else:
                player = self.clients[sender]
		accountid = player.account.AccountID
	        uuid = self.uuid.gen_uuid(self.serverid,UUID_Type.CHARACTER)
		new_character = Character(accountid,ProfessionID,
			uuid,jsobj['name'],jsobj['gender'],
			profession.Appr,profession.Strength,
			profession.Intelligence,profession.Magic,
			profession.Vit)

		self.dbsession.add(new_character)
		try:
			self.dbsession.commit()
		except IntegrityError, errstr:
			#print errstr
			self.dbsession.rollback()
			if "Duplicate entry" in str(errstr):
			    self.SendRes2Request(sender,
				    Packets.MSGID_RESPONSE_NEWCHARACTER,\
				    Packets.DEF_MSGTYPE_REJECT)
			    PutLogList("(!) Create character fails [ %s ]. Character already exists" % new_character.CharName, Logfile.HACK)
			    return
			else:
			    PutLogFileList(str(errstr), Logfile.MYSQL)
		except OperationalError, errstr:
			print errstr
			self.dbsession.rollback()
			self.SendRes2Request(sender,Packets.MSGID_RESPONSE_NEWACCOUNT,\
				Packets.DEF_MSGTYPE_REJECT)
			PutLogList("(!) Create character fails [ %s ]. Unknown error occured!" % new_character.CharName, Logfile.HACK)
			return

		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_NEWCHARACTER,\
			Packets.DEF_MSGTYPE_CONFIRM)
		PutLogList("(*) Create character success [ %s ]." % \
			(new_character.CharName))


	def ProcessGetCharList(self,sender):
	    account_instance = self.clients[sender].account
	    num = len(account_instance.CharList)
	    i = 0
	    chars = ''
	    for Char in account_instance.CharList:
		chars += '{"uid":%d,"cid":%d,"cnm":"%s","level":%d}' % (Char.AccountID,
			Char.CharacterID,
			Char.CharName,Char.Level)
		i += 1
		if i < num:
		    chars += ','

            msg = '['
            msg += '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_GETCHARLIST,
		    Packets.DEF_MSGTYPE_CONFIRM)

	    if chars != '':
		msg += ','
		msg += chars

            msg += ']'
	    msg = msg.encode("UTF-8")

	    fmt = '>i%ds' % (len(msg))
	    SendData = struct.pack(fmt,len(msg),msg)
	    self.nserver.sendmsg(sender,SendData)

	def DeleteCharacter(self, sender, buffer):
	    pass
		
	def CreateNewAccount(self, sender, jsobj):
	    address = self.nserver.getpeeraddr(sender)
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
	    self.nserver.sendmsg(sender,SendData)

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


	def ProcessClientRequestEnterGame(self, sender, jsobj):
	    try:
		clientinfo = self.clients[sender]
	    except:
		self.SendRes2Reqest(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    if not clientinfo or self.clients[sender].state != \
		    Player.LOGINED_STATE:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False

	    character = Character.ByID(self.dbsession, jsobj['cid'])
	    if not character:
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			Packets.DEF_MSGTYPE_REJECT)
		return False
	    else:
		if character.AccountID != clientinfo.account.AccountID:
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			    Packets.DEF_MSGTYPE_REJECT)
		    return False

		if character.State == Player.ENTERED_STATE or \
			self.clients[sender].state != Player.LOGINED_STATE:
			    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
				    Packets.DEF_MSGTYPE_REJECT)
			    return False

		try:
		    character = self.scmanager.ProcessEnterGame(character)
		    if character:
			self.dbsession.commit()
		except:
		    if character:
			self.dbsession.rollback()
		    self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			    Packets.DEF_MSGTYPE_REJECT)
		    return False

		self.clients[sender].characterid = character.CharacterID
		self.clients[sender].state = Player.ENTERED_STATE
		self.SendRes2Request(sender,Packets.MSGID_RESPONSE_ENTERGAME,\
			Packets.DEF_MSGTYPE_CONFIRM)
		return True
	
	def IsAccountInUse(self, AccountName):
	    return False

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
