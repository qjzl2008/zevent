class  Player(object):

    INIT_STATE = 0x00
    ENTERED_STATE = 0x01

    def __init__(self):
	#0 1 logined 2 entered
	self.state = self.INIT_STATE
	#hex
	self.peerid = -1
	self.character = None
	self.qobject = None
	self.active = False


