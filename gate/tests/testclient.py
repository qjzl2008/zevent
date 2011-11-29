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
        self.num = num

    def run(self):
        try:
		self.sock.connect(("127.0.0.1",8887))
		n = 0;
		while(True):
			count = 0
			while(count < 10):
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

			    #request enter gs
				buf = '{"cmd":%d,"gsid":1}' %\
					(Packets.MSGID_REQUEST_BINDGS)
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "bind gs:",retmsg
				count+=1
			#sleep(1)
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
    while i<100:
        cn = connector(i)
        cn.start()
        cnlist.append(cn)
        i = i + 1
    for cn in cnlist:
        cn.join()
