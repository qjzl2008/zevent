# -*- coding: utf-8 -*- 
import os
import threading
import random
import time
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from GateLogic import GateLogic
from scenemanager import SceneManager
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *
from nclient import nc_arg_t,net_client_t,net_client

class WGServer(threading.Thread):
    def __init__(self):
	threading.Thread.__init__(self, name="WGServer")
        self.MaxTotalUsers = 1000
        self.WorldServerName = "WS1"

    @classmethod
    def instance(cls):
	if not hasattr(cls, "_instance"):
	    cls._instance = cls()
	return cls._instance

    @classmethod
    def initialized(cls):
	return hasattr(cls, "_instance")

    def run(self):
	"""
            message = (rv,msg,len,void_pointer)
	"""
	delay = 20000 #时间片
	logic_cycles = 5 #5个时间片
	save_cycles = 500 #500时间片

        logic_count = 0
	save_count = 0
	wait = True
	while(True):
	    message = self.nclient.nc_recvmsg(delay)
	    if message:
		if message[0] == 0:
		    self.ProcessMsg(message)
		    self.nclient.nc_free(message[3])
		else:
		    #disconneted to gate
		    PutLogList("(!) Disconnected to gate server!")
		    break;

	    logic_count += 1
	    save_count += 1
	    if wait and (logic_count >= logic_cycles):
		logic_count = 0
		self.MainLogic()
	    if wait and (save_count >= save_cycles):
		save_count = 0
		self.SaveArchives()
	    continue

    def Init(self):
        """
        """
        self.gconfig = GlobalConfig.instance()
	znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	gate_ip = self.gconfig.GetValue('CONFIG','gate-server-address')
	gate_port = self.gconfig.GetValue('CONFIG','gate-server-port')
        
	self.nclient = net_client(znetlib)
	nc_arg = nc_arg_t()
	nc_arg.ip = gate_ip
	nc_arg.port = gate_port
	rv = self.nclient.nc_connect(nc_arg)
	if not rv:
	    PutLogList("(*) Connect to server IP:%s PORT:%d failed!" % (gate_ip,
		gate_port))
	    return False

	self.scmanager = SceneManager.instance()
	rv = self.scmanager.Init()
	if not rv:
	    return False

	self.gatelogic = GateLogic.instance()
	rv = self.gatelogic.Init()
	if not rv:
	    return False

	if not self.RegisterGS():
	    return False

	return True
    
    def RegisterGS(self):
        #register gs
	scenes = str(list(self.scmanager.scenes.keys()))
	cmd = Packets.MSGID_REQUEST_REGGS
	buf = '{"cmd":%d,"gsid":%d, "scenes":%s}'% (cmd,self.gconfig.GetValue('CONFIG','gsid'),scenes)
	msg = struct.pack('>i',len(buf)) + buf
	self.nclient.nc_sendmsg(msg,len(msg))
	#timeout 3seconds
	message = self.nclient.nc_recvmsg(3000)
	if message:
	    if message[0] == 0:
		try:
		    obj = json.loads(message[1][4:])
		except:
		    PutLogFileList("Packet len: %d b * %s" % (len(message[1][4:]),
			repr(message[1][4:])), Logfile.PACKETMS)
		    self.nclient.nc_free(message[3])
		    return False

	        if obj['cmd'] == Packets.MSGID_RESPONSE_REGGS:
		    if obj['code'] == Packets.DEF_MSGTYPE_CONFIRM:
		        self.nclient.nc_free(message[3])
		        PutLogList("(!) Registered to gate server sucessful!")
			return True
		    else:
		        self.nclient.nc_free(message[3])
			self.nclient.nc_disconnect()
			return False
		else:
		    return False
	    else:
		#disconneted to gate
		PutLogList("(!) Disconnected to gate server!")
		return False;

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

	if obj['cmd'] == Packets.MSGID_REQUEST_ENTERGAME:
	    self.gatelogic.ProcessClientRequestEnterGame(obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWCHARACTER:
	    self.gatelogic.CreateNewCharacter(obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_GETCHARLIST:
	    self.gatelogic.ProcessGetCharList(obj)
	elif obj['cmd'] == Packets.MSGID_NOTIFY_DISCONNECT:
	    self.gatelogic.ProcessClientDisconnect(obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_SYNPOS:
	    self.scmanager.ProcessSynPos(obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_ECHO:
	    self.gatelogic.ProcessEcho(obj)
	elif obj['cmd'] == Packets.MSGID_C2SNOTIFY_READY :
	    self.scmanager.ProcessC2SNotifyReady(obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_SWITCHSCENE :
	    self.scmanager.ProcessSwitchScene(obj)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[1][4:]),
		repr(message[1][4:])), Logfile.PACKETMS)
	    return

    def MainLogic(self):
	self.scmanager.MainLogic()

    def SaveArchives(self):
	self.scmanager.SaveArchives()


