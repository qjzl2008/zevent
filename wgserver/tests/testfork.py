import os
import sys
import time
from testm import testmod
pid = os.fork() 
if pid > 0:
    os.wait()
    pid = os.fork()
    print "bbb"
    if pid > 0:
	sys.exit()
print os.getpid()

reload(testm)
mod = testmod()
mod.func()
time.sleep(100)
