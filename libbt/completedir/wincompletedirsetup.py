#!/usr/bin/env python

# Written by Bram Cohen
# see LICENSE.txt for license information

from distutils.core import setup
import py2exe

options = {'py2exe':{'compressed':1,'optimize':2,'bundle_files':1}}
setup(name='completedir',options=options,zipfile=None,
		windows=[{'script':'btcompletedirgui.py','icon_resources':[(1,'icon_bt.ico')]}])
