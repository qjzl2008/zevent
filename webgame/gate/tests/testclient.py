# -*- coding: utf-8 -*-       
from time  import sleep,ctime
import thread 
import threading
import socket
import struct
from NetMessages import Packets 

class connector(threading.Thread):
    def __init__(self,num):
        threading.Thread.__init__(self)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	self.sock.connect(("127.0.0.1",8887))
    def run(self):
        try:
		n = 0;
		while(True):
			count = 0
			while(count < 1):
			     #create account
				buf = '{"cmd":8,"name":"zhousihai","pwd":"123456",\
					"mail":"zhousihai@126.com"}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)

			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "CreateAccount Res:",retmsg
                            
			    #login
			        buf = '{"cmd":1,"cnm":"zhousihai","pwd":"123456"}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "Login Res:",retmsg

			    #request bind gs
				buf = '{"cmd":%d,"gsid":1}' %\
					(Packets.MSGID_REQUEST_BINDGS)
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "bind gs Res:",retmsg
			    #send createcharacter
				cmd1 = Packets.MSGID_REQUEST_DATA2GS
				cmd2 = Packets.MSGID_REQUEST_NEWCHARACTER
				buf = '{"cmd":%d,"msgs":[{"gsid":1,"msg":{"cmd":%d,\
					"professionid":1,\
					"name":"1周霸姐","gender":0}}]}'% (cmd1,cmd2)
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "create character Res:",retmsg

#			    #send listcharacter
				cmd1 = Packets.MSGID_REQUEST_DATA2GS
				cmd2 = Packets.MSGID_REQUEST_GETCHARLIST
				buf = '{"cmd":%d,"msgs":[{"gsid":1,"msg":{"cmd":%d}}]}'% (cmd1,cmd2)
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "list characters Res:",retmsg
			    #send entergame
				cmd1 = Packets.MSGID_REQUEST_DATA2GS
				cmd2 = Packets.MSGID_REQUEST_ENTERGAME
				buf = '{"cmd":%d,"msgs":[{"gsid":1,"msg":{"cmd":%d,\
					"cid":"%s"}}]}'% (cmd1,cmd2,"4ed5f09040000001")
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "enter game Res:",retmsg
#
#			    #send echo
			        num = 0
				while num < 1:
				    cmd1 = Packets.MSGID_REQUEST_DATA2GS
				    cmd2 = Packets.MSGID_REQUEST_ECHO
				    buf = '{"cmd":%d,"msgs":[{"gsid":1,"msg":{"cmd":%d,\
					    "data":"%s"}}]}'% (cmd1,cmd2,"test echo")
				    message = struct.pack('>i',len(buf)) + buf
				    self.sock.send(message)
				    retmsg = self.sock.recv(4)
				    nlen, = struct.unpack('>i',retmsg)
				    retmsg = self.sock.recv(nlen)
				    #print "echo Res:",retmsg
				    num = num + 1
				count+=1

			n+=1
			self.sock.close()
			print "close"
			return
        except socket.error,e:
            print e
            return

if __name__ == '__main__':
    cnlist = []
    i = 0
    while i<1:
        cn = connector(i)
        cn.start()
        cnlist.append(cn)
        i = i + 1
    for cn in cnlist:
        cn.join()
