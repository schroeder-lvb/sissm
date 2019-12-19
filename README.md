# sissm
Simple Insurgency Sandstorm Server Manager "SISSM" (FPS Gaming)

This project is currently in Alpha stage of development --

SISSM is a server application designed to run alongside your Insurgency Sandstorm dedicated server software.  It is designed to be an "unattended watchdog" for your server.  It is not a remote management tool, nor a server setup tool.

*  Compiled for Linux and Windows servers
*  Auto-reboots ISS servers for stability, as non-intrusively as possible, with early warnings sent to your game clients
*  Adaptive Checkpoint mode Anti-rusher algorithm adjusts territorial objective capture rate
*  Auto-kicks bad-name players
*  Reserved slots for approved list of players (typically Admins.txt)
*  Sent to your game clients: greetings (2 lines at start) and server rules (up to 10 items)
*  Sent to your game clients: player connects and disconnects -- with incognito option for a short list of admins
*  "First Player Downgrade" makes gameplay easier for a lone player - helps to prime the empty servers
*  "Ruleset Override" sets server properties locked by some rulesets, such as bot count in Frenzy mode
*  "Command line Administrator" allows your admins in-game control of the server including rudiementary macro execution
*  HTML server status generator
*  Co-op Checkpoint only: automatically adjust bot scale/difficulty per map, change bot count per objective segment
*  Allows 2-letter typed shortcuts to display predefined tacticaly communications to be printed in-game, for those not using voice
*  Generates a player connection log to help facilitate after-the-fact ban
*  Configured through a single .cfg file, per server.  All plugins can be enabled/disabled individually.

Miscellaneous Features:

*  All functional features are implemented through plugins.  You can write your own, or modify an existing one!
*  Localization: Most SISSM messages sent to your game clients are all in SISSM's config file.  This makes localization possible

IMPORTANT NOTE to Commercial Game Server Providers (GSPs):

SISSM allows a user to specify system commands or a script in order to reboot the game server.  If your environment restricts shell access to your clients, it is extremely important to use the restricted variant of SISSM (sissm_restricted/sissm_restricted.exe).  A malicious actor may replace the reboot command with an abrbitary OS commands.  If you are compiling from the source, set SISSM_RESTRICTED macro at top of sissm.c.

