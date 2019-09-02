# sissm
Simple Insurgency Sandstorm Server Manager "SISSM" (FPS Gaming)

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
*  HTML server status generator
*  Generates a player connection log to help facilitate after-the-fact ban
*  Configured through a single .cfg file, per server.  All plugins can be enabled/disabled individually.

Miscellaneous Features:

*  All functional features are implemented through plugins.  You can write your own, or modify an existing one!
*  Localization: All SISSM messages sent to your game clients are all in SISSM's config file.  This makes localization possible ***


