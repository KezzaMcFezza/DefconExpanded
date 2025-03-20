#!/usr/bin/python

# module for parsing event logs

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

import sys
import string

class ClientBase:
    def __init__( self ):
        self.IP         = "127.0.0.1"
        self.clientID   = -1
        self.score      = -1
        self.teamID     = -1
        self.allianceID = -1
        self.keyID      = "DEMO"
        self.name       = "NewPlayer"
        self.version    = "Unknown"

    # calculates a unique ID string for this player
    # assumes IP, keyID and version are set.
    def GetID( self ):
        if self.version.find(" steam") > 0 or self.keyID == "DEMO":
            self.ID = "IP " + self.IP
        else:
            self.ID = "KeyID " + self.keyID

        return self.ID


class Parser:
    # Methods for your parser to override; they get called when
    # you call Parse()

    # factory method: overload in your subclass to create
    # a new client object
    def NewClient( self ):
        return ClientBase()

    # factory method to create CPU
    def NewCPU( self ):
        return self.NewClient()

    # called after a new client has been filled
    def OnNewClient( self, client ):
        pass

    # called after a new CPU has been filled
    def OnNewCPU( self, cpu ):
        self.OnNewClient( cpu )

    # called when a team is created
    def OnTeamEnter( self, client, teamID ):
        pass

    # called when a team is destroyed
    def OnTeamLeave( self, client, OldTeamID ):
        pass

    # called when a player quits 
    def OnQuit( self, player, reason ):
        pass

    # called when a player griefs
    def OnGrief( self, player, grief ):
        # player is the player, grief the grief ID string
        pass

    # called on name changes
    def OnNameChange( self, player, oldName, newName ):
        pass

    # called on allieance changes
    def OnAllianceChange( self, player, oldAlliance, newAlliance ):
        pass
    
    # called on setting change messages
    def OnSetting( self, setting, value ):
        pass

    # called when clients go out of sync
    def OnOutOfSync( self, player ):
        pass
    
    # called when clients repair their sync errros
    def OnBackInSync( self, player ):
        pass

    # called when a score is set
    def OnScore( self, player, score ):
        pass

    # called at the start of the actual game
    def OnStart( self ):
        pass

    # called at the end of parsing; parameter is true if scores were set and signed
    # and maps from clientID resp. teamID to clients
    def OnEnd( self, scoresSigned, clients, players ):
        pass

    # called with a tokenized line if the log entry was not
    # processed
    def OnUnknown( self, lineSplit ):
        pass
    
    # implementation. 

    # strips timestamp from 
    def StripTimestamp( self, line ):
        lineUnicode = unicode(line, self.charsetIn )
        # find start of game to calibrate timestamp
        if not self.started:
            findStart = lineUnicode.find( 'SERVER_START' )
            if findStart >= 0:
                for i in range(0, findStart ):
                    if not lineUnicode[i].isalnum():
                        self.timestampSize = self.timestampSize + 1
                #print self.timestampSize
                self.started = True
            else:
                return []
        #else:
        # strip timestamp from current line
        timestampNow = 0
        for i in range(0, len( lineUnicode ) ):
            if not lineUnicode[i].isalnum():
                timestampNow = timestampNow + 1
                if timestampNow >= self.timestampSize:
                    #print i, lineUnicode[i+1:]
                    return lineUnicode[i+1:].split()

        

    def Parse( self ):
        #print "Parse!"
        
        cpuID = 0
        self.charsetIn  = "latin1"

        self.game = "Unknown"
        self.version = "Unknown"

        scoresSigned = False

        # map of client IDs to clients
        clients = {}

        # map of team IDs to clients
        players = {}

        self.started = False

        # count of whitespace/punctuation in timestamp
        self.timestampSize = 0

        # parse the event log
        for line in sys.stdin:
            # convert from latin1 (Defcon and DedCon's charset) to utf8 and split it
            lineSplit = self.StripTimestamp( line )
            #try:
            #    print clients[16], lineSplit
            #except: pass

            try:
                if len(lineSplit) == 0:
                    continue
            except:
                # print "Unparseable line '%s' ignored."
                continue

            # print lineSplit

            # parse server start
            if lineSplit[0] == 'SERVER_START':
                try:
                    self.game = lineSplit[1]
                    self.version = lineSplit[2]
                except:
                    pass
            
            # parse charset
            elif lineSplit[0] == 'CHARSET':
                self.charsetIn = lineSplit[1]

            # parse new clients
            elif lineSplit[0] == 'CLIENT_NEW':
                client = self.NewClient()
                client.clientID = int(lineSplit[1])
                client.keyID    = lineSplit[2]
                client.IP       = lineSplit[3]
                client.version  = string.join(lineSplit[4:])

                # take over name from previous client of the same ID
                try:
                    oldClient = clients[client.clientID]
                    client.name = oldClient.name
                    client.teamID = oldClient.teamID
                    if client.teamID >= 0:
                        players[client.teamID] = client
                except:
                    pass

                # store it
                clients[client.clientID] = client

                client.GetID()
                self.OnNewClient( client )


            # parse new CPUS
            elif lineSplit[0] == 'CPU_NEW':
                cpuID = cpuID + 1

                client = self.NewCPU()
                client.clientID  = int(lineSplit[1])
                client.keyID     = "DEMO"
                client.IP        = "CPU"
                client.version   = "CPU"
                client.name      = str(cpuID)

                # store it
                clients[client.clientID] = client

                client.ID = "CPU " + client.name
                self.OnNewCPU( client )

            # parse client or CPU name
            elif lineSplit[0] == 'CLIENT_NAME' or lineSplit[0] == 'CPU_NAME':
                ID      = int(lineSplit[1])
                name    = string.join(lineSplit[2:])

                try:
                    client = clients[ID]
                    client.name = name
                    self.OnNameChange( client, client.name, name )
                except:
                    pass

            # parse team enter message
            elif lineSplit[0] == 'TEAM_ENTER':
                teamID  = int(lineSplit[1])
                ID      = int(lineSplit[2])

                try:
                    client          = clients[ID]
                    players[teamID] = client
                    client.teamID   = teamID
                    self.OnTeamEnter( client, teamID )
                    # print players
                except:
                    # print "Can't add team!"
                    pass
                    #print "No player", ID

            # parse team leave message
            elif lineSplit[0] == 'TEAM_LEAVE':
                teamID  = int(lineSplit[1])

                try:
                    client          = players[teamID]
                    client.teamID   = -1
                    del players[teamID]
                    self.OnTeamLeave( client, teamID )
                    # print players
                except:
                    pass
                    #print "No team", teamID

            # parse client quit
            elif lineSplit[0] == 'CLIENT_QUIT':
                ID      = int(lineSplit[1])
                reason  = 0
                if len(lineSplit) > 2:
                    reason = int(lineSplit[2])
                
                self.OnQuit( client, reason )

            # parse griefings
            elif lineSplit[0] == 'CLIENT_GRIEF':
                ID      = int(lineSplit[1])
                grief   = lineSplit[2]

                try:
                    player = clients[ID]
                    self.OnGrief( player, grief )
                except:
                    pass
                    #print "No player", ID

            elif lineSplit[0] == 'SETTING':
                self.OnSetting( lineSplit[1], string.join(lineSplit[2:]) )

            # parse sync errors, disguise as drops
            elif lineSplit[0] == 'CLIENT_OUT_OF_SYNC':
                ID      = int(lineSplit[1])
                self.OnOutOfSync( clients[ID] )

            # parse sync errors, disguise as drops
            elif lineSplit[0] == 'CLIENT_BACK_IN_SYNC':
                ID      = int(lineSplit[1])
                self.OnBackInSync( clients[ID] )

            # parse alliances
            elif lineSplit[0] == 'TEAM_ALLIANCE':
                teamID      = int(lineSplit[1])
                allianceID  = int(lineSplit[2])
                try:
                    player = players[teamID]
                    self.OnAllianceChange( player, player.allianceID, allianceID )
                    player.allianceID = allianceID
                except:
                    pass
                    #print "No player", ID


            # parse scores
            elif lineSplit[0] == 'SCORE_TEAM':
                name      = string.join( lineSplit[4:] )
                ID      = int(lineSplit[3])
                score   = lineSplit[2]
                teamID  = int(lineSplit[1])
                #print ID, teamID, score

                try:
                    client = clients[ID]
                    client.score = float(score)
                    # print ID, client, client.score, teamID, players[teamID], players[teamID].score
                    self.OnScore( client, score )
                except:
                    #print "Can't set score", ID
                    pass
                    #print "No player", ID

            elif lineSplit[0] == 'SCORE_SIGNATURE_PLAYER' or lineSplit[0] == 'SCORE_SIGNATURE_SPECTATOR':
                # someone signed the scores, they must be valid. Sort clients by score
                scoresSigned = True

            # parse game start message
            elif lineSplit[0] == 'GAME_START':
                self.OnStart()

            # parse scores
            elif lineSplit[0] == 'SERVER_QUIT':
                #print "End!"
                self.OnEnd( scoresSigned, clients, players )
                #print "End2!"
                return True

            else:
                self.OnUnknown( lineSplit )

        # no proper game
        return False

# test
if __name__ == "__main__":
    while Parser().Parse():
        pass
