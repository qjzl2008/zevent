from ctypes import *
import time
class nc_arg_t(Structure):
    pass
nc_arg_t._fields_ = [('ip',c_char * 32),
	('port',c_ushort),
	('timeout',c_int),
	('func',c_void_p)
	]

class net_client_t(Structure):
    pass

class net_client(object):

    def __init__(self,libname):
	self.lib = cdll.LoadLibrary(libname)

    def nc_connect(self,nc_arg):
	self.lib.nc_connect.restype = c_int
	self.lib.nc_connect.argtypes = [POINTER(POINTER(net_client_t)),
		POINTER(nc_arg_t)]
	pnc_arg = pointer(nc_arg_t())
	pnc_arg.contents = nc_arg
	self.nc = pointer(pointer(net_client_t()))
	rv = self.lib.nc_connect(self.nc,pnc_arg)
	if rv < 0:
	    return False
	return True

    def nc_disconnect(self):
	self.lib.nc_disconnect.restype = c_int
	self.lib.nc_disconnect.argtypes = [POINTER(net_client_t)]

	self.lib.nc_disconnect(self.nc.contents)

    def nc_sendmsg(self,msg,msg_len):
	self.lib.nc_sendmsg.restype = c_int
	self.lib.nc_sendmsg.argtypes = [POINTER(net_client_t),
		c_void_p,c_uint]

        p_msg = c_char_p(msg)
        pmsg = cast(p_msg,c_void_p)

	rv = self.lib.nc_sendmsg(self.nc.contents,pmsg,msg_len)
	if rv < 0:
	    return False
	return True

    def nc_recvmsg(self,timeout):
	self.lib.nc_recvmsg.restype = c_int
	self.lib.nc_recvmsg.argtypes = [POINTER(net_client_t),POINTER(c_void_p),
		POINTER(c_uint),c_uint]

	msg = c_char()
	pmsg = pointer(msg)
        pmsg = cast(pmsg,c_void_p)

	msg_len = c_uint(0)
	plen = pointer(msg_len)

	rv = self.lib.nc_recvmsg(self.nc.contents,pmsg,plen,timeout)
	if rv < 0:
	    return False
	elif rv == 0:
	    size = plen[0]
	    msg = string_at(pmsg.value,size)
	    return (rv,msg,plen[0],pmsg)
	elif rv == 1:
	    return (rv,)

    def nc_tryrecvmsg(self):
	self.lib.nc_tryrecvmsg.restype = c_int
	self.lib.nc_tryrecvmsg.argtypes = [POINTER(net_client_t),POINTER(c_void_p),
		POINTER(c_uint)]

	msg = c_char()
	pmsg = pointer(msg)
        pmsg = cast(pmsg,c_void_p)

	msg_len = c_uint(0)
	plen = pointer(msg_len)

	rv = self.lib.nc_tryrecvmsg(self.nc.contents,pmsg,plen)
	if rv < 0:
	    return False
	elif rv == 0:
	    size = plen[0]
	    msg = string_at(pmsg.value,size)
	    return (rv,msg,plen[0],pmsg)
	elif rv == 1:
	    return (rv,)

    def nc_free(self,msg):
	rv = self.lib.nc_free(self.nc.contents,msg)

if __name__ == "__main__":
    import sys
    import struct
    nclient = net_client("libznet.so")
    nc_arg = nc_arg_t()
    nc_arg.ip = "127.0.0.1"
    nc_arg.port = 8887
    nc_arg.timeout = 3
    rv = nclient.nc_connect(nc_arg)
    if not rv:
	sys.exit(0)

    buf = '{"cmd":1,"cnm":"zhousihai","pwd":"123456"}'
    message = struct.pack('>i',len(buf)) + buf

    nclient.nc_sendmsg(message,len(message))
    while True:
	msg = nclient.nc_recvmsg(0)
	if msg:
	    #nserver.ns_sendmsg(msg[0],msg[1],msg[2])
	    if msg[0] == 0:
		nclient.nc_free(msg[3])
	    else:
		break
    nclient.nc_disconnect()

