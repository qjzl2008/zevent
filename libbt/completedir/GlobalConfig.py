#coding=utf-8
import os
import json
class GlobalConfig(object):
	def __init__(self):
		"""
			Initializing globalconfig
		"""
		self.config = None
		self.cfg = "config.json"

        @classmethod
        def instance(cls):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
		cls._instance.Init()
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")
		
	def Init(self):
	     """
	     Parse main configuration file
	     """
	     if not os.path.exists(self.cfg) and not os.path.isfile(self.cfg):
		 return False
	     conf = open(self.cfg)
	     buf = conf.read()
	     self.config = json.loads(buf.decode('gbk'))
	     conf.close()

	     return True

	def GetValue(self,section,key):
	    if self.config == None:
		return False
	    return self.config[section][key]

