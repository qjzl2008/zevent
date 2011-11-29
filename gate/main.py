#!/usr/bin/env python
# -*- coding: utf-8 -*- 
import os
import simplejson as json
from GlobalConfig import GlobalConfig
from ClientManager import ClientManager
from GSManager import GSManager
from log import *
from daemon import Daemon
from nserver import ns_arg_t,net_server_t,net_server

class MyDaemon(Daemon):
    def run(self):
        self.gconfig = GlobalConfig.instance()
	self.znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	csip = self.gconfig.GetValue('CONFIG','clients-server-address')
	csport = self.gconfig.GetValue('CONFIG','clients-server-port')

	gsip = self.gconfig.GetValue('CONFIG','gs-server-address')
	gsport = self.gconfig.GetValue('CONFIG','gs-server-port')
	#run server for clients and game server
	nserver4clients = net_server(self.znetlib)
	nserver4gs = net_server(self.znetlib)
	ns_arg = ns_arg_t()
	ns_arg.ip = gsip
	ns_arg.port = gsport
	nserver4gs.ns_start(ns_arg)

	ns_arg.ip = csip
	ns_arg.port = csport
	nserver4clients.ns_start(ns_arg)

	client_manager = ClientManager("ClientManager",nserver4clients)
	gs_manager = GSManager("GSManager",nserver4gs)

	client_manager.Init(gs_manager)
	gs_manager.Init(client_manager)

	gs_manager.start()
	client_manager.start()

	client_manager.join()
	gs_manager.join()

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

