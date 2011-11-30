#!/usr/bin/env python
# -*- coding: utf-8 -*- 
import os
import simplejson as json
from GlobalConfig import GlobalConfig
from wgserver import WGServer
from log import *
from daemon import Daemon
from nclient import nc_arg_t,net_client_t,net_client

class MyDaemon(Daemon):
    def run(self):
        self.gconfig = GlobalConfig.instance()
	self.znetlib = self.gconfig.GetValue('CONFIG','net-lib')
	self.ip = self.gconfig.GetValue('CONFIG','gate-server-address')
	self.port = self.gconfig.GetValue('CONFIG','gate-server-port')
        
	nclient = net_client(self.znetlib)
	nc_arg = nc_arg_t()
	nc_arg.ip = self.ip
	nc_arg.port = self.port
	rv = nclient.nc_connect(nc_arg)
	if not rv:
	    PutLogList("(*) Connect to server IP:%s PORT:%d failed!" % (self.ip,self.port))
	    sys.exit(0)
	wgserver = WGServer(nclient)
	if not wgserver.Init():
	    sys.exit(0)
	wgserver.run()

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

