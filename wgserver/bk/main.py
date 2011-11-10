import errno
import time
import functools
import ioloop
import socket,logging

def handle_connection(connection,address):
    connection.close()
    return;

def timeout_cb():
    print "timeout"
    io_loop = ioloop.IOLoop.instance()
    timeout_callback = functools.partial(timeout_cb);
    io_loop.add_timeout(time.time()+10,timeout_callback)
    return;

def connection_ready(sock, fd, events):
    while True:
	try:
	    connection, address = sock.accept()
	except socket.error, e:
	    if e[0] not in (errno.EWOULDBLOCK, errno.EAGAIN):
		raise
	    return
	connection.setblocking(0)
	handle_connection(connection, address)

logger = logging.getLogger("network-server")  
  
def InitLog():  
    logger.setLevel(logging.DEBUG)  
    fh = logging.FileHandler("network-server.log")  
    fh.setLevel(logging.DEBUG)
    ch = logging.StreamHandler()  
    ch.setLevel(logging.INFO)  
    formatter = logging.Formatter("[%(asctime)s] - %(name)s - %(levelname)s - %(message)s")
    ch.setFormatter(formatter)  
    fh.setFormatter(formatter) 
    logger.addHandler(fh)  
    logger.addHandler(ch)

################################# main program##################################
if __name__ == "__main__":  
    InitLog() 
    logger.info("starting server...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setblocking(0)
    sock.bind(("", 7676))
    sock.listen(128)

    io_loop = ioloop.IOLoop.instance()
    callback = functools.partial(connection_ready, sock)
    timeout_callback = functools.partial(timeout_cb)
    io_loop.add_handler(sock.fileno(), callback, io_loop.READ)
    io_loop.add_timeout(time.time()+3,timeout_callback)
    io_loop.start()

