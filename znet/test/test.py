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
            self.sock.send("test string")
            #rstr = self.sock.recv(BUFLEN)
            #print 'sent ',START
            #print 'received ',rstr
            #self.sock.send(USER+str(self.num))
            self.sock.close()
            return
        except socket.error,e:
            print e
            return

if __name__ == '__main__':
    cnlist = []
    i = 0
    while i<500:
        cn = connector(i)
        cn.start()
        cnlist.append(cn)
        i = i + 1
    for cn in cnlist:
        cn.join()
