#!/usr/bin/env python
import ConfigParser as CP
import MySQLdb as DB
import binascii,os,sys

db_config = 'db.conf'

def crc_hash(key):
	return (binascii.crc32(key) >> 16) & 0x7fff

class DBOpr:
    def __init__(self,cfg_path='.'):
        self.db_connection = []
        self.sep = []
        self.total = 0
	self.dbnames = []
        parser=CP.ConfigParser()
        parser.read(cfg_path + os.sep + db_config)
        dbs = parser.sections()
	print dbs
	dblist = []
	for db in dbs:
		dblist.insert(parser.getint(db,'order'),db)
        for db in dblist:
            host_ = parser.get(db,'host')
            user_ = parser.get(db,'user')
            pwd_ = parser.get(db,'password')
            db_ = parser.get(db,'db')
            self.dbnames.append(db_)
            sep_ = parser.getint(db,'separator')
            try:
                print host_
                print user_
                print pwd_
                print db_
                conn = DB.connect(host=host_,user=user_,passwd=pwd_,db=db_,charset="utf8")
                print conn
            except DB.Error, e:
                print "Error %d: %s" % (e.args[0], e.args[1])
                for dbcon in self.db_connection:
                    dbcon.close()
                sys.exit(0)
            else:
                self.db_connection.append(conn)
                self.sep.append(sep_)

    def __del__(self):
        for conn in self.db_connection:
            conn.close()

    def select_db(self,user):
        conn_num  = len(self.db_connection)
        hash_value = crc_hash(str(user))
	conn_id = hash_value%conn_num
	print "db:",self.db_connection[conn_id],"db name:",self.dbnames[conn_id]
	return conn_id;

    def select_table(self,user):
	conn_id = self.select_db(user)
        hash_value = crc_hash(str(user))
	tableid = hash_value%self.sep[conn_id]
	print "db:",self.db_connection[conn_id],"db name:",self.dbnames[conn_id],"tableid:",tableid
        return (conn_id,tableid)
            
    def sql_exec(self,db,sql,flag):
	"""exec sql,flga(0:exec,1:get one row,,2:get multi row)"""
        set = None
        cursor = self.db_connection[db].cursor()
        try:
            while True:
		print sql
                cursor.execute(sql)
		if(flag == 1):
			set = cursor.fetchone()
		elif(flag == 2):
			set = cursor.fetchall()
                break
        except DB.Error,e:
            print "Error %d: %s" % (e.args[0], e.args[1])
            cursor.close()
	    if(flag != 0):
		    raise Exception("sql_exec query sql error!")
            return None

        cursor.close()
        self.db_connection[db].commit()
        return set

############################   test program
#-*- coding: utf8 -*-
if __name__ == '__main__':
    dbconn = DBOpr()
    del dbconn
    
