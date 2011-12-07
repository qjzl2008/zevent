#!/usr/bin/env python
# -*- coding: utf-8 -*- 
import os
import simplejson as json
from GlobalConfig import GlobalConfig
from ClientManager import ClientManager
from GSManager import GSManager
from log import *
from daemon import Daemon
from StoreClient import StoreClient

class MyDaemon(Daemon):
    def run(self):
	storeclient = StoreClient.instance()
	rv = storeclient.Init()
	if not rv:
	    sys.exit()
	
	client_manager = ClientManager.instance()
	rv = client_manager.Init()
	if not rv:
	    sys.exit()
	
	gs_manager = GSManager.instance()
	rv = gs_manager.Init()
	if not rv:
	    sys.exit()

        storeclient.start()
	gs_manager.start()
	client_manager.start()

	client_manager.join()
	gs_manager.join()
	storeclient.join()

if __name__ == "__main__":
        daemon = MyDaemon('/tmp/gateserver.pid')

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

