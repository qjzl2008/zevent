# -*- coding: utf-8 -*-       
from time  import sleep,ctime
import thread 
import threading
import socket
import struct

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
			while(count < 1000):
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

			     #create character
				buf = '{"cmd":16,"professionid":5670219206962356229,\
					"name":"无敌霸姐","gender":0}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)

			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "CreateCharacter Res:",retmsg

			     #list character
				buf = '{"cmd":17}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)

			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "ListCharacter Res:",retmsg
                            #entergame
			        buf = '{"cmd":2,"cid":5672894519046307842}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)

			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "EnterGame Res:",retmsg

			     #leavegame
			        buf = '{"cmd":4}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)

			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "LeaveGame Res:",retmsg

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
