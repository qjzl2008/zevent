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
		self.sock.connect(("127.0.0.1",7766))
		n = 0;
		while(True):
			count = 0
			while(count < 1):
			    #test execsql
				cmd1 = Packets.MSGID_REQUEST_EXECSQL
				cmd2 = Packets.MSGID_REQUEST_ENTERGAME
				buf = '{"cmd":%d,"msg":{"cmd":%d,\
					"cid":%d,"sql":"%s"}}'% (cmd1,
						cmd2,5672894519046307842,
						"update account set IpAddress = '227.0.0.1'")
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "execsql Res:",retmsg

                            #test exec procedure
				cmd1 = Packets.MSGID_REQUEST_EXECPROC
				cmd2 = Packets.MSGID_REQUEST_ENTERGAME
				buf = '{"cmd":%d,"msg":{"cmd":%d,\
					"cid":%d,"sql":"%s",\
					"sqlout":["@rv1,@rv2"]}}'% (cmd1,
						cmd2,5672894519046307842,
						"call login_user(@rv1,@rv2)")
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "execproc Res:",retmsg
			     
			     #test query
				cmd1 = Packets.MSGID_REQUEST_QUERY
				cmd2 = Packets.MSGID_REQUEST_ENTERGAME
				buf = '{"cmd":%d,"msg":{"cmd":%d,\
					"cid":%d,"sql":"%s"}}'% (cmd1,
						cmd2,5672894519046307842,
						"select * from account")
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
				retmsg = self.sock.recv(nlen)
				print "query Res:",retmsg

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
