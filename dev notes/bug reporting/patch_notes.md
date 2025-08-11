## 22/06/2025

You can now watch DEFCON replays in the browser on https://defconexpanded.com/
you don't need any additional software you just need a web browser and a computer!

When you visit DefconExpanded you will have two options when viewing a game on the homepage:

- Download (download the raw recording)
- Watch online (uses the new replay viewer)

You can watch Vanilla games and UK Mod games, however Eight and Ten player games are not supported yet.
The replay viewer is currently in an Alpha release stage, there is some bugs to iron out but they are small but they are small enough that they don't matter to much for an Alpha release!

Using the replay viewer on mobile is buggy and not entirely supported so...
mobile support will be up next once the bugs are ironed out :pepe: 

Feel free to try it out and tell me your opinions, if you want a feature added or if you find a bug just let me know :slight_smile:

## 29/06/2025

Replay viewer has been updated from Alpha 1.04 to Alpha 1.45

Fixes:
- Added smart rendering logic that should at minimum give you a 5x increase in fps, most people that tested the first version got around 30 to 70 fps. Now you should see upwards of 200fps
- Reduced CPU operations by removing code that is never used in the replay viewer

Known issues:
- Hovering over the people tooltip will flicker random unit textures
- End of game scores are incorrect to the websites scores

Whats next:
- Fast forward and rewind buttons
- Seeking, allowing the user to drag a slider to skip to whatever point in the recording
- Add Potato PC graphics settings for those who really need all the help they can get
- Get the mobile version actually usable

## 01/07/2025

Replay viewer has been updated from Alpha 1.45 to Alpha 1.51

Fixes:
- Fixed the population button on the toolbar rendering random unit textures, this was due to my ignorance while adding buffers for the sprite rendering. So I just switched this back to immediate mode rendering.

Known issues:
- End of game scores are incorrect to the websites scores, still digging into this one

Whats next:
- Fast forward and rewind buttons
- Seeking, allowing the user to drag a slider to skip to whatever point in the recording
- Add Potato PC graphics settings for those who really need all the help they can get
- Get the mobile version actually usable

## 02/07/2025

Replay viewer has been updated from Alpha 1.51 to Alpha 1.74

Fixes:
- Thanks to @megasievert , I have fixed a bug that caused buffer corruption when enabling the orders overlay. This was due to the order icons using immediate mode rendering while also existing inside the same texture buffer as the units.
- removed unnecessary buffer flushes for the rectangles and lines system, this dropped the draw calls almost in half when gunfire, unit trails and orders are being rendered
- removed some textures from the replay viewer that aren't being used

Known issues:
- End of game scores are incorrect to the websites scores, still digging into this one
- Texture atlas system proving extremely difficult, this is the final optimisation

Whats next:
- Fast forward and rewind buttons
- Seeking, allowing the user to drag a slider to skip to whatever point in the recording
- Add Potato PC graphics settings for those who really need all the help they can get
- Get the mobile version actually usable

if you want to see the bug @megasievert  fixed in action check the image below

## 03/07/2025

Big update for the replay viewer, updated from Alpha 1.74 to Alpha 1.83

Fixes:
- Fixed a bug that I created when creating the playback window, when trying to move the window it would snap the top left of the window to your cursor.
This was my fault for not understanding defcons window system correctly when creating the said window.

Features:
- Fast-forwarding/seeking is now implemented for the lobby phase and the game phase. you can now skip to any point of the recording just like how a youtube video works. Was a lot simpler than I thought, it uses the exact same system for skipping to the game start. Except we now add a seek bar that maps to the sequence ids from start to finish
- Increased the speed of the fast forward/connection phase, the old implementation limited us to 20 fps. I assume it was done for syncing upto real games that handle real network packets. Here we are connecting to local games so this shouldn't matter and its a lot faster

Known issues:
- (small bug) Hovering over single units does not show the waypoint crosshair for the unit order, however the orders overlay works without issue
- (medium) UI still unoptimized, turning off the chat window increases your fps by a good margin

Whats next:
- Rewinding, this might be almost impossible
- Game segments to skip to? I don't think we can do this just by parsing the dcrec file
- Add Potato PC graphics settings for those who really need all the help they can get
- End of game scores are incorrect to the websites scores, still digging into this one
- Texture atlas system proving extremely difficult, this is the final optimisation
- Get the mobile version actually usable

## 06/07/2025

The replay has finally entered beta stage! Updated from Alpha 1.74 to Beta 1.00

Fixes:
- Hovering over single units did not show the waypoint crosshair for the unit order
- (big) Added a texture atlas for all the unit sprites in the map scene, this reduced draw calls by 60 percent.
   Tested with the latest 3v3 (with the chat window turned off) 700 draw calls, 350 fps

More information:
- A draw call in simple terms is a message that the CPU sends to the GPU to render something, too many and you actually
  get a CPU bottleneck!
- Defcon has single unit image files, this meant adding them to a buffer would mean flushing them upon texture change.
  This resulted in extreme draw calls for simple unit rendering. with a texture atlas every unit can use 1 image with 
  different coordinates for each sprite.

Known issues:
- (big) Chat window will actually render all the messages regardless of being in view, as the game progresses the fps will drop
- (medium) UI still unoptimized, turning off the chat window increases your fps by a good margin

Whats next:
- Rewinding
- Game segments to skip to? I don't think we can do this just by parsing the dcrec file
- POV mode, ability to watch a replay from a teams/players radar range. putting yourself in their shoes
- Manual replay uploads
- Get the mobile version actually usable

## 06/07/2025

Update for replay viewer! Updated from Beta 1.00 to Beta 1.35

Fixes:
- Added batching to the chat window text, this fixes the chat window fps issues as the game progresses

Features:
- (big) You can now finally watch a replay from another players radar perspective! Now instead of guessing
  what they saw during the live game, you can put yourself in their shoes!

Whats next:
- Rewinding
- Game segments to skip to? I don't think we can do this just by parsing the dcrec file
- Manual replay uploads
- Get the mobile version actually usable (this is getting closer to reality)

check the image below for what it looks like!

## 07/07/2025

Update for replay viewer! Updated from Beta 1.38 to Beta 1.45

Features:
- You can now see the health of certain units in replays by toggling a button!
  The units that have health tracking are as follows...
- Silos
- Airbases
- Battleships
- Carriers

Now this means you can determine how lucky the enemy was by checking how low their units were.
Its a small feature but its a feature nonetheless!

Whats next:
- Game segments to skip to? Im pretty sure dedcon is able to track defcon levels
- Manual replay uploads
- Get the mobile version actually usable (this is getting closer to reality)
- Rewinding

Check the image below for what this looks like in action!

