#!/usr/bin/python

# parses the event log for griefing events, keeps database of users

# requires: sqlite

# Copyright (C) 2007, Manuel Moos

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 or version 3 of the
# License.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# the GPL is not included into this distribution in its full text because that would
# convey that the whole of DedCon is covered by it, which it isn't. In case you have
# no copy of the GPL at hand, here are the important points:
# - as long as you don't pass on the software in an altered form, you may do with
#   it whatever you like.
# - you are free to modify and redistribute the software in source form, provided this
#   copyright and license notice stays intact and you grant the recipient at least the
#   rights the GPL gives them. You may of course append your own copyright statement.
# - you can also distribute modified versions in binary form provided you give the
#   recipient also access to the modified source.

# This little program reads event logs from stdin and counts how many players were
# using each version of DEFCON.
 
# usage: versionstat.py
# the event log is expected on stdin.

#  versionstat.db: database used to collect stats

# This file is mainly for internal use and probably not useful for anyone, hence no further
# documentation.

import sqlite_wrapper

import os
import sys
import string
import ConfigParser
import time
import codecs

# connect to database
connection = sqlite_wrapper.connect( "versionstat.db", timeout=120.0 )
cursor = connection.cursor()

# create tables if they don't exist yet
try:
    # the table of all players and their versions
    cursor.execute('CREATE TABLE players ( keyID VARCHAR(20), version VARCHAR(20), IP VARCHAR(20) )')
    cursor.execute('CREATE INDEX versionindex ON players (version)')
    cursor.execute('CREATE INDEX ipindex ON players (IP)')

    cursor.execute('CREATE TABLE combined ( combined PRIMARY KEY )')

    # table of versions
    cursor.execute('CREATE TABLE versions ( version VARCHAR(20) PRIMARY KEY )')


    print "Database created."
except sqlite_wrapper.DatabaseError:
    pass

connection.commit()

# parse the event log
for line in sys.stdin:
    # convert from latin1 (Defcon and DedCon's charset) to utf8 and split it
    lineSplit = unicode(line, "latin1" ).split()[1:]

    if len(lineSplit) == 0:
        break
    
    # parse new clients
    if lineSplit[0] == 'CLIENT_NEW':
        ID      = lineSplit[1]
        keyID   = lineSplit[2]
        IP      = lineSplit[3]
        version = string.join(lineSplit[4:])

        # just insert the player and ignore errors
        combined = keyID + " " + version
        cursor.execute ("SELECT * FROM combined where combined = ?", ( combined, ) )
        if len( cursor.fetchall() ) == 0:
            cursor.execute ("SELECT * FROM players where IP = ? and version = ?", ( IP, version ) )
            if len( cursor.fetchall() ) == 0:
                cursor.execute("INSERT INTO combined VALUES( ? )", ( combined, ) )
                cursor.execute("INSERT INTO players VALUES( ?, ?, ? )", ( keyID, version, IP ) )
            
        cursor.execute ("SELECT * FROM versions where version = ?", ( version, ) )
                
        if len( cursor.fetchall() ) == 0:
            cursor.execute("INSERT INTO versions VALUES( ? )", ( version, ) )

connection.commit()

# print stats
cursor.execute ("SELECT version FROM versions" )
for version in cursor.fetchall():
    cursor.execute ("SELECT * FROM players where version = ?", ( version[0], ) )
    print version[0].encode('utf-8'), ":", len(cursor.fetchall())
    
