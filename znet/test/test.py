from time  import sleep,ctime
import thread 
import threading
import struct
import socket

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
			sendcount = 0
			recvcount = 0
			while(count < 10000):
			        buf = '{"cmd":1,"cnm":"zhousihai","pwd":"123456"}'
			        message = struct.pack('>i',len(buf)) + buf
				self.sock.send(message)
				sendcount += 1
			#	print "sendcount:%d" % sendcount
			        retmsg = self.sock.recv(4)
				nlen, = struct.unpack('>i',retmsg)
			#	print "nlen:%d" % nlen
				retmsg = self.sock.recv(nlen)
				recvcount += 1
			#	print "recvcount:%d" % sendcount
				#print "Login Res:",retmsg

				#self.sock.send("0123456789")
			        #rstr = self.sock.recv(10)
           			#print rstr
				count+=1
				#return
			#sleep(1)
			n+=1
			self.sock.close()
			print "return"
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
    #for cn in cnlist:
    #    cn.join()
