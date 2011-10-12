#!/usr/bin/env python

#coding=utf-8
import sys 
import db_opr

def quotesql(text):
	return text.replace("\\", "\\\\").replace("'", "''").replace("%","%%")

class DBInit:
	def __init__(self):
		self.dbopr = db_opr.DBOpr();
	def __del__(self):
		del self.dbopr
	def init_data(self):
		db = 0;
		uid = 0;
		ret = 0;
		while (uid < 100000):
			sql = "INSERT INTO `game_user` (`uid`, `uname`) VALUES \
				(%d, 'zhousihai%d')" % (uid,uid);
			self.dbopr.sql_exec(db,sql,0);
			uid+=1;
		return ret;
	def run(self):
	    try:
		if(self.init_data() < 0):
		    print "recover role failed";
		    return -1;
	    except Exception, ex:
			print '%s' % (ex.message)
	    return 0;

################################# main program##################################
if __name__ == '__main__':
	print 'start initdb...'
	db_init = DBInit()
	db_init.run()
	del db_init
	print 'end initdb...'
