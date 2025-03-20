#!/usr/bin/python

# creates a dynamic league from an unorganized pool of games

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
# convey that the whole of DedCon/Dedwinia is covered by it, which it isn't. In case you have
# no copy of the GPL at hand, here are the important points:
# - as long as you don't pass on the software in an altered form, you may do with
#   it whatever you like.
# - you are free to modify and redistribute the software in source form, provided this
#   copyright and license notice stays intact and you grant the recipient at least the
#   rights the GPL gives them. You may of course append your own copyright statement.
# - you can also distribute modified versions in binary form provided you give the
#   recipient also access to the modified source.

import sqlite_wrapper

import os
import sys
import string
import ConfigParser
#import time
import codecs
import math
import parser

# usage: ./rating.py <list of commands>
# the ini file ./rating.ini is always read. Available commands (they can be chained)
# are given by calling the program without commands.

# The freeprint command is a little special and deserves further explanation; it accepts
# two arguments, one a format string for rated players, the other a format string
# for unrated players. Players count as rated when the system is able to give them
# a lower score; they need to have won a significant number of games for that.
# The format string is echoed to standard output for every player and the following
# replacements are made:
# %p -> the player's position in the table
# %i -> the player's ID
# %j -> the player's ID with uncensored IP address
# %n -> the player's name
# %g -> the number of games played
# %c -> the number of games the player did not enter the scores for
# %l -> the player's lower score
# %m -> the player's middle socre
# %u -> the player's upper score
# %w -> the total weight of the games of that player
# %t -> the total number of players
# %r -> the total number of rated players

# principle of the rating: determine the scores of the players so that for each player,
# the sum over
# weight * ( pwned( other ) - math.tanh( score - other.score ) )
# is zero. The sum runs over all other players. pwned( other ) is 1 if the current player
# always was victorious against the other, -1 if he always lost, and in between for the mixed
# results. weight is the number of played games between the two players.

# A phantom player of configurable skill and fixed score is added to anchor the scores.

# Game results are enterend into the database in various forms:
#  win : only winning the game (being the player with the top score) counts. The winner gets
#        one point, the loser gets none.
#  score : scores are normalized (everyone gets the same bonus so that nobody has negative score),
#          then each player gets that normalized score.
#  clamped: like 'score', but the normailization just clamps scores below zero to zero.
#  difference: for each pair of players, the one with the higher score gets the score difference,
#              the one with the lower score gets nothing.

# Each of these variations also has a team variant where the scores of the players' pre-game
# alliances are added up before they are processed.


# Database layout:
# The first table, name version, only contains one column (version) and one row. The
# value of the single cell is the database format version.

# The second table, name players, contains the individual players as they are tracked.
# Its columns are:
# id: this primary key identification of the player, either a KeyID or an IP address.
# name: the name the player used lasst
# score: the player's current ranking
# games: the number of games played
# missed: the number of games missed; a game counts as missed if the player took part
#         in it, but no scores were entered.

# The third table, named games, contains the logged score data for each player pair.
# Its columns:
# id1: the ID of the first player
# id2: the ID of the second player
# win1: the points accumulated by player 1
# win2: the points accumulated by player 2
# type: the stat type

# read config file
config = ConfigParser.ConfigParser()
config.add_section("sys")
config.set("sys", "charsetIn", "latin1" )
config.set("sys", "charsetOut", "utf-8" )
config.set("sys", "hideIP", "yes" )

config.add_section("files")
config.set("files", "database",  "rating.db" )
config.set("files", "text",      "rating.txt" )
config.set("files", "html",      "rating.html" )
config.set("files", "unrated",   "unrated.txt" )
config.set("files", "ipranges",  "ipranges.txt" )
config.set("files", "aliases",   "aliases.txt" )

config.add_section("weights")
config.set("weights", "win",         "1.0" )
config.set("weights", "position",    "0.0" )
config.set("weights", "score",       "0.0" )
config.set("weights", "clamped",     "0.0" )
config.set("weights", "difference",  "0.0" )
config.set("weights", "teamwin",         "0.0" )
config.set("weights", "teamposition",    "0.0" )
config.set("weights", "teamscore",       "0.0" )
config.set("weights", "teamclamped",     "0.0" )
config.set("weights", "teamdifference",  "0.0" )

config.add_section("prescale")
config.set("prescale", "demo",         "1.00" )
for prefix in ('','team'):
    config.set("prescale", prefix + "win",         "1.00" )
    config.set("prescale", prefix + "position",    "1.00" )
    config.set("prescale", prefix + "score",       "0.01" )
    config.set("prescale", prefix + "clamped",     "0.01" )
    config.set("prescale", prefix + "difference",  "0.01" )

config.add_section("phantom")
config.set("phantom", "score",      "600" )
config.set("phantom", "skill",      "90" )
config.set("phantom", "weight",      "1" )
config.set("phantom", "relativeweight",      "1" )
config.set("phantom", "range",       "1" )

config.add_section("score")
config.set("score", "min",        "-1000" )
config.set("score", "max",        "10000" )
config.set("score", "range",      "100" )
config.set("score", "missed",     "1.0" )
config.set("score", "missedTolerance", "0.1" )
config.set("score", "minWeight",  "2.0" )
config.set("score", "minGames",   ".5" )
config.set("score", "apprenticeGames",  "1.0" )
config.set("score", "apprenticeGamesUnreliable",   "3.0" )
config.set("score", "minEps",   ".1" )
config.set("score", "maxIterations",   "10" )

config.add_section("decay")
config.set("decay", "player",        ".5" )
config.set("decay", "games",         ".25" )

config.add_section("amnesia")
config.set("amnesia", "MaxWeight",        "40" )
config.set("amnesia", "Base",             "1" )
config.set("amnesia", "Distance",         "6" )

# write default configuration
#try:
    #config.write(open('rating_default.ini','w'))
#except:
    #pass

config.read('rating.ini')

charsetIn     = config.get("sys", "charsetIn" )
charsetOut    = config.get("sys", "charsetOut" )

missedInfluence = float( config.get("score", "missed" ) )
missedTolerance = float( config.get("score", "missedTolerance" ) )
influenceRange = float( config.get( "score", "range" ) )
minScore = float( config.get( "score", "min" ) )
maxScore = float( config.get( "score", "max" ) )

minWeight = float( config.get( "score", "minWeight" ) )
minGames = float( config.get( "score", "minGames" ) )

apprenticeGames = float( config.get( "score", "apprenticeGames" ) )
apprenticeGamesUnreliable = float( config.get( "score", "apprenticeGamesUnreliable" ) )

minEps = float( config.get( "score", "minEps" ) )
maxIter = int( config.get( "score", "maxIterations" ) )

hideIP = ( config.get( "sys", "hideIP" ) == "yes" )

large = 1E+6
        
# read dynamic IP range users
dynamicIPUsers = {}
try:
    dynamicIPFile = codecs.open( config.get( "files", "ipranges" ), "r", charsetIn )
    for line in dynamicIPFile:
        lineSplit = line.split()
        IP = lineSplit[0]
        name = string.join(lineSplit[1:])
        try:
            dynamicIPUsers[name].append(IP)
        except KeyError:
            dynamicIPUsers[name] = [IP]
except IOError:
    pass

# read aliases
aliases= {}
try:
    aliasesFile = codecs.open( config.get( "files", "aliases" ), "r", charsetIn )
    for line in aliasesFile:
        lineSplit = line.split()
        if len(lineSplit) >= 2 and line[0] != '#':
            alias    = lineSplit[0].replace("~"," ")
            original = string.join(lineSplit[1:])
            aliases[alias] = original
            # print alias, "->", original
except IOError:
    pass

# print dynamicIPUsers

# an influence for another score
class Influence:
    def __init__( self, score, pwned, weight ):
        self.range = influenceRange
        self.score = score
        if score < minScore:
            self.score = minScore
        if score > maxScore:
            self.score = maxScore
        self.pwned = pwned
        self.weight = weight
        self.errUp = 0
        self.errDown = 0

# calculates the force on a player's score based on the influences
def Force( score, influences, bias ):
    force = bias
    for influence in influences:
        f = influence.weight * ( influence.pwned - math.tanh( (score - influence.score)/influence.range ) )
        # f = influence.weight * ( influence.pwned - math.tanh( (score - influence.score)*.01 ) )
        # print "influence = ", influence.score, influence.pwned, "force = ", f
        force = force + f

    #print force
    return force

# finds an equilibrium position between two scores where the force is zero
def Equilibrium( minScore, maxScore, influences, eps, bias ):
    midScore = ( maxScore + minScore ) * .5
    while maxScore > midScore and midScore > minScore and maxScore - minScore > eps:
        # print minScore, maxScore
        f = Force( midScore, influences, bias )
        if f == 0:
            return midScore
        if f > 0:
            minScore = midScore
        else:
            maxScore = midScore
        midScore = ( maxScore + minScore ) * .5

    return midScore

# finds an equilibrium position
def Rate( influences, eps, start, bias ):
    minScore = 0.0
    maxScore = 0.0

    delta = eps * 3
    while Force( start - delta, influences, bias ) * Force( start + delta, influences, bias ) > 0 and delta < large * 5:
        delta = delta * 5
    return Equilibrium( start - delta, start + delta, influences, eps, bias )

class Player:
    def __init__( self, id, name, score, games, missed, minScore, maxScore ):
        self.id = id
        self.name = name
        self.score = float(score)
        self.games = float(games)
        self.missed = float(missed)
        self.minScore = float(minScore)
        self.maxScore = float(maxScore)

        # statistical weight of this player. Players of lower weight don't influence
        # others that much.
        games = 1.0
        if self.id.find( "IP" ) == 0:
            games = apprenticeGamesUnreliable
        else:
            games = apprenticeGames

        self.weight = self.games/games
        if self.weight > 1.0:
            self.weight = 1.0

    def PrintID( self ):
        if self.id.find( "IP" ) == 0 and hideIP:
            return "IP x.x.x.x"
        else:
            return self.id

# exception to throw for fishy games
class Fishy:
    pass

class Phantom:
    def __init__( self, score, pwn, weight, relativeweight, range ):
        self.range = range
        self.score = float(score)
        self.pwn = float(pwn)
        self.weight = float(weight)
        self.relativeweight = float(weight)

    def GetInfluence( self, factor, games ):
        weight = self.weight
        if weight > games * self.relativeweight:
            weight = games * self.relativeweight
        influence = Influence( self.score, self.pwn * factor, weight )
        influence.range = self.range
        return influence

    def SetRange( self, range ):
        self.range = range
        
    def SetEntry( self, zero ):
        influence = Influence( 0, self.pwn, 1 )
        influence.range = 1.0
        influences = [ influence ]
        atanh = Rate( influences, 1E-10, 0, 0 )

        diff = self.score - zero

        self.range = diff/atanh
        # print atanh, diff, self.range

class NoPlayer:
    pass

class Database:
    def CreatePlayer( self, p ):
        if p == None:
            raise NoPlayer
            
        # print p
        return( Player( p[0], p[1], p[2], p[3], p[4], p[5], p[6] ) )
        
    def __init__( self ):
        self.connection = sqlite_wrapper.connect( config.get( "files", "database" ), timeout=7200.0 );
        self.cursor = self.connection.cursor()

        created = False
        currentVersion = 5
        try:
        #if 1:
            self.cursor.execute('CREATE TABLE version ( version INT )')
            self.cursor.execute('INSERT INTO version VALUES ( 0 )')

            self.cursor.execute('CREATE TABLE players ( id VARCHAR(30) PRIMARY KEY,name VARCHAR(50), score FLOAT )')
            self.cursor.execute('CREATE TABLE games ( id1 VARCHAR(30), id2 VARCHAR(30), win1 FLOAT, win2 FLOAT )')

            self.cursor.execute('CREATE INDEX id1index ON games (id1)')
            self.cursor.execute('CREATE INDEX id2index ON games (id2)')

            created = True
        except:
            pass

        self.cursor.execute('SELECT version FROM version')
        version = self.cursor.fetchone()[0]

        if version < 1:
            self.cursor.execute('ALTER TABLE players ADD games FLOAT')
            self.cursor.execute('UPDATE players SET games = 1')

        if version < 2:
            self.cursor.execute('ALTER TABLE games ADD type VARCHAR(20)')
            self.cursor.execute("UPDATE games SET type = 'win'")

        if version < 3:
            self.cursor.execute('ALTER TABLE players ADD missed FLOAT')
            self.cursor.execute("UPDATE players SET missed = 0")

        if version < 4:
            self.cursor.execute('ALTER TABLE players ADD minScore FLOAT')
            self.cursor.execute('ALTER TABLE players ADD maxScore FLOAT')
            self.cursor.execute("UPDATE players SET minScore = 0, maxScore = 0")

        if version < 5:
            if not created:
                print "Sorry, database formats prior to 5 are not fully upgradable. Error bars will be wrong."
            self.cursor.execute('ALTER TABLE games ADD err1up FLOAT')
            self.cursor.execute('ALTER TABLE games ADD err2up FLOAT')
            self.cursor.execute('ALTER TABLE games ADD err1down FLOAT')
            self.cursor.execute('ALTER TABLE games ADD err2down FLOAT')
            self.cursor.execute("UPDATE games SET err1down = win1+win2, err2down = win1+win2, err1up = win1+win2, err2up = win1+win2")

        if version < currentVersion:
            self.cursor.execute("UPDATE version SET version = ?", [currentVersion] )
            if created: 
                print "Database version %d created." % currentVersion
            else:
                print "Updated database from version %d to %d." % (version, currentVersion )

	self.connection.commit()

        # self.cursor.execute( "SELECT * FROM players WHERE id == 'TOP'" )
        # self.topScorer = self.CreatePlayer( self.cursor.fetchone() )

        self.Save()
        
    def Save( self ):
        self.connection.commit()

    
    # returns a list of players in the database
    def GetPlayers( self ):
        self.players = {}
        self.cursor.execute( 'SELECT * FROM players ORDER BY score DESC' )
        for p in self.cursor.fetchall():
            player = self.CreatePlayer( p )
            #if player.id == 'TOP':
            #    player = self.topScorer
                
            self.players[ player.id ] = player

        return self.players

    def GetPlayer( self, id ):
        try:
            return self.players[id]
        except KeyError:
            # a new player must have been entered into the database while we were busy with
            # other stuff, return a dummy (inserting stuff into the player list does not work, sadly)
            return Player( id, "UNKNOWN", 0,1,0,0,0);
    
        #self.cursor.execute( 'SELECT * FROM players WHERE id = ?', [id] )
        #p = self.cursor.fetchone()
        #return self.CreatePlayer( p )

    # updates the scores of players in the database
    def SavePlayer( self, player ):
        self.cursor.execute( 'UPDATE players SET score = ?, minScore = ?, maxScore = ? WHERE id == ?', ( player.score, player.minScore, player.maxScore, player.id ) )

    # updates the scores of players in the database
    def SavePlayers( self, players ):
        for player in players:
            self.UpdatePlayer( player )

    # returns the influences for a player
    def GetInfluencesInternal_( self, cursor, influences, externalWeight ):
        for i in cursor.fetchall():
            otherID = i[0]
            try:
                otherPlayer = self.GetPlayer( otherID )
                weightConf = float( config.get( 'weights', i[3] ) ) * otherPlayer.weight * externalWeight
                win1 = i[1] * weightConf
                win2 = i[2] * weightConf
                errUp = i[4] * weightConf * weightConf
                errDown = i[5] * weightConf * weightConf
                weight = float(win1 + win2)
                if weight > 0:
                    score = otherPlayer.score
                    # print otherID, win1, win2, score

                    influence = Influence( score, ( win1 - win2 )/weight, weight )
                    influence.otherID = otherID
                    influence.errUp = errUp
                    influence.errDown = errDown
                    influences.append( influence )
            except NoPlayer:
                pass
                
        
    # returns the influences for a player
    def GetInfluences( self, player, phantom ):
        influences = []

        # add real data
        self.cursor.execute( 'SELECT id2, win1, win2, type, err1up, err1down FROM games WHERE id1 == ? ORDER BY id2', [player.id] )
        self.GetInfluencesInternal_( self.cursor, influences, player.weight )
        self.cursor.execute( 'SELECT id1, win2, win1, type, err2up, err2down  FROM games WHERE id2 == ? ORDER BY id1', [player.id] )
        self.GetInfluencesInternal_( self.cursor, influences, player.weight )
        
        # add phantom influence
        if phantom != None:
            influences.append( phantom.GetInfluence( -1.0, player.games ) )

        return influences

    # returns the influences for a player
    def GetPhantomInfluences( self, players, phantom ):
        influences = []

        for p in players:
            player = players[p]
            influence = phantom.GetInfluence( +1.0, player.games )
            influence.score = player.score
            influences.append( influence )

        return influences

class AtanScores:
    def __init__( self, database ):
        self.database = database
        self.players = database.GetPlayers()
        self.phantom = Phantom( float( config.get( "phantom", "score" ) ),
                                float( config.get( "phantom", "skill" ) )/50.0 - 1,
                                float( config.get( "phantom", "weight" ) ),
                                float( config.get( "phantom", "relativeweight" ) ),
                                float( config.get( "phantom", "range" ) ) * influenceRange )
        # self.phantom.SetEntry( 0 )

    def Save( self ):
        for player in self.players:
            self.database.SavePlayer( self.players[player] )

    def UpdateAll( self, eps ):
        players = []
        for p in self.players:
            players.append( self.players[p] )
        self.Update( eps, players )

    def Update( self, eps, players, phantom ):
        for player in players:
            player.lastScore = player.score
            player.score = Rate( self.database.GetInfluences( player, self.phantom ), eps, player.score, 0 )
            # print "score", player.score

        # move all players around for the phantom score
        if phantom:
            move = self.phantom.score - Rate( self.database.GetPhantomInfluences( self.players, self.phantom ), eps, self.phantom.score, 0 )

        # print move
        maxChange = 0
        for player in players:
            if phantom:
                player.score = player.score + move

            change = abs( player.lastScore - player.score )
            if change > maxChange:
                maxChange = change

        return maxChange

    def Error( self, eps ):
        for p in self.players:
            player = self.players[p]
            errUp = 0.0
            errDown = 0.0
            for influence in self.database.GetInfluences( player, self.phantom ):
                scoreDifference = ( player.score - influence.score )/influence.range
                if abs(scoreDifference) < 10:
                    x = (1+math.tanh( scoreDifference ))*.5
                    errUp = errUp + influence.errUp *  x
                    errDown = errDown + influence.errDown * (1-x)

            if errUp < minWeight:
                errUp = minWeight
            if errDown < minWeight:
                errDown = minWeight

            errUp = math.sqrt(errUp)
            errDown = math.sqrt(errDown)

            #if player.name == "Bert":
            #print player.name, player.games, points, weight, relativeError, err

            
            # print player.games, won, lost, errUp, errDown

            # effective number of missed games
            missed = player.missed - missedTolerance * player.games
            if missed < 0:
                missed = 0
            missed = missed * missedInfluence

            # it influences the upper and lower error bound:
            errUp = errUp - missed
            if errUp < 0:
                errUp = 0
            errDown = -errDown - missed

            player.minScore = Rate( self.database.GetInfluences( player, self.phantom ), eps, player.score, errDown )
            player.maxScore = Rate( self.database.GetInfluences( player, self.phantom ), eps, player.score, errUp )

            if player.minScore < -large:
                player.minScore = -large
            if player.maxScore > large:
                player.maxScore = large

def StabilizeCore( database, scores, players, phantom, minEpsLocal, maxIterLocal ):
    if len(players) == 0:
        return

    eps = minEpsLocal
    lastEps = eps
    while eps  >= minEpsLocal and maxIterLocal > 0:
        maxIterLocal = maxIterLocal - 1
        eps *= .1
        if eps > lastEps * 2:
            eps = lastEps * 2
        lastEps = eps
        eps = scores.Update( eps, players, phantom )

    scores.Error( 1E-8 )
    scores.Save()
    database.Save()

def Stabilize( database, scores, players, phantom ):
    if len(players) == 0:
        return

    maxIterLocal = len(players)
    maxIterLocal = maxIterLocal * maxIterLocal
    if maxIterLocal > maxIter:
        maxIterLocal = maxIter
    minEpsLocal = 1/float( len(players) )
    if minEpsLocal < minEps:
        minEpsLocal = minEps
    StabilizeCore( database, scores, players, phantom, minEpsLocal, maxIterLocal )

def StabilizeAll():
    database = Database()
    scores = AtanScores( database )

    players = []
    for p in scores.players:
        players.append( scores.players[p] )

    Stabilize( database, scores, players, True )

def StabilizeAllLazy():
    database = Database()
    scores = AtanScores( database )

    players = []
    for p in scores.players:
        players.append( scores.players[p] )

    StabilizeCore( database, scores, players, True, influenceRange/5, 5 )

def CompareMinScore( a, b ):
    if b.minScore > a.minScore:
        return 1
    if b.minScore < a.minScore:
        return -1
    if b.score > a.score:
        return 1
    if b.score < a.score:
        return -1
    return 0

def Print():
    database = Database()
    scores = AtanScores( database )
    # scores.Error(1E-8)

    players = []
    for p in scores.players:
        player = scores.players[p]

        influences = database.GetInfluences( player, None )

        # calculate total weight and decay possibility
        weight = 0.0
        for influence in influences:
            weight = weight + influence.weight

        player.enemies = weight
        players.append( player )

    players.sort( CompareMinScore )

    # open files
    txt = codecs.open( config.get( "files", "text" ), "w", charsetOut )    
    html = codecs.open( config.get( "files", "html" ), "w", charsetOut )    
    unrated = codecs.open( config.get( "files", "unrated" ), "w", charsetOut )    

    unratedCount = 0

    index = 0
    for player in players:
        index = index + 1
        if player.games < minGames:
            continue

        # print general player info
        if player.minScore > -large:
            txt.write( "%d %d %s %s\n" % (int(player.minScore), int(player.score), player.PrintID(), player.name) )
            maxScore = player.maxScore
            if maxScore < large:
                maxScore = "%d" % int(maxScore+.5)
            else:
                maxScore = "-"

            html.write( "<tr><td>%d</td><td>%d</td><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td></tr>\n" % (index, int(player.minScore+.5), int(player.score+.5), maxScore, int(player.games+.49999), int(player.enemies+.499999), int(player.missed+.49999), player.PrintID(), player.name) )
        else:
            unratedCount = unratedCount + 1
            
    unrated.write( "%d\n" % (unratedCount) )

class Client(parser.ClientBase):
    def __init__( self ):
        parser.ClientBase.__init__( self )
        self.dropped = False
        self.score = 0

def Plot( player ):
    database = Database()
    database.cursor.execute("SELECT * FROM players WHERE id like ?", [player] )
    player = database.CreatePlayer( database.cursor.fetchone() )
    phantom = Phantom( float( config.get( "phantom", "score" ) ),0,0 )
    points = 0.0
    weight = 0.0
    influences = database.GetInfluences( player, phantom )
    min = 1E+20
    max = -1E+20
    for influence in influences:
        if influence.score < min:
            min = influence.score
        if influence.score > max:
            max = influence.score
    min = min - influenceRange
    max = max + influenceRange
    x = int(min)
    maxWeight = 10
    while True:
        weight = 0.0
        for influence in influences:
            c = math.cosh( ( x - influence.score )/influenceRange )
            weight = weight + influence.weight/(c*c)
        
        print x, weight
        if weight > maxWeight:
            maxWeight = weight
        if x > max and weight < maxWeight * .1:
            break
        x = x + 1

def FreePrint( format, unrated ):
    database = Database()
    scores = AtanScores( database )

    players = []
    for p in scores.players:
        player = scores.players[p]

        influences = database.GetInfluences( player, None )

        # calculate total weight and decay possibility
        weight = 0.0
        for influence in influences:
            weight = weight + influence.weight

        player.enemies = weight
        players.append( player )

    players.sort( CompareMinScore )

    unratedCount = 0
    count = len(players)

    # count rated players
    rated = 0
    for player in players:
        if player.games >= minGames and player.minScore > -large:
            rated = rated + 1
    
    pos = 1
    for player in players:
        if player.games < minGames:
            continue

        # print general player info
        out = format
        if player.minScore <= -large:
            out = unrated
        if len(out) > 0:
            maxScore = player.maxScore
            if maxScore < large:
                maxScore = "%d" % int(maxScore+.5)
            else:
                maxScore = "-"

            minScore = player.minScore
            if minScore > -large:
                minScore = "%d" % int(minScore+.5)
            else:
                minScore = "-"

            out = out.replace( "%p", "%d" % pos )
            out = out.replace( "%l", minScore )
            out = out.replace( "%m", "%d" % int(player.score+.5) )
            out = out.replace( "%u", maxScore )
            out = out.replace( "%i", player.PrintID() )
            out = out.replace( "%j", player.id )
            out = out.replace( "%g", "%d" % int(player.games+.5) )
            out = out.replace( "%c", "%d" % int(player.missed+.5) )
            out = out.replace( "%w", "%d" % int(player.enemies+.5) )
            out = out.replace( "%t", "%d" % int(count) )
            out = out.replace( "%r", "%d" % int(rated) )

            out = out.replace( "%n", player.name )

            print out.encode( charsetOut )

        pos = pos + 1

class Parser(parser.Parser):
    def __init__( self, stabilize ):
        self.database = Database()
        self.stabilize = stabilize

    def Save( self ):
        self.database.Save()

    def Loop( self ):
        try:
            while self.Parse():
                pass
        except:
            pass
        self.Save()

    def Parse( self ):
        try:
            return parser.Parser.Parse( self )
        except Fishy:
            return True

    # factory method: overload in your subclass to create
    # a new client object
    def NewClient( self ):
        return Client()

    # factory method to create CPU
    def NewCPU( self ):
        return self.NewClient()

    # called after a new client has been filled
    def FindClientAliases( self, client ):
        # find dynamic IP players
        try:
            IPs = dynamicIPUsers[ client.name ]
            for IP in IPs:
                ID = "IP " + IP
                # print ID
                if client.ID.find(ID) == 0:
                    client.ID = "DYN " + client.name
        except KeyError:
            pass    

        # find aliases
        try:
            # print client.ID
            original = aliases[ client.ID ]
            # print client.ID, "replaced"
            client.ID = original
        except KeyError:
            pass    
        
    def OnNewClient( self, client ):
        self.FindClientAliases( client )

    def OnNameChange( self, client, oldName, newName ):
        self.FindClientAliases( client )

    # called after a new CPU has been filled
    def OnNewCPU( self, cpu ):
        self.OnNewClient( cpu )

    # called when a player quits 
    def OnQuit( self, player, reason ):
        if reason != 0:
            player.dropped = True        

    # called on allieance changes
    def OnAllianceChange( self, player, oldAlliance, newAlliance ):
        pass
    
    # called when clients go out of sync
    def OnOutOfSync( self, player ):
        player.dropped = True
    
    # called at the end of parsing; parameter is true if scores were set and signed
    # and maps from clientID resp. teamID to clients
    def OnEnd( self, scoresSigned, clients, playerDictionary ):
        # players is passed as a dictionary. we want it as a list.
        players = []
        for p in playerDictionary:
            player = playerDictionary[p] 
            if player.IP == "CPU":
                player.ID = "CPU " + player.name
            players.append( player )
        
        # no scores?
        if not scoresSigned:
            # if we're here, that means no scores were entered. Shame on the players!
            # well, unless it was only one player.
            if  len( players ) <= 1:
               return

            # or if the user set a magic environment variable telling us he doesn't
            # want missed games to count.
            if os.getenv("RATING_DONT_COUNT_MISSING") not in ["",None]:
                return

            # count dropped players
            dropped = 0
            for player in players:
                if player.dropped:
                    dropped = dropped + 1

            # if at least two players (or one in a duel) dropped, there may have been technical difficulties.
            # cut some slack.
            if dropped >= 2 or dropped * 2 >= len(players):
                return

            # ok, now shame on them. Log the game as missed for all of them.
            for player in players:
                # print player.ID
                self.database.cursor.execute("UPDATE players SET missed = missed + 1 WHERE id = ?", [player.ID] )

            # self.database.Save()
            return

        # scores were entered. Helper functions
        def PlayerCompare( a, b ):
            if b.score > a.score:
                return 1
            if b.score < a.score:
                return -1
            return 0

        def AddPlayer( player ):
            # see if the player is already registered
            self.database.cursor.execute("SELECT * FROM players WHERE id == ?", [player.ID] )
            result = self.database.cursor.fetchone()
            if result != None:
                # there is. just update the name
                self.database.cursor.execute( 'UPDATE players SET name = ?, games = ? WHERE id == ?', ( player.name, result[3] + 1, player.ID ) )
            else:
                # no? Make a new entry.
                self.database.cursor.execute( 'INSERT INTO players VALUES( ?, ?, 0, 1, 0, 0, 0 )', ( player.ID, player.name ) )

        # abort if not at least two players are present
        if  len( players ) <= 1:
            print "Need at least two players to do anything useful."
            return

        # log player participation
        for p in players:
            p.worthAdding = False

        def HandleGame( players, prefix, playerCount ):
            if playerCount < 2:
                return

            players.sort( PlayerCompare )

            # determine minimal score
            minScore = 0
            scoreSum = 0
            for p in players:
                scoreSum = scoreSum + p.score
                if p.score < minScore:
                    minScore = p.score

            # adds a game between id1 and id2 to the database
            def AddGame( p1, p2, win1, win2, err1up, err1down, err2up, err2down, type ):
                id1 = p1.ID
                id2 = p2.ID
                if id1 > id2:
                    AddGame( p2, p1, win2, win1, err2up, err2down, err1up, err1down, type )
                else:
                    # fetch and apply prescale
                    prescale = float(config.get("prescale", type ))

                    # apply demo prescale
                    if p1.keyID == 'DEMO' or p1.keyID == 'DEMO':
                        prescale = prescale * float(config.get("prescale", "demo" ))

                    if prescale == 0:
                        return
                    if prescale < 0:
                        # flip wins and prescale
                        prescale = -prescale
                        (win1,win2) = (win2,win1)

                    p1.worthAdding = True
                    p2.worthAdding = True

                    win1 = win1 * prescale
                    win2 = win2 * prescale
                    err1up = err1up * prescale * prescale
                    err2up = err2up * prescale * prescale
                    err1down = err1down * prescale * prescale
                    err2down = err2down * prescale * prescale

                    # see if there already is a game between the two registered
                    self.database.cursor.execute("SELECT win1, win2, err1up, err2up, err1down, err2down  FROM games WHERE id1 == ? AND id2 == ? AND type == ?", (id1, id2,type ) )
                    result = self.database.cursor.fetchone()
                    if result != None:
                        # there is. append to it.
                        self.database.cursor.execute( 'UPDATE games SET win1 = ?, win2 = ?, err1up = ?, err2up = ?, err1down = ?, err2down = ? WHERE id1 == ? AND id2 == ? AND type == ?', ( win1 + result[0], win2 + result[1], err1up + result[2], err2up + result[3], err1down + result[4], err2down + result[5], id1, id2, type ) )
                    else:
                        # no? Make a new entry.
                        self.database.cursor.execute("INSERT INTO games VALUES(?, ?, ?, ?, ?, ?, ?, ?, ? )", ( id1, id2, win1, win2, type, err1up, err2up , err1down, err2down ) )

            # determine winners and losers for 'win' stats
            winners = [ players[0] ]
            losers = players[1:]
            while len(losers) > 0 and losers[-1].score >= winners[0].score:
                winners.append(losers.pop())

            global warnFishy
            # check for players from the same IP, abort if there was one
            for winner in winners:
                for loser in losers:
                    if winner.IP == loser.IP:
                        print "A winner and non-winner came from the same IP address. This looks fishy, not counting this game."
                        raise Fishy()
                        return

            # log win stats
            for winner in winners:
                for loser in losers:
                    AddGame( winner, loser, 1, 0, 0, playerCount*playerCount/len(winners), playerCount*playerCount/len(losers), 0, prefix + "win" )

            # log draw stats for winners
            for i in range(0,len(winners)-1):
                a = winners[i]
                rest = winners[i+1:]
                for b in rest:
                    AddGame( a, b, .5, .5, playerCount, playerCount,  playerCount, playerCount, prefix + "win" )

            def Clamp( x ):
                if x > 0:
                    return x
                else:
                    return 0

            scoreError = scoreSum * scoreSum / ( playerCount - 1 );

            # run through all player pairs and log score differences
            for i in range(0,len(players)-1):
                a = players[i]
                rest = players[i+1:]
                for b in rest:
                    if a.score > b.score:
                        AddGame( a,b, 1, 0, 0, playerCount-1, playerCount-1, 0, prefix + 'position' )
                    else:
                        AddGame( a,b, .5, .5, playerCount-1, playerCount-1, playerCount-1, playerCount-1, prefix + 'position' )


                    # a and b are now a unique pair of two players, add the various score logs
                    AddGame( a, b, a.score - minScore, b.score - minScore, scoreError, scoreError, scoreError, scoreError, prefix + 'score' )
                    AddGame( a, b, Clamp(a.score),     Clamp(b.score),     scoreError, scoreError, scoreError, scoreError, prefix + 'clamped' )
                    AddGame( a,b, a.score - b.score, 0,                    scoreError, scoreError, scoreError, scoreError, prefix + 'difference' )

        # process single player scores
        HandleGame( players, "", len(players) )

        # for the second pass, replace scores by team score average
        allianceScores = [0] * 10
        allianceCount = [0] * 10
        numberOfAlliances = 0
        for player in players:
            ID = player.allianceID
            if allianceCount[ID] == 0:
                numberOfAlliances = numberOfAlliances + 1
            allianceScores[ID] = allianceScores[ID] + player.score
            allianceCount[ID] = allianceCount[ID] + 1
        for player in players:
            ID = player.allianceID
            player.score = allianceScores[ID]/float(allianceCount[ID])

        # process team scores
        HandleGame( players, "team", numberOfAlliances )

        # log player participation
        for p in players:
            if p.worthAdding:
                AddPlayer( p )

        if self.stabilize:
            scores = AtanScores( self.database )

            playersData = []
            for c in players:
                playersData.append( self.database.GetPlayer(c.ID) )

            Stabilize( self.database, scores, playersData, False )

        # self.database.Save()

    # called with a tokenized line if the log entry was not
    # processed
    def OnUnknown( self, lineSplit ):
        pass

# purge old games
def Purge( database ):
    # purge inactive players
    database.cursor.execute("SELECT id FROM players WHERE games < ?", [float(config.get("decay", "player"))] )
    deletedPlayers = database.cursor.fetchall()
    for player in deletedPlayers:
        database.cursor.execute("DELETE FROM games WHERE id1 == ? OR id2 == ?", [player[0],player[0]] )
        database.cursor.execute("DELETE FROM players WHERE id == ?", [player[0]] )
            
    # purge insignifficant games
    database.cursor.execute("DELETE FROM games WHERE win1 + win2 < ?", [float(config.get("decay", "games"))] )

def Decay( factor ):
    database = Database()

    # let data decay
    database.cursor.execute("UPDATE players SET games = games * ?, missed = missed * ?", [factor, factor] )
    database.cursor.execute("UPDATE games SET win1 = win1 * ?, win2 = win2 * ?, err1up = err1up * ?, err2up = err2up * ?, err1down = err1down * ?, err2down = err2down * ?", [factor, factor, factor*factor, factor*factor, factor*factor, factor*factor] )

    # purge old games
    Purge( database )

    database.Save()

# selective amnesia. Lets each players' games decay so the
# total weight of them approaches totalWeight. All games
# decay at least at the 'base' rate. Games between players
# with far away rankings decay with a rate scaled with 'distance'.
def AmnesiaCore( totalWeight, base, distance ):
    runAgain = False

    database = Database()

    # get all players
    scores = AtanScores( database )

    players = []
    for p in scores.players:
        player = scores.players[p]

        # the decay rate of an influence with given score
        def GetDecayRate( score ):
            return base + distance * math.log( math.cosh( (player.score - score )/influence.range ) )

        influences = database.GetInfluences( player, None )

        # calculate total weight and decay possibility
        weight = 0
        totalDecay = 0
        for influence in influences:
            weight = weight + influence.weight
            decay = influence.weight * GetDecayRate( influence.score )
            totalDecay = totalDecay + decay
            influence.decay = decay

        if weight > totalWeight:
            # only then, we need to do something
            # print player.name, weight, player.score, totalDecay

            scale = ( weight - totalWeight )/totalDecay

            # collect actual total decay
            totalWeightLoss = 0
            
            for influence in influences:
                if influence.weight > 0:
                    decay = influence.decay * scale/ influence.weight
                    # print influence.score, decay, influence.otherID
                
                    factor = 1 - decay
                    if factor < 0:
                        # too much decay. Clamp and run again.
                        factor = 0
                        runAgain = True

                    totalWeightLoss = totalWeightLoss + (1-factor) * influence.weight

                    database.cursor.execute("UPDATE games SET win1 = win1 * ?, win2 = win2 * ?, err1up = err1up * ?, err2up = err2up * ?, err1down = err1down * ?, err2down = err2down * ? WHERE ( id1 == ? AND id2 == ? ) OR ( id1 == ? AND id2 == ? )", [factor, factor, factor*factor, factor*factor, factor*factor, factor*factor, player.id, influence.otherID, influence.otherID, player.id ] )

            # decay player
            factor = 1-totalWeightLoss/weight
            # print factor
            database.cursor.execute("UPDATE players SET games = games * ?, missed = missed * ? WHERE id == ?", [factor, factor, player.id] )


    # purge old games
    Purge( database )
    database.Save()

    return runAgain

def Amnesia():
    totalWeight = float( config.get("amnesia", "MaxWeight" ) )
    base = float( config.get("amnesia", "Base" ) )
    distance = float( config.get("amnesia", "MaxWeight" ) )

    maxTries = 10
    while AmnesiaCore( totalWeight, base, distance ) and maxTries > 0:
        maxTries = maxTries - 1

def Run():
    i = 1
    while i < len(sys.argv):
        command = sys.argv[i]
        if command == 'parse':
            parser = Parser(False)
            parser.Loop()
        if command == 'parsestab':
            parser = Parser(True)
            parser.Loop()

        if command == 'stabilize':
            StabilizeAll()

        if command == 'stabilizelazy':
            StabilizeAllLazy()

        if command == 'decay':
            i = i + 1
            if i >= len(sys.argv):
                print "decay expects an additional argument."
                return
            Decay( float(sys.argv[i]) )

        if command == 'amnesia':
            Amnesia()

        if command == 'print':
            Print()

        if command == 'freeprint':
            i = i + 2
            if i >= len(sys.argv):
                print "freeprint expects two additional arguments."
                return
            FreePrint( sys.argv[i-1], sys.argv[i] )

        if command == 'plot':
            i = i + 1
            if i >= len(sys.argv):
                print "plot expects one additional argument."
                return
            Plot( sys.argv[i] )

        i = i + 1

    if len( sys.argv ) <= 1:
        print "Available commands:"
        print "parse          : parses event log from stdin and enters games into the database. The log needs to be timestamped with a stamp consisting only of non-whitespace characters, followed by a bit of whitespace"
        print "stabilize      : updates the player ratings based on the games played"
        print "parsestab      : parses event log and updates the scores of the players that participated"
        print "print          : prints a list of players and scores"
        print "freeprint <format for rated players> <format for unrated <layers> : prints the list in a custom format"
        print "decay <factor> : multiplies all stats with the given factor. Use this to make past games less important than current games. Using 'decay .9' every month gives all data a half life of half a year."
        print "amnesia        : Selective amnesia of games between players of big ranking difference."

Run()
    
