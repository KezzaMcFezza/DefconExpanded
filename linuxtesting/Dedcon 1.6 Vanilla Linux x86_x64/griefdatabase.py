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
# convey that the whole of DedCon/DedWinia is covered by it, which it isn't. In case you have
# no copy of the GPL at hand, here are the important points:
# - as long as you don't pass on the software in an altered form, you may do with
#   it whatever you like.
# - you are free to modify and redistribute the software in source form, provided this
#   copyright and license notice stays intact and you grant the recipient at least the
#   rights the GPL gives them. You may of course append your own copyright statement.
# - you can also distribute modified versions in binary form provided you give the
#   recipient also access to the modified source.

# usage: griefdatabase.py <reference string>
# the event log is expected on stdin, the reference string will be logged
# and printed as a reference into the warn and ban files.
# The log file is expected to be timestamped with a non-whitespace timestamp
# followed by whitespace.

# output: (configurable in the griefdatabase.ini file)
#  griefer.db: griefer sqlite database
#  auto_warn.txt: automatically generated warning messages
#  auto_ban.txt:  automatically generated bans

# Database layout:
# The first table, name version, only contains one column (version) and one row. The
# value of the single cell is the database format version.

# The second table, name griefers, contains the individual griefers as they are tracked.
# Its columns are:
# id: this primary key identification of the griefer, either a KeyID or an IP address.
# name: the name the griefer used first
# timesPlayed: the number of times the griefer entered a game, counted beginning from
#              his first recorded griefing
# lastGrief:   the latest date at which the griefer did something bad

# The third table, named events, contains the griefing events. Its columns:
# id: a random unique ID to keep the events apart. Not actually used.
# griefer: the id of the griefer that committed the griefing
# type: the grief type (spam, noReady) as logged in the event log
# reference: a reference string, taken from the command line. Can be anyting the user
#            feeds in and may find useful to have later.
# name: the name the griefer had had that time.

import sqlite_wrapper
import os
import sys
import string
import ConfigParser
import time
import codecs
import parser

def GetCurrentDate():
    return int( time.strftime("%Y%m%d") )

currentDate = GetCurrentDate()
# print currentDate

def DaysSince( t ):
    # assume every month has 31 days. Suffices for our purposes here :)
    y = (currentDate/1000) - (t/1000)
    m = ((currentDate/100) % 100) - ((t/100) % 100)
    d = (currentDate % 100) - (t % 100)

    return ( y * 12 + m ) * 31 + d

# print DaysSince( "20060701" )

# command line: everything is interpreted as griefing reference
reference = string.join(sys.argv[1:])

# read config file
config = ConfigParser.ConfigParser()

config.add_section("sys")
config.set("sys", "charsetOut", "utf-8" )

config.add_section("files")
config.set("files", "database", "griefer.db" )
config.set("files", "ban",      "auto_ban.txt" )
config.set("files", "warning",  "auto_warn.txt" )

config.add_section("policies")
config.set("policies", "banOffset", "10")
config.set("policies", "warnOffset", "3")
config.set("policies", "perGameOffset", "1")
config.set("policies", "perDayOffset", ".2")
config.set("policies", "purgeOffset", "0")
config.set("policies", "warnMessage", "WARNING: you have been naughty and the automatic Santa Claus has you on his list. Better behave, or you get banned.")

config.add_section("severities")
config.set("severities", "earlyQuit", "3" )
config.set("severities", "noReady", "4" )
config.set("severities", "spam", "6" )
config.set("severities", "voteKick", "4" )
config.set("severities", "adminKick", "6" )

# write default configuration
#try:
#    config.write(open('griefdatabase_default.ini','w'))
#except:
#    pass

config.read('griefdatabase.ini')

# returns the severity of a grief type
def GetGriefSeverity( type ):
    try:
        return float(config.get( "severities", type ))
    except:
        return 0.0

# parse config
charsetOut    = config.get("sys", "charsetOut" )

banFileName   = config.get("files", "ban" )
warnFileName  = config.get("files", "warning" )

banOffset     = float(config.get("policies", "banOffset"))
warnOffset    = float(config.get("policies", "warnOffset" ))
perGameOffset = float(config.get("policies", "perGameOffset" ))
perDayOffset =  float(config.get("policies", "perDayOffset" ))
purgeOffset   = float(config.get("policies", "purgeOffset"))
# config.get("policies", "perGameOffset2" )

warnMessage   = config.get("policies", "warnMessage")

# connect to database
connection = sqlite_wrapper.connect( config.get("files", "database"), timeout=120.0 )
cursor = connection.cursor()

# create tables if they don't exist yet
try:
    # create table that contains the version
    cursor.execute('CREATE TABLE version ( version INT )')
    cursor.execute('INSERT INTO version VALUES ( 0 )')

    # the table of all players that griefed at least once
    cursor.execute('CREATE TABLE griefers (id VARCHAR(30) PRIMARY KEY, name VARCHAR(50), timesPlayed INT )')
        # the table of the griefing events
    cursor.execute('CREATE TABLE events (id INTEGER PRIMARY KEY, griefer VARCHAR(30), type VARCHAR(20), reference VARCHAR)')
    cursor.execute('CREATE INDEX grieferindex ON events (griefer)')

    print "Database created."
except sqlite_wrapper.DatabaseError:
    pass

# database exists. check version.
cursor.execute("SELECT version FROM version")
currentVersion = 1
version = cursor.fetchone()[0]
if version < currentVersion:
    if version < 1:
        cursor.execute("ALTER TABLE events ADD name VARCHAR(50)")
        cursor.execute("UPDATE events SET name = ''")
        cursor.execute("ALTER TABLE griefers ADD lastGrief DATE")
        cursor.execute("UPDATE griefers SET lastGrief = ?", [currentDate] )
        #cursor.execute("UPDATE griefers SET lastGrief = 20070705" )

    # store new version
    cursor.execute("UPDATE version SET version = ?", [currentVersion])

    print "Database updated from version %s to version %s." % (version,currentVersion)

connection.commit()

# players
class Player(parser.ClientBase):
    # initializes player, fetching stuff from the database
    def __init__( self ):
        parser.ClientBase.__init__( self )
        self.timesPlayed = 0
        self.inDataBase = False
        self.griefed = False
        self.griefedNow = False
        self.lastGrief=currentDate

    def Load( self ):
        try:
            cursor.execute("SELECT timesPlayed, name, lastGrief FROM griefers WHERE id = ?", [self.ID] )
            data = cursor.fetchone()
            if data != None:
                self.inDataBase = True
                self.timesPlayed = data[0]
                self.name = data[1]
                self.lastGrief = data[2]

        except sqlite_wraper.OperationalError:
            cursor.execute("DELETE FROM griefers WHERE id = ?", [self.ID] )
            self.name = "unknown"
            self.griefed = True
        
    # stores changed data into the database
    def Store( self ):
        # print self.ID, self.inDataBase
        if self.inDataBase:
            cursor.execute("UPDATE griefers SET timesPlayed = ?, lastGrief = ?, name = ? WHERE id = ?", (self.timesPlayed, self.lastGrief, self.name, self.ID) )
        else:
            cursor.execute("INSERT INTO griefers VALUES( ?, ?, ?, ? )", (self.ID, self.name, self.timesPlayed, self.lastGrief ) )
            self.inDataBase = True
        
    # logs offenses into a file, appending to it
    def Log( self, filename, prefix, suffix, ID, griefs ):
        # open file for appending
        f = codecs.open( filename, "a", charsetOut )    

        # print general player info
        f.write( "# Name = %s, Times played: %d\n" % (self.name, self.timesPlayed) )
    
        # list griefs
        for grief in griefs:
            f.write( "# %s %s %s\n" % (grief[0], grief[2], grief[1]) )
        
        # write ban/warning
        f.write( "%s%s%s\n\n" % (prefix, ID, suffix) )

    # determines whether a specific griefer should be warned or banned
    def WarnBan( self ):
        cursor.execute('SELECT type, reference, name FROM events WHERE griefer = ?', [self.ID] )
        griefs = cursor.fetchall()
    
        severity = 0.0
        for f in griefs:
            severity = severity + GetGriefSeverity( f[0] )

        #print self.lastGrief
        daysSinceLastGrief = DaysSince( self.lastGrief )
        # print daysSinceLastGrief

        #print self.ID, self.name, daysSinceLastGrief
        
        severity = severity - self.timesPlayed * perGameOffset

        if severity > banOffset + daysSinceLastGrief * perDayOffset:
            self.Log( banFileName, "Ban", "", self.ID, griefs )
        else: 
            if severity > warnOffset + daysSinceLastGrief * perDayOffset:
                self.Log( warnFileName, "Message", " " + warnMessage, self.ID, griefs )

        # check whether the entry should be purged
        severity = severity - daysSinceLastGrief * perDayOffset * .5
        if severity < purgeOffset:
            # erase the griefing events
            if len( griefs ) > 0:
                print "Erasing griefing events of player ", self.ID
                cursor.execute("DELETE FROM events WHERE griefer = ?", (self.ID, ) )

            # but adapt the number of games played so that the next griefing stays
            # in the database
            if perGameOffset > 0:
                self.timesPlayed = - purgeOffset / perGameOffset
                self.Store()

class Parser(parser.Parser):
    # Methods for your parser to override; they get called when
    # you call Parse()

    # factory method: overload in your subclass to create
    # a new client object
    def NewClient( self ):
        return Player()

    # called after a new client has been filled
    def OnNewClient( self, client ):
        client.Load()

    # called after a new CPU has been filled
    #def OnNewCPU( self, cpu ):
    #    pass

    # called when a player griefs
    def OnGrief( self, player, grief ):
        # player is the player, grief the grief ID string
        # add them to the database
        cursor.execute('INSERT INTO events VALUES( null, ?, ?, ?, ? )', (player.ID, grief, reference, player.name ) )
        player.griefed = True
        player.griefedNow = True
        player.lastGrief = currentDate

    # called at the end of parsing; parameter is true if scores were set and signed
    # and maps from clientID resp. teamID to clients
    def OnEnd( self, scoresSigned, clients, players ):
        # update player database to count how often players have played
        for p in players:
            player = players[p]
            player.timesPlayed = player.timesPlayed + 1
            player.Store()

        # test whether the ban and warn files exist
        try:
            open(warnFileName).close()
            open(banFileName).close()

            # they exist? Good, then do only incremental updates
            # analyze the database to find out who needs to be banned or warned
            for p in players:
                player = players[p]
                if player.griefedNow:
                    player.WarnBan()    

        except IOError:
            # No. Well, create them!
            open(warnFileName,"a").close()
            open(banFileName,"a").close()

            # and fill them with data from the whole database
            cursor.execute( "SELECT id FROM griefers" )
            griefers = cursor.fetchall()
            for id in griefers:
                player = Player()
                player.ID = id[0]
                player.Load()
                player.WarnBan()

while Parser().Parse(): pass
connection.commit()
 
