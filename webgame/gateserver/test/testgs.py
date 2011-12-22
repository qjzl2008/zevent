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
		self.sock.connect(("127.0.0.1",8888))
		n = 0;
		while(True):
			count = 0
			while(count < 1):
			     #register gs
				cmd = Packets.MSGID_REQUEST_REGGS
				buf = '{"cmd":%d,"gsid":1,"scenes":[]}'% cmd
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)

			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "RegisterGS Res:",retmsg
				sleep(10000000)

			     #send data 2 clients
				cmd = Packets.MSGID_DATA2CLIENTS
				buf = '{"cmd":%d,"msgs":[{"cid":100,"msg":{"cmd":10,"x":100,"y":100}}]}'% cmd
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
                            
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
    while i<1:
        cn = connector(i)
        cn.start()
        cnlist.append(cn)
        i = i + 1
    for cn in cnlist:
        cn.join()
