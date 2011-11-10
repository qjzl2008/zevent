# -*- coding: utf-8 -*- 
import os, time, struct, sys
from GlobalDef import DEF, Logfile
reload(sys)
sys.setdefaultencoding('utf-8')


def PutLogFileList(buffer, sLogName, bIsPacket = False):
	if bIsPacket:
		fmt = "<h"
		s = struct.unpack(fmt, buffer[:struct.calcsize(fmt)])
		MsgType = s[0]
		buffer = buffer[struct.calcsize(fmt):]
	
	if len(buffer) > DEF.MAXLOGLINESIZE or len(buffer) == 0:
		return
			
	if sLogName == '':
		sLogName = Logfile.EVENTS
		
	sFileName = ''
	if sLogName == Logfile.GM:
		sFileName = '%s/%s/GM-Event-%s.txt' % (Logfile.BASE, Logfile.GM, time.strftime("%Y-%m-%d"))
	if sLogName == Logfile.ITEM:
		sFileName = '%s/%s/Item-Event-%s.txt' % (Logfile.BASE, Logfile.ITEM, time.strftime("%Y-%m-%d"))
	if sLogName == Logfile.CRUSADE:
		sFileName = '%s/%s/Crusade-Event-%s.txt' % (Logfile.BASE, Logfile.CRUSADE, time.strftime("%Y-%m-%d"))
	if sLogName == Logfile.EVENTS:
		sFileName = '%s/Events.txt' % (Logfile.BASE)
	if sFileName == '':
		sFileName = '%s/%s' % (Logfile.BASE, sLogName)
		
	if not os.path.isdir(os.path.dirname(sFileName)):
		os.makedirs(os.path.dirname(sFileName))
		
	FileHandle = open(sFileName, 'a')
	try:
		FileHandle.write("%s - %s\n" % (time.strftime("%Y:%m:%d:%H:%M"), buffer))
	finally:
		FileHandle.close()

def PutLogList(text, sLogName = '', Echo = True):
        text = text.encode("UTF-8")
	if Echo:
	    sys.stdout.write(text + "\n")
	    PutLogFileList(text, Logfile.EVENTS, False)
	#if sLogName != '':
	PutLogFileList(text, sLogName)
