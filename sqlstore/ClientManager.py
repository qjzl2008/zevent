# -*- coding: utf-8 -*- 
import os
import threading
import random
import time
import database
from GlobalDef import DEF, Logfile, Version ,UUID_Type
import simplejson as json
from NetMessages import Packets   
from GlobalConfig import GlobalConfig
from log import *

class ClientManager(object):
    def __init__(self, nserver):
	self.nserver = nserver
	self.gconfig = GlobalConfig.instance()

    def run(self):
	"""
            message = (rv,peerid,msg,len,void_pointer)
	"""
	while(True):
	    sleep = True
	    message = self.nserver.ns_recvmsg(0)
	    if message:
		if message[0] == 1:
		    PutLogList("(*) Client(ID:%d) disconnected" % message[1])
		    self.ProcessClientDisConnect(message[1])
		else:
		    self.ProcessMsg(message)
		    self.nserver.ns_free(message[4])
	    continue

    def Init(self):
        """
        Loading main configuration, and initializing Database Driver
        (For now, its MySQL)
        """
	self.dbaddr = self.gconfig.GetValue('CONFIG','dbaddr')
	if not self.dbaddr:
	    return False
	self.db = self.gconfig.GetValue('CONFIG','db')
	if not self.db:
	    return False
	self.dbuser = self.gconfig.GetValue('CONFIG','dbuser')
	if not self.dbuser:
	    return False
	self.dbpwd = self.gconfig.GetValue('CONFIG','dbpwd')
	if not self.dbpwd:
	    return False

        self.db = database.Connection(self.dbaddr, self.db,self.dbuser,self.dbpwd)
       	return True
   
    def ProcessMsg(self,message):
	"""
           message = (rv,peerid,msg,len,void_pointer)
	   void_pointer ns_free的参数 释放消息到底层网络库内存池
	"""
	try:
	    obj = json.loads(message[2][4:])
	except:
	    PutLogFileList("Packet len: %d b * %s" % (len(message[2][4:]),
		repr(message[2][4:])), Logfile.PACKETMS)
	    return

        peerid = message[1]
	if obj['cmd'] == Packets.MSGID_REQUEST_EXECSQL:
	    self.ProcessExecSQL(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_EXECPROC:
	    self.ProcessExecProc(peerid,obj)
	elif obj['cmd'] == Packets.MSGID_REQUEST_QUERY:
	    self.ProcessQuery(peerid,obj)
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj['cmd'], len(message[2][4:]),
		repr(message[2][4:])), Logfile.PACKETMS)
	    return

    def Send2Client(self,peerid,buf):
	fmt = '>i%ds' % (len(buf))
	SendData = struct.pack(fmt,len(buf),buf)
	rv = self.nserver.ns_sendmsg(peerid,SendData,len(SendData))
	if not rv:
	    PutLogList("(*) Send2Client peerid: %d failed,msg:%s !" % (peerid,buf),'',False)
	    return False
	return True

    def ProcessExecSQL(self,peerid,obj):
	msg = json.dumps(obj)
	try:
	    sql = obj['msg']['sql']
	except:
	    PutLogList("(*) ProcessExecSQL Failed,msg:%s !" % (msg),'',False)
	    return False

	rv = self.db.execute(sql)
	if not rv:
	    del obj['msg']['sql']
	    obj['cmd'] = Packets.MSGID_RESPONSE_EXECSQL
	    obj['code'] = Packets.DEF_MSGTYPE_REJECT
	    buf = json.dumps(obj)
	    self.Send2Client(peerid,buf)
	    PutLogList("(*) ProcessExecSQL Failed,msg:%s !" % (msg),'',False)
	    return False

	del obj['msg']['sql']
	obj['cmd'] = Packets.MSGID_RESPONSE_EXECSQL
	obj['code'] = Packets.DEF_MSGTYPE_CONFIRM
	buf = json.dumps(obj)
	self.Send2Client(peerid,buf)
	return True

    def ProcessExecProc(self,peerid,obj):
	msg = json.dumps(obj)
	try:
	    sql = obj['msg']['sql']
	except:
	    PutLogList("(*) ProcessExecProc Failed,msg:%s !" % (msg),'',False)
	    return False

	rv = self.db.execute(sql)
	if not rv:
	    del obj['msg']['sql']
	    obj['cmd'] = Packets.MSGID_RESPONSE_EXECPROC
	    obj['code'] = Packets.DEF_MSGTYPE_REJECT
	    buf = json.dumps(obj)
	    self.Send2Client(peerid,buf)
	    PutLogList("(*) ProcessExecProc Failed,msg:%s !" % (msg),'',False)
	    return False

	fields = ','.join(obj['msg']['sqlout'])
	sql = 'select %s' % fields
	res = None
	try:
	    res = self.db.get(sql)
	except:
	    PutLogList("(*) ProcessExecProc Failed,query sql:%s !" %\
		    (sql),'',False)

	del obj['msg']['sql']
	obj['cmd'] = Packets.MSGID_RESPONSE_EXECPROC
	obj['code'] = Packets.DEF_MSGTYPE_CONFIRM
	obj['msg']['sqlout'] = res
	buf = json.dumps(obj)
	self.Send2Client(peerid,buf)
	return True
    
    def ProcessQuery(self,peerid,obj):
	msg = json.dumps(obj)
	try:
	    sql = obj['msg']['sql']
	except:
	    PutLogList("(*) ProcessQuery Failed,msg:%s !" % (msg),'',False)
	    return False

	res = self.db.query(sql)
	if not res:
	    del obj['msg']['sql']
	    obj['cmd'] = Packets.MSGID_RESPONSE_QUERY
	    obj['code'] = Packets.DEF_MSGTYPE_REJECT
	    buf = json.dumps(obj)
	    self.Send2Client(peerid,buf)
	    PutLogList("(*) ProcessQuery Failed,msg:%s !" % (msg),'',False)
	    return False

	del obj['msg']['sql']
	obj['cmd'] = Packets.MSGID_RESPONSE_QUERY
	obj['code'] = Packets.DEF_MSGTYPE_CONFIRM
	obj['msg']['res'] = res
	buf = json.dumps(obj)
	self.Send2Client(peerid,buf)
	return True

    def ProcessClientDisConnect(self,peerid):
	pass

