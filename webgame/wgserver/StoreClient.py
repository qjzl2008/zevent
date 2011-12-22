# -*- coding: utf-8 -*- 
import os
import threading
import simplejson as json
from GlobalConfig import GlobalConfig
from log import *
from uuid import uuid
from nclient import nc_arg_t,net_client_t,net_client
from NetMessages import Packets   
from Character import Character

class StoreClient(threading.Thread):
        def __init__(self):
	    threading.Thread.__init__(self, name="StoreClient")

        @classmethod
        def instance(cls):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")

	def Init(self):
	    from GateLogic import GateLogic
	    from scenemanager import SceneManager

	    self.uuid = uuid.instance()

            self.gconfig = GlobalConfig.instance()
	    znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	    sqlip = self.gconfig.GetValue('CONFIG','sqlstore-address')
	    sqlport = self.gconfig.GetValue('CONFIG','sqlstore-port')
	    #connect to sqlstore
	    self.nclient = net_client(znetlib)
	    nc_arg = nc_arg_t()
	    nc_arg.ip = sqlip
	    nc_arg.port = sqlport
	    rv = self.nclient.nc_connect(nc_arg)
	    if not rv:
		PutLogList("(*) Connect to sqlstore IP:%s PORT:%d failed!"\
			% (sqlip,sqlport))
		return False

	    self.gatelogic = GateLogic.instance()
	    self.scene_manager = SceneManager.instance()
	    return True

	def run(self):
	    """
		message = (rv,msg,len,void_pointer)
	    """
	    while(True):
		sleep = True
		message = self.nclient.nc_recvmsg(0)
		if message:
		    if message[0] == 0:
			self.ProcessMsg(message)
			self.nclient.nc_free(message[3])
		    else:
			#disconneted to gate
			PutLogList("(!) Disconnected to gate server!")
			break;
		continue

	def ProcessMsg(self,message):
	    """
	     message = (peerid,msg,len,void_pointer)
	     void_pointer ns_free的参数 释放消息到底层网络库内存池
	    """
	    try:
		obj = json.loads(message[1][4:])
	    except:
		PutLogFileList("Packet len: %d b * %s" % (len(message[1][4:]),
		  repr(message[1][4:])), Logfile.PACKETMS)
	        return
	    
	    if obj['cmd'] == Packets.MSGID_RESPONSE_EXECPROC:
		if obj['msg']['cmd'] == Packets.MSGID_REQUEST_NEWCHARACTER:
                    self.ProcessCreateCharacterRes(obj)
		elif obj['msg']['cmd'] == Packets.MSGID_REQUEST_ENTERGAME:
		    self.ProcessEnterGameRes(obj)
		elif obj['msg']['cmd'] == Packets.MSGID_REQUEST_LEAVEGAME:
		    self.ProcessLeaveGameRes(obj)

            elif obj['cmd'] == Packets.MSGID_RESPONSE_QUERY:
		if obj['msg']['cmd'] == Packets.MSGID_REQUEST_GETCHARLIST:
		    self.ProcessGetCharListRes(obj)
		elif obj['msg']['cmd'] == Packets.MSGID_REQUEST_JOINSCENE:
		    self.ProcessJoinSceneRes(obj)
	    else:
		PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'],
		    len(message[1][4:]),repr(message[1][4:])), Logfile.PACKETMS)
		return

	def Send2Store(self,message):
	    fmt = '>i%ds' % (len(message))
	    SendData = struct.pack(fmt,len(message),message)
	    rv = self.nclient.nc_sendmsg(SendData,len(SendData))

        def ProcessCreateCharacterRes(self,obj):
	    code = obj['code']
	    if code != Packets.DEF_MSGTYPE_CONFIRM:
		self.gatelogic.SendRes2Request(obj['msg']['peerid'],
			Packets.MSGID_RESPONSE_NEWCHARACTER,
			Packets.DEF_MSGTYPE_REJECT)
	    else:
		rv = obj['msg']['sqlout']['@rv']
		if rv != 0:
		    self.gatelogic.SendRes2Request(obj['msg']['peerid'],
			    Packets.MSGID_RESPONSE_NEWCHARACTER,
			    Packets.DEF_MSGTYPE_REJECT)
		else:
		    self.gatelogic.SendRes2Request(obj['msg']['peerid'],
			    Packets.MSGID_RESPONSE_NEWCHARACTER,
			    Packets.DEF_MSGTYPE_CONFIRM)
            return True

	def ProcessGetCharListRes(self,obj):
	    sender = obj['msg']['peerid']
	    code = obj['code']
	    if code != Packets.DEF_MSGTYPE_CONFIRM:
		self.gatelogic.SendRes2Request(obj['msg']['peerid'],
			Packets.MSGID_RESPONSE_GETCHARLIST,
			Packets.DEF_MSGTYPE_REJECT)
	    else:
		CharList = obj['msg']['res']

		num = len(CharList)
		i = 0
		chars = ''
		for Char in CharList:
		    chars += '{"uid":"%s","pid":%d,"pnm":"%s","cid":"%s","cnm":"%s","level":%d}' %\
			    (self.uuid.uuid2hex(Char['AccountID']),
		             Char['ProfessionID'],
			     Char['PName'],
			    self.uuid.uuid2hex(Char['CharacterID']),
			    Char['CharName'],
			    Char['Level'])
		    i += 1
		    if i < num:
			chars += ','

		msg = '{"cmd":%d,"code":%d,"num":%d,"chars":[%s]}' % (Packets.MSGID_RESPONSE_GETCHARLIST,
			Packets.DEF_MSGTYPE_CONFIRM,num,chars)

		msg = msg.encode("UTF-8")

		buf = '[{"peerid":"%s","msg":%s}]' % (sender,msg)
		self.gatelogic.SendData2Clients(buf)

	def ProcessEnterGameRes(self,obj):
	    code = obj['code']
	    hexsender = obj['msg']['peerid']
	    if code != Packets.DEF_MSGTYPE_CONFIRM:
		self.gatelogic.SendRes2Request(hexsender,
			Packets.MSGID_RESPONSE_ENTERGAME,
			Packets.DEF_MSGTYPE_REJECT)
	    else:
		rv = obj['msg']['sqlout']['@rv']
		if rv != 0:
		    self.gatelogic.SendRes2Request(hexsender,
			    Packets.MSGID_RESPONSE_ENTERGAME,
			    Packets.DEF_MSGTYPE_REJECT)
		else:
		    accountid = obj['msg']['sqlout']['@uid']
		    charid = obj['msg']['sqlout']['@cid']
		    self.GetCharInfo(hexsender,accountid,charid,
			    Packets.MSGID_REQUEST_JOINSCENE)
            return True
	
	def GetCharInfo(self,hexsender,accountid,charid,cmd):
	    sql = "select * from `profession` A inner join(select * from `character` where accountid = %d and characterid = %d) B on A.professionid = B.professionid " % (accountid,charid)

	    cmd1 = Packets.MSGID_REQUEST_QUERY
	    cmd2 = cmd
	    buf = '{"cmd":%d,"msg":{"cmd":%d,\
		    "peerid":"%s",\
		    "sql":"%s"}}'% (cmd1,cmd2,hexsender,sql)
            msg = buf.encode('utf-8')
	    self.Send2Store(msg)
	    return True

	def SC_LeaveGame(self,hexsender,cid):
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
	    self.Send2Store(msg)
	    return True

        def ProcessJoinSceneRes(self,obj):
	    character = Character()
	    character.Init(obj['msg']['res'][0])
	    peerid = obj['msg']['peerid']
            rv = self.scene_manager.ProcessEnterGame(peerid,character)
	    if not rv:
		self.gatelogic.SendRes2Request(peerid,
			Packets.MSGID_RESPONSE_ENTERGAME,
			Packets.DEF_MSGTYPE_REJECT)
		cid = character.CharacterID
		self.SC_LeaveGame(peerid,cid)
	    else:
		msg = '{"cmd":%d,"code":%d,"sceneid":%d,"x":%d,"y":%d}' % \
			(Packets.MSGID_RESPONSE_ENTERGAME,
			Packets.DEF_MSGTYPE_CONFIRM,
			character.Scene,
			character.LocX,
			character.LocY)
		buf = '[{"peerid":"%s","msg":%s}]' % (peerid,msg)
		self.gatelogic.SendData2Clients(buf)
		#临时此处设置player状态为一切就绪等待接收场景帧
		self.scene_manager.SetPlayerReady(peerid)

	def ProcessLeaveGameRes(self,obj):
	    code = obj['code']
	    sender = obj['msg']['peerid']

	    if code != Packets.DEF_MSGTYPE_CONFIRM:
		PutLogList("(*) peerid: %s Leave Game failed!"\
			    % (sender))
	    else:
	        charid = obj['msg']['sqlout']['@cid']
		rv = obj['msg']['sqlout']['@rv']
		if rv != 0:
		    PutLogList("(*) CharID:%d Leave Game failed!"\
				% (charid))
		else:
		    pass
            return True
	
