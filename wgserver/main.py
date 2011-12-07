#!/usr/bin/env python
# -*- coding: utf-8 -*- 
import os
import simplejson as json
from GlobalConfig import GlobalConfig
from wgserver import WGServer
from log import *
from daemon import Daemon
from StoreClient import StoreClient

class MyDaemon(Daemon):
    def run(self):
	storeclient = StoreClient.instance()
	rv = storeclient.Init()
	if not rv:
	    sys.exit()
	
	wgserver = WGServer.instance()
	if not wgserver.Init():
	    sys.exit(0)
        storeclient.start()
	wgserver.start()

	wgserver.join()
	storeclient.join()

if __name__ == "__main__":
        daemon = MyDaemon('/tmp/wgserver.pid')

        if len(sys.argv) == 2:
                if 'start' == sys.argv[1]:
                        daemon.start()
                elif 'stop' == sys.argv[1]:
                        daemon.stop()
                elif 'restart' == sys.argv[1]:
                        daemon.restart()
                else:
                        print "Unknown command"
                        sys.exit(2)
                sys.exit(0)
        else:
                print "usage: %s start|stop|restart" % sys.argv[0]
                sys.exit(2)

