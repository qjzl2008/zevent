import os
import simplejson as json
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
		 PutLogList("(!) Cannot open configuration file.")
		 return False
	     self.config = json.load(open(self.cfg))

	     return True

	def GetValue(self,section,key):
	    if self.config == None:
		return False
	    return self.config[section][key]

