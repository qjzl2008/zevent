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
	self.lib = cdll.LoadLibrary(libname)

    def ns_start(self,ns_arg):
	self.lib.ns_start_daemon.restype = c_int
	self.lib.ns_start_daemon.argtypes = [POINTER(POINTER(net_server_t)),
		POINTER(ns_arg_t)]
	pns_arg = pointer(ns_arg_t())
	pns_arg.contents = ns_arg
	self.ns = pointer(pointer(net_server_t()))
	rv = self.lib.ns_start_daemon(self.ns,pns_arg)
	if rv < 0:
	    return False
	return True

    def ns_stop(self):
	self.lib.ns_stop_daemon.restype = c_int
	self.lib.ns_stop_daemon.argtypes = [POINTER(net_server_t)]

	self.lib.ns_stop_daemon(self.ns.contents)

    def ns_getpeeraddr(self,peer_id):
	self.lib.ns_getpeeraddr.restype = c_int
	self.lib.ns_getpeeraddr.argtypes = [POINTER(net_server_t),c_ulonglong,
		c_char_p]
        peer_ip = create_string_buffer(32)

	self.lib.ns_getpeeraddr(self.ns.contents,peer_id,peer_ip)
	return peer_ip.raw

    def ns_sendmsg(self,peer_id,msg,msg_len):
	self.lib.ns_sendmsg.restype = c_int
	self.lib.ns_sendmsg.argtypes = [POINTER(net_server_t),c_ulonglong,
		c_void_p,c_uint]

        p_msg = c_char_p(msg)
        pmsg = cast(p_msg,c_void_p)

	rv = self.lib.ns_sendmsg(self.ns.contents,peer_id,pmsg,msg_len)
	if rv < 0:
	    return False
	return True

    def ns_recvmsg(self,timeout):
	self.lib.ns_recvmsg.restype = c_int
	self.lib.ns_recvmsg.argtypes = [POINTER(net_server_t),POINTER(c_void_p),
		POINTER(c_uint),POINTER(c_ulonglong),c_uint]

	msg = c_char()
	pmsg = pointer(msg)
        pmsg = cast(pmsg,c_void_p)

	msg_len = c_uint(0)
	plen = pointer(msg_len)

	peerid = c_ulonglong(0)
	peer_id = pointer(peerid)
	rv = self.lib.ns_recvmsg(self.ns.contents,pmsg,plen,peer_id,timeout)
	if rv < 0:
	    return False
	elif rv == 0:
	    size = plen[0]
	    msg = string_at(pmsg.value,size)
	    return (rv,peer_id[0],msg,plen[0],pmsg)
	elif rv == 1:
	    return (rv,peer_id[0])

    def ns_tryrecvmsg(self):
	self.lib.ns_tryrecvmsg.restype = c_int
	self.lib.ns_tryrecvmsg.argtypes = [POINTER(net_server_t),POINTER(c_void_p),
		POINTER(c_uint),POINTER(c_ulonglong)]

	msg = c_char()
	pmsg = pointer(msg)
        pmsg = cast(pmsg,c_void_p)

	msg_len = c_uint(0)
	plen = pointer(msg_len)

	peerid = c_ulonglong(0)
	peer_id = pointer(peerid)
	rv = self.lib.ns_tryrecvmsg(self.ns.contents,pmsg,plen,peer_id)
	if rv < 0:
	    return False
	elif rv == 0:
	    size = plen[0]
	    msg = string_at(pmsg.value,size)
	    return (rv,peer_id[0],msg,plen[0],pmsg)
	elif rv == 1:
	    return (rv,peer_id[0])

    def ns_free(self,msg):
	rv = self.lib.ns_free(self.ns.contents,msg)

    def ns_disconnect(self,peer_id):
	self.lib.ns_disconnect.argtypes = [POINTER(net_server_t),c_ulonglong]
	rv = self.lib.ns_disconnect(self.ns.contents,peer_id)

if __name__ == "__main__":
    nserver = net_server("libznet.so")
    ns_arg = ns_arg_t()
    ns_arg.ip = "127.0.0.1"
    ns_arg.port = 8888
    nserver.ns_start(ns_arg)
    while True:
	msg = nserver.ns_recvmsg(1000000)
	if msg:
	    nserver.ns_sendmsg(msg[0],msg[1],msg[2])
            nserver.ns_getpeeraddr(msg[0])
	    nserver.ns_free(msg[3])
	    #nserver.ns_disconnect(msg[0])
    nserver.ns_stop()

