# -*- coding: utf-8 -*- 
import os
import threading
from net import tcpserver
from net import ioloop
from net import msg
import random
import time
from Database import Account, Character, Item, Skill, DatabaseDriver
from playermanager import PlayerManager
import simplejson as json
from NetMessages import Packets   
from log import *

# Consumer thread

class WGServer(threading.Thread):
    def __init__(self, threadname,nserver):
	threading.Thread.__init__(self, name = threadname)
	self.nserver = nserver
        self.MaxTotalUsers = 1000
        self.WorldServerName = "WS1"

    def run(self):
	while(True):
	    message = self.nserver.recvmsg()
	    if message == None:
	    	continue
	    self.processmsg(message)

    def Init(self):
        """
        Loading main configuration, and initializing Database Driver
        (For now, its MySQL)
        """
        if not self.ReadProgramConfigFile("config.json"):
	    return False
	self.Database = DatabaseDriver()
	if not self.Database.Initialize(self.dbaddress):
	    PutLogList("(!) DatabaseDriver initialization fails!")
	    return False

	self.playermanager = PlayerManager.instance(self.nserver,self.Database)
	return True
		
    def ReadProgramConfigFile(self, cfg):
         """
         Parse main configuration file
         """
         if not os.path.exists(cfg) and not os.path.isfile(cfg):
	     PutLogList("(!) Cannot open configuration file.")
	     return False

	 config = json.load(open(cfg))
	 self.dbaddress = str(config['CONFIG']['db'])
	 PutLogList("(*) DB address : %s" % self.dbaddress,'',False)
	 return True
    
    def processmsg(self,message):
	print "Recv msg:%s" % (message.data)
	try:
	    obj = json.loads(message.data)
	except:
	    PutLogFileList("Packet len: %d b * %s" % (len(message.data),
		 repr(message.data)), Logfile.PACKETMS)
	    return

	if obj['cmd'] == Packets.MSGID_REQUEST_LOGIN:
	    self.playermanager.ProcessClientLogin(message.peerid,obj)
	    del message
	elif obj['cmd'] == Packets.MSGID_REQUEST_ENTERGAME:
	    self.playermanager.ProcessClientRequestEnterGame(message.peerid,obj)
	    del message
	elif obj['cmd'] == Packets.MSGID_REQUEST_LEAVEGAME:
	    self.playermanager.ProcessLeaveGame(message.peerid)
	    del message
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWACCOUNT:
	    self.playermanager.CreateNewAccount(message.peerid,obj)
	    del message
	elif obj['cmd'] == Packets.MSGID_REQUEST_NEWCHARACTER:
	    self.playermanager.CreateNewCharacter(message.peerid,obj)
	    del message
	elif obj['cmd'] == Packets.MSGID_REQUEST_GETCHARLIST:
	    self.playermanager.ProcessGetCharList(message.peerid)
	    del message
	else:
	    PutLogFileList("MsgID: (0x%08X) %db * %s" % (obj[0], len(message.data),
		 repr(message.data)), Logfile.PACKETMS)
	    return;

	    #self.nserver.sendmsg(msg.peerid,msg.data)
	    #print msg.peerid,msg.data,len(msg.data)
	pass


if __name__ == "__main__":  
    cfg = "config.json"
    if not os.path.exists(cfg) and not os.path.isfile(cfg):
	PutLogList("(!) Cannot open configuration file.")
	sys.exit()

    config = json.load(open(cfg))
    ip = config['CONFIG']['game-server-address']
    port = config['CONFIG']['game-server-port']

    nserver = tcpserver.PyTCPServer(None)
    nserver.listen(port,ip)
    wgserver = WGServer('Consumer', nserver)
    wgserver.Init()
    wgserver.start()
    ioloop.IOLoop.instance().start()

