from time  import sleep,ctime
import thread 
import threading
import socket

class connector(threading.Thread):
    def __init__(self,num):
        threading.Thread.__init__(self)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.num = num

    def run(self):
        try:
		self.sock.connect(("127.0.0.1",8899))
		n = 0;
		while(True):
			count = 0
			while(count < 30):
				self.sock.send("0123456789")
			        rstr = self.sock.recv(10)
				#print rstr
				count+=1
			sleep(0.1)
			n+=1
			#self.sock.close()
			#return
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
