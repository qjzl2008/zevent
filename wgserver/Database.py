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

class Profession(Base):
        __tablename__ = "profession"
	ProfessionID = Column(BigInteger, primary_key = True)
	Name = Column(String(10),nullable = False, unique = True) 
	Profile = Column(String(20))
	Gender = Column(SmallInteger, default = 0)
	Strength = Column(Integer, default = 10)
	Intelligence = Column(Integer, default = 10)
	Magic = Column(Integer, default = 10)
	Appr = Column(Integer, default = 0)
	Vit = Column(Integer, default = 1)

	@staticmethod
	def ByID(session, ProfessionID):
		try:
			return session.query(Profession).\
				filter(Profession.ProfessionID == ProfessionID).\
				one()
		except NoResultFound:
			return False


class Character(Base):
	__tablename__ = "character"
	AccountID = Column(BigInteger, nullable = False)
	ProfessionID = Column(BigInteger, ForeignKey("profession.ProfessionID"))
	CharacterID = Column(BigInteger, primary_key = True)
	CharName = Column(String(10), nullable = False, unique = True)
	Level = Column(Integer, default = 1)
	Strength = Column(Integer, default = 10)
	Intelligence = Column(Integer, default = 10)
	Magic = Column(Integer, default = 10)
	Luck = Column(Integer, default = 0)
	Experience = Column(Integer, default = 0)
	Gender = Column(SmallInteger, default = 0)
	Appr = Column(Integer, default = 0)
	Scene = Column(BigInteger, default = 0)
	LocX = Column(SmallInteger, default = 0)
	LocY = Column(SmallInteger, default = 0)
	Profile = Column(String(255))
	CreateDate = Column(DateTime)
	LogoutDate = Column(DateTime)
	BlockDate = Column(DateTime)
	GuildName = Column(String(20))
	GuildID = Column(Integer, default = -1)
	GuildRank = Column(Integer, default = -1)
	FightNum = Column(SmallInteger, default = 0)
	FightDate = Column(SmallInteger, default = 0)
	FightTicket = Column(SmallInteger, default = 0)
	QuestNum = Column(SmallInteger, default = 0)
	QuestID = Column(SmallInteger, default = 0)
	QuestCount = Column(SmallInteger, default = 0)
	QuestRewType = Column(SmallInteger, default = 0)
	QuestRewAmmount = Column(SmallInteger, default = 0)
	QuestCompleted = Column(SmallInteger, default = 0)
	EventID = Column(Integer, default = 0)
	HP = Column(Integer, default = 0)
	MP = Column(Integer, default = 0)
	SP = Column(Integer, default = 0)
	PK = Column(Integer, default = 0)
	RewardGold = Column(Integer, default = 0)
	State = Column(SmallInteger, default = 0)
	
	@staticmethod
	def Exists(session, Name):
		try:
			return session.query(Character).\
				filter(Character.CharName == Name).\
				one()
		except NoResultFound:
			return False
		
	def __init__(self, AccountID, ProfessionID, CharID, CharName, Gender, Appr,Str, Int, Mag, Vit):
		# TODO : fix ApprX values!
		self.AccountID = AccountID
		self.ProfessionID = ProfessionID
		self.CharacterID = CharID
		self.CharName = CharName
		self.Gender = int(Gender)
		self.Strength = Str
		self.Intelligence = Int
		self.Magic = Mag
		self.Appr = int(Appr)
		self.HP = (Vit * 3) + (Str / 2) + 2
		self.MP = (Mag * 2) + (Int / 2) + 2
		self.SP = (Str * 2) + 2
		self.CreateDate = datetime.datetime.now()
		self.CreateDate = datetime.datetime.now()

	def __repr__(self):
		return "<Character(CharName = '%s', Level = '%d')>" % (self.CharName, self.Level)
	
	@staticmethod
	def ByID(session, chid):
		try:
			return session.query(Character).\
				filter(Character.CharacterID == chid).\
				one()
		except NoResultFound:
			return False
	@staticmethod
	def ByAccountID(session, accountid):
		try:
			return session.query(Character).\
				filter(Character.AccountID == accountid).\
				all()
		except NoResultFound:
			return False


	def AddItem(self, _ID, _Name, _LifeSpan, _Color, _Attr, _Equip, _X, _Y):
		self.Items.append(Item(Name = _Name,
								ItemID = _ID,
								Color = _Color,
								LifeSpan = _LifeSpan,
								Attribute = _Attr,
								Equip = _Equip,
								X = _X,
								Y = _Y))
	def Erase(self, sess):
		while len(self.Skills):
			sess.delete(self.Skills.pop(0))
		while len(self.Items):
			sess.delete(self.Items.pop(0))
		sess.delete(self)
		
	
class Item(Base):
	__tablename__ = "item"
	ID = Column(Integer, primary_key = True)
	CharID = Column(Integer, ForeignKey("character.CharacterID"))
	Name = Column(String(20), nullable = False)
	ItemID = Column(Integer, nullable = False)
	Count = Column(Integer, default = 1)
	Type = Column(Integer, default = 0)
	ID1 = Column(Integer, default = 0)
	ID2 = Column(Integer, default = 0)
	ID3 = Column(Integer, default = 0)
	Color = Column(Integer, default = 0)
	Effect1 = Column(Integer, default = 0)
	Effect2 = Column(Integer, default = 0)
	Effect3 = Column(Integer, default = 0)
	LifeSpan = Column(Integer, default = 0)
	Attribute = Column(Integer, default = 0)
	Equip = Column(Boolean, default = False)
	X = Column(Integer, default = 0)
	Y = Column(Integer, default = 0)
	Char = relationship(Character, backref = backref("Items"))
	
class Skill(Base):
	__tablename__ = "skill"
	ID = Column(Integer, primary_key = True)
	CharacterID = Column(Integer, ForeignKey("character.CharacterID"))
	SkillID = Column(Integer, default = 0)
	SkillMastery = Column(Integer, default = 0)
	SkillSSN = Column(Integer, default = 0)
	SkillChar = relationship(Character, backref = backref("Skills", order_by = ID))

"""class Guild(Base):
	ID = Column(Integer, primary_key = True)
	Name = Column(String(20))
	Char = 	"""

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
