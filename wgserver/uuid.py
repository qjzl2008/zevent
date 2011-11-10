import os
import struct
import sys
import time
#time  |  fudge  | svrid |  idtype 
#32bit    14bit     2bit     16bit 
class uuid(object):
    def __init__(self):
	self.time_last = 0
	self.fudge = 0
        self.MAX_PER_SECOND = 20000
	if os.path.isfile("state.dat"):
	    self.fobject = open("state.dat","r+")
	    self.time_last,self.fudge = struct.unpack("II",self.fobject.read(8))
	else:
	    self.fobject = open("state.dat","w+")

    @classmethod
    def instance(cls):
       if not hasattr(cls, "_instance"):
            cls._instance = cls()
       return cls._instance

    @classmethod
    def initialized(cls):
        return hasattr(cls, "_instance")

    def gen_uuid(self,svrid,idtype):
	self.fobject.seek(0)
	time_now = int(time.time())
	if self.time_last != time_now:
	    self.fudge = 0;
	    self.time_last = time_now
	else:
	    if self.fudge >= self.MAX_PER_SECOND - 1:
		return None
	    self.fudge += 1

	self.fobject.write(struct.pack("II",time_now,self.fudge))
	ntime = int(time_now)
	uuid = (ntime << 32) | (self.fudge << 18) | svrid << 30 | idtype
	return uuid

if __name__ == "__main__":  
    uuid = uuid.instance()
    while(True):
	guid = uuid.gen_uuid(1,2)
	print guid
