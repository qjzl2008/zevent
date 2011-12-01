# -*- coding: utf-8 -*- 
import os
import threading
import random
import time
from Database import Character, Item, Skill, DatabaseDriver
from GlobalDef import DEF, Logfile, Version ,UUID_Type
from GateLogic import GateLogic
from scenemanager import SceneManager
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *

class WGServer:
    def __init__(self, nclient):
	self.nclient = nclient
        self.MaxTotalUsers = 1000
        self.WorldServerName = "WS1"
	self.gconfig = GlobalConfig.instance()

    def run(self):
	"""
            message = (rv,msg,len,void_pointer)
	"""
	while(True):
	    sleep = True
	    message = self.nclient.nc_recvmsg(1000000)
	    if message:
		if message[0] == 0:
		    self.ProcessMsg(message)
		    self.nclient.nc_free(message[3])
		else:
		    #disconneted to gate
		    PutLogList("(!) Disconnected to gate server!")
		    break;
	    continue

    def Init(self):
        """
        Loading main configuration, and initializing Database Driver
        (For now, its MySQL)
        """
	self.dbaddress = self.gconfig.GetValue('CONFIG','db')
	if not self.dbaddress:
	    return False

	PutLogList("(*) DB address : %s" % self.dbaddress,'',False)
	self.Database = DatabaseDriver.instance(self.dbaddress)
        if not self.Database:
	    PutLogList("(!) DatabaseDriver initialization fails!")
	    return False

	self.gatelogic = GateLogic.instance(self.nclient,self.Database)
	self.scmanager = SceneManager.instance(self.nclient)

	if not self.RegisterGS():
	    return False

	return True
    
    def RegisterGS(self):
        #register gs
	cmd = Packets.MSGID_REQUEST_REGGS
	buf = '{"cmd":%d,"gsid":%d}'% (cmd,self.gconfig.GetValue('CONFIG','gsid'))
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
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[1][4:]),
		repr(message[1][4:])), Logfile.PACKETMS)
	    return

	pass
    def MainLogic(self):
	self.scmanager.MainLogic()
	pass

