#!/usr/bin/env python
# -*- coding: utf-8 -*- 
import os
import simplejson as json
from GlobalConfig import GlobalConfig
from ClientManager import ClientManager
from log import *
from daemon import Daemon
from nserver import ns_arg_t,net_server_t,net_server

class MyDaemon(Daemon):
    def run(self):
        self.gconfig = GlobalConfig.instance()
	self.znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	ip = self.gconfig.GetValue('CONFIG','sqlstore-ip')
	port = self.gconfig.GetValue('CONFIG','sqlstore-port')

	#run server for clients and game server
	nserver = net_server(self.znetlib)
	ns_arg = ns_arg_t()
	ns_arg.ip = ip
	ns_arg.port = port
	nserver.ns_start(ns_arg)

	client_manager = ClientManager(nserver)

	client_manager.Init()
	client_manager.run()

if __name__ == "__main__":
        daemon = MyDaemon('/tmp/sqlstore.pid')

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

