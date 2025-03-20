#!/usr/bin/python

# parses the event log and counts how often each player played

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

# usage: tournament.py <message 
# the event log is expected on stdin, output is a player count dependant message
# include file.

# output: 
#  tournament.db: player count sqlite database

# Database layout:
# The first table, name version, only contains one column (version) and one row. The
# value of the single cell is the database format version.

# The second table, name griefers, contains the individual griefers as they are tracked.
# Its columns are:
# id: this primary key identification of the griefer, either a KeyID or an IP address.
# name: the name the griefer used first
# timesPlayed: the number of times the griefer entered a game, counted beginning from
#              his first recorded griefing

# The third table, named events, contains the griefing events. Its columns:
# id: a random unique ID to keep the events apart. Not actually used.
# griefer: the id of the griefer that committed the griefing
# type: the grief type (spam, noReady) as logged in the event log
# reference: a reference string, taken from the command line. Can be anyting the user
#            feeds in and may find useful to have later.
# name: the name the griefer had had that time.

import sqlite_wrapper
import parser
import os
import sys
import string
import ConfigParser
import time
import codecs

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
message = string.join(sys.argv[1:])

# read config file
config = ConfigParser.ConfigParser()

config.add_section("files")
config.set("files", "database", "tournament.db" )

# write default configuration
#try:
#    config.write(open('griefdatabase_default.ini','w'))
#except:
#    pass

config.read('tournament.ini')

charsetOut    = "utf-8"

# connect to database
connection = sqlite_wrapper.connect( config.get("files", "database"), timeout=120.0 )
cursor = connection.cursor()

if True:
    # create tables if they don't exist yet
    try:
        # create table that contains the version
        cursor.execute('CREATE TABLE version ( version INT )')
        cursor.execute('INSERT INTO version VALUES ( 0 )')

        # the table of all players
        cursor.execute('CREATE TABLE players (id VARCHAR(30) PRIMARY KEY, name VARCHAR(50), timesPlayed INT )')

        print "Database created."
    except sqlite_wrapper.DatabaseError:
        pass

    # database exists. check version.
    cursor.execute("SELECT version FROM version")
    currentVersion = 1
    version = cursor.fetchone()[0]
    if version < currentVersion:
        if version == 0:
            # the table of games
            cursor.execute('CREATE TABLE games ( player VARCHAR(30), points INT, gameMode INT, score FLOAT )')
            cursor.execute('CREATE INDEX playerIndex ON games (player)')

        # store new version
        cursor.execute("UPDATE version SET version = ?", [currentVersion])

        print "Database updated from version %s to version %s." % (version,currentVersion)

    connection.commit()

# players
class Player(parser.ClientBase):
    # initializes player, fetching stuff from the database
    def __init__( self ):
        parser.ClientBase.__init__(self)
        self.timesPlayed = 0
        self.inDataBase = False
        self.ID = ""

    def Load( self ):
        # return
        cursor.execute("SELECT timesPlayed, name FROM players WHERE id = ?", [self.ID] )
        data = cursor.fetchone()
        if data != None:
            self.inDataBase = True
            self.timesPlayed = data[0]
            self.name = data[1]
        
    # stores changed data into the database
    def Store( self ):
        # return
        # print self.ID
        if self.inDataBase:
            cursor.execute("UPDATE players SET timesPlayed = ?, name = ? WHERE id = ?", (self.timesPlayed, self.name, self.ID) )
        else:
            cursor.execute("INSERT INTO players VALUES( ?, ?, ? )", (self.ID, self.name, self.timesPlayed ) )
            self.inDataBase = True
        
    # logs offenses into a file, appending to it
    def Log( self, stream ):
        # open file for appending
        # print general player info

        stream.write( "Message%s Welcome back, %s. You have played %d games so far. Remember: Your worst two games are elimiated, and your placing in the next worst three games count in the end. Playing more than five or less than three games may harm your score.\n" % ( self.ID,  self.name, self.timesPlayed ) )

    # logs offenses into a file, appending to it
    def LogName( self, stream ):
        # open file for appending
        # print general player info

        stream.write( "ForceName%s %s\n" % ( self.ID,  self.name ) )

class Parser( parser.Parser ):
    def __init__( self ):
        self.valid = True
        self.gameMode = -1

    # factory method: overload in your subclass to create
    # a new client object
    def NewClient( self ):
        return Player()

    # factory method to create CPU
    def NewCPU( self ):
        return self.NewClient()

    # called after a new client has been filled
    def OnNewClient( self, client ):
        client.Load()

    # called after a new CPU has been filled
    def OnNewCPU( self, cpu ):
        pass
    
    # called when clients go out of sync
    def OnOutOfSync( self, player ):
        # sync errors invalidate games
        self.valid = False
    
    # called on setting change messages
    def OnSetting( self, setting, value ):
        if setting == "GameMode":
            self.gameMode = int(value)

    def OnEnd( self, scoresSigned, clients, origPlayers ):
        if not self.valid or not scoresSigned:
            return

        # eliminate evil/futurewinians
        players = {}
        for p in origPlayers:
            if origPlayers[p].teamID < 4:
                players[p] = origPlayers[p]

        sorted = []

        def PlayerCompare( a, b ):
            if b.score > a.score:
                return 1
            if b.score < a.score:
                return -1
            return 0

        totalScore = 0
        for p in players:
            player = players[p]
            totalScore = totalScore + player.score
            if player.version != "CPU":
                player.timesPlayed = player.timesPlayed + 1
                player.Store()
            sorted.append( player )
                
        # normalize scores to 100
        if totalScore > 0:
            for p in players:
                player = players[p]
                player.score = player.score * 100.0 / totalScore

        sorted.sort( PlayerCompare )
        # print players, sorted

        # points per position
        points = [ 7, 3, 1, 0 ]

        # average points if players have equal scores
        lastScore = -1
        lastRangeStart = 0
        for i in range(0,len(sorted)+1):
            if i == len(sorted) or sorted[i].score != lastScore:
                # average the scores of the last range
                if lastRangeStart >= 0:
                    sum = 0
                    for j in range(lastRangeStart, i ):
                        sum = sum + points[j]
                    for j in range(lastRangeStart, i ):
                        points[j] = sum/(i - lastRangeStart )
                lastRangeStart = i
            elif lastScore < 0:
                lastRangeStart = i
            try:
                lastScore = sorted[i].score
            except: pass

        # award points
        for i in range(0,len(sorted)):
            # print sorted[i].name, points[i]
            if sorted[i].version != "CPU":
                cursor.execute( 'INSERT INTO games VALUES ( ?, ?, ?, ? )', (sorted[i].ID, points[i], self.gameMode, sorted[i].score ) )

        #print "GameMode =", self.gameMode

# parse all games
while Parser().Parse(): pass
connection.commit()


if len(sys.argv) > 1:
    allplayers = []
    # and fill them with data from the whole database
    cursor.execute( "SELECT id FROM players" )
    players = cursor.fetchall()
    #print players

    messages = codecs.open( "messages.txt", "w", charsetOut )
    names = codecs.open( "names.txt", "w", charsetOut )
    
    for id in players:
        player = Player()
        player.ID = id[0]
        player.Load()
        player.Log( messages )
        player.LogName( names )
        allplayers.append( player )

        cursor.execute( "SELECT points FROM games WHERE player = ? ORDER BY points", [player.ID] )
        scores = cursor.fetchall()
        sum = 0
        
        total = 5
        keep  = 4
        
        minRange = total-keep
        maxRange = min(len(scores), total)
        if maxRange - minRange < keep:
            minRange = maxRange - keep
        if minRange < 0:
            minRange = 0

        individualScores = ""

        #print player.name, minRange, maxRange, len(scores)

        for i in range(minRange, maxRange):
            #print scores[i][0]
            sum = sum + scores[i][0]

            if len( individualScores ) > 0:
                individualScores =  " + " + individualScores
            individualScores = str(int(scores[i][0])) + individualScores
        player.individualScores = individualScores
            
        player.points = sum
        player.bestScores = []
        cursor.execute( "SELECT score FROM games WHERE player = ? ORDER BY score DESC", [player.ID] )
        score = 0
	try:
            for entry in cursor.fetchall():
		score = entry[0]
		player.bestScores.append(score)
        except:
            pass

        #print player.name, sum, player.bestScores

    def PlayerCompare( a, b ):
        if b.points > a.points:
            return 1
        if b.points < a.points:
            return -1
        for i in range(0, min( len(b.bestScores), len(a.bestScores) ) ):
            if b.bestScores[i] > a.bestScores[i]:
                return 1
            if b.bestScores[i] < a.bestScores[i]:
                return -1
        if len(b.bestScores) > len(a.bestScores):
            return 1
        if len(b.bestScores) < len(a.bestScores):
            return -1
        return 0

    allplayers.sort( PlayerCompare )

    leaderboard = codecs.open( "leaderboard.html", "w", charsetOut )

    leaderboard.write( '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Final//EN">\n' )
    leaderboard.write('<HTML><HEAD><meta http-equiv="expires" content="120">\n' )
    leaderboard.write('<title>Tournament Leaderboard</title>\n' )
    # <link rel="stylesheet" type="text/css" href="dedcon.css" />
    leaderboard.write( '</HEAD><body>\n' )
    leaderboard.write( '<h1>Tournament Leaderboard</h1>\n' )
    leaderboard.write( '<table border="1">\n' )
    leaderboard.write( "<tr><th>Rank</th><th>ID</th><th>Points</th><th>Best Scores</th><th>Games</th><th>Name</th></tr>\n" )
    rank = 0
    for player in allplayers:
        rank = rank + 1
        ID = player.ID
        if ID[0] == 'I':
            ID = "IP x.x.x.x"
        bestScores = ""
        for score in player.bestScores:
	    if len( bestScores ) > 40:
                bestScores = bestScores + "."
            else:
                if len( bestScores ) > 0:
                    bestScores = bestScores + ', '
                bestScores = bestScores + str(int(score))
        points = str(player.points)
        if len( player.bestScores ) > 1:
            points = points + " = " + player.individualScores
        leaderboard.write( "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>\n" % ( rank, ID, points, bestScores, player.timesPlayed, player.name ) )

    leaderboard.write( "</table></body></html>" )
