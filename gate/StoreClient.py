# -*- coding: utf-8 -*- 
import os
import threading
import simplejson as json
from GlobalConfig import GlobalConfig
from log import *
from uuid import uuid
from nclient import nc_arg_t,net_client_t,net_client
from NetMessages import Packets   

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
            from ClientManager import ClientManager
            from PlayerManager import PlayerManager
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

	    self.client_manager = ClientManager.instance()
	    self.player_manager = PlayerManager.instance()

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
		if obj['msg']['cmd'] == Packets.MSGID_REQUEST_NEWACCOUNT:
		    self.ProcessCreateAccountRes(obj)
		if obj['msg']['cmd'] == Packets.MSGID_REQUEST_LOGIN:
		    self.ProcessLoginRes(obj)
	    else:
		PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[1][4:]),
		    repr(message[1][4:])), Logfile.PACKETMS)
		return

	def Send2Store(self,message):
	    fmt = '>i%ds' % (len(message))
	    SendData = struct.pack(fmt,len(message),message)
	    rv = self.nclient.nc_sendmsg(SendData,len(SendData))

        def ProcessCreateAccountRes(self,obj):
	    code = obj['code']
	    if code != Packets.DEF_MSGTYPE_CONFIRM:
		msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_NEWACCOUNT,
			code)
		self.client_manager.Send2Player(obj['msg']['cid'],msg)
	    else:
		rv = obj['msg']['sqlout']['@rv']
		if rv != 0:
		    msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_NEWACCOUNT,
			    Packets.DEF_MSGTYPE_REJECT)
		    self.client_manager.Send2Player(obj['msg']['cid'],msg)
		else:
		    msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_NEWACCOUNT,
			    Packets.DEF_MSGTYPE_CONFIRM)
		    self.client_manager.Send2Player(obj['msg']['cid'],msg)

            return True

        def ProcessLoginRes(self,obj):
	    code = obj['code']
	    if code != Packets.DEF_MSGTYPE_CONFIRM:
		msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_LOGIN,
			code)
		self.client_manager.Send2Player(obj['msg']['cid'],msg)
	    else:
		rv = obj['msg']['sqlout']['@rv']
		if rv != 0:
		    msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_LOGIN,
			    Packets.DEF_MSGTYPE_REJECT)
		    self.client_manager.Send2Player(obj['msg']['cid'],msg)
		else:
		    self.player_manager.JoinNewPlayer(obj['msg']['cid'],
			    obj['msg']['sqlout']['@accountid'])
		    msg = '{"cmd":%d,"code":%d}' % (Packets.MSGID_RESPONSE_LOGIN,
			    Packets.DEF_MSGTYPE_CONFIRM)
		    self.client_manager.Send2Player(obj['msg']['cid'],msg)
            return True


