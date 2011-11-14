from ctypes import *
import time
class ns_arg_t(Structure):
    pass
ns_arg_t._fields_ = [('ip',c_char * 32),
	('port',c_ushort),
	('max_peers',c_uint),
	('func',c_void_p)
	]

class net_server_t(Structure):
    pass

class net_server(object):

    def __init__(self,libname):
	cdll.LoadLibrary("libc.so.6")
	self.lib = cdll.LoadLibrary(libname)

    def ns_start(self,ns_arg):
	self.lib.ns_start_daemon.restype = c_int
	self.lib.ns_start_daemon.argtypes = [POINTER(POINTER(net_server_t)),
		POINTER(ns_arg_t)]
	pns_arg = pointer(ns_arg_t())
	pns_arg.contents = ns_arg
	self.ns = pointer(pointer(net_server_t()))
	rv = self.lib.ns_start_daemon(self.ns,pns_arg)
	print "rv:%d" % rv
	time.sleep(10000)

if __name__ == "__main__":
    nserver = net_server("libznet.so")
    ns_arg = ns_arg_t()
    ns_arg.ip = "127.0.0.1"
    ns_arg.port = 5555
    nserver.ns_start(ns_arg)

