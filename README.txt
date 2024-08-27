Unix Installation:

None required. Dedcon does not use any data files, 
you can run it from anywhere. If you feel like it, move the 
binary to /usr/local/bin/. It doesn't care.

-------------------------------------------------------------------------------

Invocation:

./dedcon.exe <configuration file(s)> options
Where <configuration file(s)> is the list of your configuration files.
Have a look at ConfigFile.sample for the documentation of what can go
into them. A regular game will be started with the settings from the
configuration file(s), and the program exits as soon as all clients
have quit. To run a continuous server, you should invoke this in a loop
from a script; for your convenience, the included file loop.sh is just
such a script that passes all command line arguments to the server.

Command line options can be mixed freely among the configuration files.
The available command line options are:

-c <command> : Executes the given command as if it appeared in a configuration
               file. Configuration files and these direct commands are executed 
               in the order they were given, and wait commands inside the 
               configuration files will not block reading of other configuration
               files.
-l <recording> : Loads the specified recording for playback; clients connecting
                 to the server will be shown the recording as if they had beed
                 spectators to the recorded game.

Dedcon expects all input to be in the latin1 character set 
and it produces latin1 output.

-------------------------------------------------------------------------------
Chat commands:

Everyone has some extra chat commands available, like the default /me and /name:

/ping: Gives you a list of the connected clients, player names and pings

/ignore: Ignores a player by name part. "/ignore Bert" will get rid of all my
         future chats for you.

/unignore: undoes /ignore.

/speed:  gives you information about the speeds players have requested and 
         the amount of slowdown budget they have left.

/kick:   issues a kick vote against a player based by a part of the player's 
         name. Kick votes need to be enabled by the server administrator 
         with KickVoteMin.

/ranked and /unranked: if all players say /unranked before the game, a marker
         is left in the logs telling parsers that this game should not count
         for rankings.

All commands that affect another player/spectator also have a variant
that takes a client ID instead. The name is that of the original command,
with "id" appended (e.g. /kickid for kicking someone by ID). You can get 
a list of clients and their IDs with /ping.

------------------------------------------------------------------------------

Administering the server:

For administrators, there are even more chat commands:

/login <password>: Needs to be said before the other commands. See
                   ConfigFile.sample to see how to configure admin access.

/info: Detailed information about the connected clients (IP, keyID, version,
       admin status)

/set: Allows you to change settings, if you know what you are doing, even during
      the game. For example, saying "/set MaxTeams 6" opens the server to the
      maximal amount of players. What comes after the /set is interpeted as if
      it appeared in a configuration file.

/include: Includes preset configuration files. Works like the Include config
          file command. This is largely equivalent to "/set Include <file>",
          however is allowed to be used by lower level admins by default.

/op: Gives administrator rights to another player. It is recommended that you do
     not use your admin power while you are playing; instead, /op a trusted
     spectator.

/deop: Take admin rights away from a player.

/promote: raise the admin level of a player.

/demote: lower the admin level of a player.

/kick: Kicks a player from the server based on the name. "/kick ert" will 
       kick the player with "ert" in the name, probably the author.

/remove: While in the lobby, this removes a player from the game, but not
         the server. This is intended as a final warning before /kick is
         used and requires the same rights (Rationale: a /remove is likely
         to make the player very angry, and if it can't be followed by
         a /kick, the situation after the /remove would be worse than before.)

/logout: Can be used to get rid of admin privileges again. Like on IRC, it is
         not good to run around with operator rights all of the time, only get
         them when you need them.

-------------------------------------------------------------------------------

Random Fact:

Nobody reads READMEs. Sice you are reading this, therefore, you ARE nobody :)

-------------------------------------------------------------------------------------

Player Scores:

The scores of players cannot be determined automatically for reasons explained in
the FAQ at http://dedcon.homelinux.net:8000/dedcon/wiki/FAQ . In those cases where
there is some reason to have the server know about the scores, (semi-automated
tournaments, for example), the players have to enter and sign them. Three
chat commands take care of that:

/score:  gives you a listing of all player scores as they have been entered by 
         the players. It lists the team IDs, client IDs, player names and the scores.

/setscore: gives a team a new score; to set the score of Player "DeathKiller" to 
           120 points, you need to say
           /setscore DeathKiller 120
           partial matches work, too, so
           /setscore Death 120
           serves the same purpose, unless another player named "MegaDeath" is in the game.
           In case no good unique partial name can be found or players pick hard to type
           names, there also is an alternative command using numeric IDs:

/setscoreid: to set the score Team 0, the team that is usually assigned to the 
             first player entering a server, to 120 points, you need to say
             /setscoreid 0 120

/signscore: signs the scores. Makes it known that the last scores you saw when you
            said /score are acknowledged by you as the true scores.

Score signatures get voided whenever there is game action, so signing scores during
the game is largely pointless. Score signatures also get voided when the scores are
changed. If someone secretly changes the scores between you checking them with
/socre and signing them with /signscore, the signature gets rejected and the current
scores are displayed instead.

The scores that were signed by most players get logged into the event log for 
automatic processing.

-------------------------------------------------------------------------------------

Recordings and playback:

To record a game, simply use the "Record" setting in your configuration file or
on the command line (with the -c option), it expects the file name the game should be
recorded into as argument. Various %-directives in the filename are replaced with
elements of the current time and date, see http://de3.php.net/strftime for a full
list of possibilities. A simple and effective sequence to use is %F.%H.%M.%S.
The recommended file extension is .dcrec; Windows users are able to play files with
those extension back with a simple doubleclick.

An important note is that the recording will only be saved on a regular exit of the
server, either when all clients have quit or the Quit command is executed. If you
abort the server with a kill signal or CTRL-C, the recording will be lost.

To play a recording back, use the -l command line switch of the dedicated server.
You can also pass regular configuration files along with that. Settings that affect
the game itself will be ignored, of course. By default, the playback server will
be announced on the LAN and the Internet and bear the same name as the server the
recording was made with, with "playback" appended. You can change all of that in
the configuration files.

When a client connects to the server started in this way, the playback will start
and jump directly to the beginning of the game. Multiple clients can connect and
even chat with each other, they will appear to each other as regular spectators.

The spectators can change the speed of the playback by saying "/speed <ticks per second>".
The normal speed is 10 ticks per second, lower values cause the clients' rendering
to stutter and are not recommended. Higher values should work fine, but will look
increasingly odd because the clients extrapolate the game a bit too far into the future.
Note: It is not technically possible to change the regular speed multiplier, any change 
to that would be a change to the game mechanics and cause horrible failures.
You can select the initial playback speed with the configuration option PlaybackSpeed.

If you tell the server so with a "Record" setting, it will make a recording of the 
playback session, too. The new spectator chat will be interleaved with the previous
recording in places where it does not disturb the game. The process can be iterated
as long as there are free spots in the recording to squeeze in the new spectator 
chat, in practice that is as often as you like. You can use this to publish recording
with a commentary.

-------------------------------------------------------------------------------

Log parsers:

Dedcon produces machine readable log files (called event log in the sample config file),
as examples how they can be put to use, two GPLed Python scripts are included in this
distribution. Don't expect too much from them; they do their job for the author, but may
be utterly useless for you.

The first script is a banbot; its name is grieferdatabase.py. Documentation can be found
at the start of the file and the sample configuration file grieferdatabase.ini. Note
that the format of the configuragion file is a standard .ini file where it matters in wich
section the configuration variables appear. The sole reason for that is the author's
lazyness, Python comes with a parser for those files.

The second script is a game result logger and ranking table generator. Again, some
documentation is in the source rating.py, some in the sample configuration file
rating.ini. But perhaps a word or two about the general ideas behind the program
would be useful here.

-------------------------------------------------------------------------------

rating.py:

The principle is simple. Let's only look at the 'win' stat for now, and only consider
two player games. For each player pair that met in a game where the scores have been 
entered via the /setscore chat interface, the system keeps track of how may games
each of them won. So after a while, the database may contain the information that
 * A won 3 times against B
 * B won 2 times against A
 * C won 2 times against A
 * A won 3 times against C
Note that B and C have not played each other, but the stats hint that A is a better player 
than B and that C is a better player than A, hence C probalby is a better player than B.

What the system then does with this information is this: it assigns each player a 
score, a numerical value. It picks the scores so that for each player X, a sum of forces
from the other players is zero; the force from each other player Y is given by
<times lost> - <times won> + <times played> * tanh( <score difference> / <range> )
where <times lost> is the number of times X lost to Y, <times won> is the number of times
X won against Y, <times played> is the sum of the two, <score difference> is the score of X
minus the score of Y, and <range> is a configurable constant (default 100). In our example,
taking X = B and Y = A, we have <times lost> = 3, <times won> = 2, <times played> = 5,
so to make the total force for B vanish, a little experientation shows that B's score needs to be
about 0.203 * <range> or 20 points with default settings below A's score. Likewise, C's score
will be 20 points above A and the system reproduces the observation that C probably is better
than B by giving him 40 points more.

The system also is aware of statistical errors; games often end up close, and a game
between players of roughly equal strenght can go either way. These errors are incorporated
by looking at each player in isolation and pretending he won a statistically just barely
relevant amount of games more or less than he had, and calculating his score with the assumed
lower or higher number of wins.

The other stats are explained in the .ini sample file and are treated in similar 
fashion by the system, you just have to replaced "games won/lost" by the other
stats. 

