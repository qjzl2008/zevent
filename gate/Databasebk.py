from sqlalchemy.orm import mapper, relationship, sessionmaker, backref
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.exc import *
from sqlalchemy.sql import exists
from sqlalchemy.sql.expression import *
from sqlalchemy import (MetaData, Table, Column, DateTime, SmallInteger, Integer, 
	                                   BigInteger, Boolean,
					   String, ForeignKey, create_engine,
					   and_, or_)

import time, random, os, re, datetime
from log import PutLogFileList, PutLogList
from GlobalDef import DEF, Logfile
from sqlalchemy.orm.exc import NoResultFound

Base = declarative_base()

class Account(Base):
	__tablename__ = "account"
	AccountID = Column(BigInteger, primary_key = True)
	Name = Column(String(10), nullable = False, unique = True)
	Password = Column(String(10), nullable = False)
	Mail = Column(String(50), nullable = False)
	Quiz = Column(String(45))
	Answer = Column(String(20))
	SignupDate = Column(DateTime(), nullable = False)
	BlockDate = Column(DateTime())
	IpAddress = Column(String(15), nullable = False)
	
	def __init__(self, AccountID, name, password, mail, address):
		self.AccountID = AccountID
		self.Name = name
		self.Password = password
		self.Mail = mail
		self.SignupDate = datetime.datetime.now()
		self.IpAddress = address
		
	def __repr__(self):
		return "<User('%s', '%s', '%s')>" % (str(self.AccountID), self.Name, self.Password)
	@staticmethod
	def ByName(session, name):
		try:
			return session.query(Account).\
				filter(Account.Name == name).\
				one()
		except NoResultFound:
			return False
	@staticmethod
	def Match(session, name, pwd):
		try:
			return session.query(Account).\
				filter(Account.Name == name).\
				filter(Account.Password == pwd).\
				one()
		except NoResultFound:
			return False
		
	def Has(self, player_name):
		return filter(lambda ch: ch.CharName == player_name, self.CharList)
	def Find(self, player_name):
		return (lambda x: x[0] if x else False)(self.Has(player_name))

class DatabaseDriver:
        @classmethod
        def instance(cls,URL):
	    if not hasattr(cls, "_instance"):
		cls._instance = cls()
		cls._instance.Initialize(URL)
	    return cls._instance

        @classmethod
        def initialized(cls):
	    return hasattr(cls, "_instance")
	
	def Initialize(self, URL):
		self.engine = None
		self.Session = None
		try:
			self.engine = create_engine(URL, echo = False)
			Base.metadata.create_all(self.engine)
		except ArgumentError:
			return False
		self.Session = sessionmaker(bind = self.engine)
		return True

	def session(self):
		return self.Session()
