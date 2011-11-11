#!/usr/bin/env python
"""A non-blocking server."""

import errno
import functools
import ioloop
import iostream
import logging
import os
import socket
import threading
import time
import struct
from Queue import Queue
import msg

try:
    import fcntl
except ImportError:
    if os.name == 'nt':
        import win32_support as fcntl
    else:
        raise

class PyTCPServer(object):
    """A non-blocking server.
    """
    def __init__(self, request_callback, io_loop=None):
        """Initializes the server with the given request callback.
        """
	self.peers = {}
	self.die_peers = {}
	self.peerid = 0;
        self.request_callback = request_callback
        self.io_loop = io_loop
        self._socket = None
        self._started = False
	self.queue = Queue()
	self.InitNetLog()

    def InitNetLog(self):  
	logging.getLogger().setLevel(logging.DEBUG)  
        if not os.path.exists("Logs"):
	    os.mkdir("Logs")

	fh = logging.FileHandler("Logs/network-server.log")  
	fh.setLevel(logging.DEBUG)
	#ch = logging.StreamHandler()  
	#ch.setLevel(logging.INFO)  
	formatter = logging.Formatter("[%(asctime)s] - %(name)s - %(levelname)s - %(message)s")
	#ch.setFormatter(formatter)  
	fh.setFormatter(formatter) 
	logging.getLogger().addHandler(fh)  
	#logging.getLogger().addHandler(ch)

    def listen(self, port, address=""):
        """Binds to the given port and starts the server in a single process.

        This method is a shortcut for:

            server.bind(port, address)
            server.start(1)

        """
        self.bind(port, address)
        self.start(1)

    def bind(self, port, address=""):
        """Binds this server to the given port on the given IP address.
        """
        assert not self._socket
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        flags = fcntl.fcntl(self._socket.fileno(), fcntl.F_GETFD)
        flags |= fcntl.FD_CLOEXEC
        fcntl.fcntl(self._socket.fileno(), fcntl.F_SETFD, flags)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.setblocking(0)
        self._socket.bind((address, port))
        self._socket.listen(128)

    def start(self, num_processes=None):
        """Starts this server in the IOLoop.
        """
        assert not self._started
        self._started = True
        if not self.io_loop:
            self.io_loop = ioloop.IOLoop.instance()
        self.io_loop.add_handler(self._socket.fileno(),
		self._handle_events,
		ioloop.IOLoop.READ)

    def stop(self):
      self.io_loop.remove_handler(self._socket.fileno())
      self._socket.close()

    def _handle_events(self, fd, events):
        while True:
            try:
                connection, address = self._socket.accept()
            except socket.error, e:
                if e[0] in (errno.EWOULDBLOCK, errno.EAGAIN):
                    return
                raise
            try:
                stream = iostream.IOStream(connection, io_loop=self.io_loop)
                peer = PyTCPConnection(self,self.peerid, stream, address, 
			self.request_callback)
		self.peers[self.peerid] = peer
		self.peerid+=1
            except:
                logging.error("Error in connection callback", exc_info=True)
    def _putmsg(self,msg):
	self.queue.put(msg)

    def recvmsg(self,timeout = None):
	if self.queue.empty():
	    return None
	msg = self.queue.get()
	return msg
	
    def sendmsg(self,peerid,msg):
        try:
	    peer = self.peers[peerid]
	except:
	    return False
	if peer:
	    peer.write(msg)
	    return True
	return False

    def getpeeraddr(self,peerid):
	try:
	    peer = self.peers[peerid]
	except:
	    return None
	if peer:
	    return peer.address


class PyTCPConnection(object):
    """Handles a connection to an TCP client, executing TCP requests.
    """
    def __init__(self, tcpserver,peerid, stream, address, request_callback):
	self.tcpserver = tcpserver
	self.peerid = peerid
        self.stream = stream
        self.address = address
        self.request_callback = request_callback
        self.stream.read_bytes(4, self._on_headers)
	self.stream.set_close_callback(self.on_connection_close)

    def write(self, chunk):
        if not self.stream.closed():
            self.stream.write(chunk, self._on_write_complete)

    def close(self):
	if not self.stream.closed():
	    self.stream.close()

    def _on_write_complete(self):
        logging.warning("write_complete")
	pass

    def _on_headers(self, data):
	content_length, = struct.unpack(">i",data) 
	if content_length:
            if content_length > self.stream.max_buffer_size:
		raise Exception("Content-Length too long")
            self.stream.read_bytes(content_length, self._on_request_body)
            return

    def _on_request_body(self, data):
	message = msg.msg_t(self.peerid,data)
	if self.request_callback:
	    self.request_callback(message)
	else:
	    self.tcpserver._putmsg(message)
        self.stream.read_bytes(4, self._on_headers)

    def on_connection_close(self):
	del self.tcpserver.peers[self.peerid]
	self.tcpserver.die_peers[self.peerid] = self
	self.tcpserver.peers[self.peerid]=None;

