# sissm
Simple Insurgency Sandstorm Server Manager "SISSM" (FPS Gaming)

SISSM is a server application designed to run alongside your Insurgency Sandstorm dedicated server software.  It is designed to be an "unattended watchdog" of your server.  It is not a remote management tool, nor a server setup tool.

SISSM must execute on the same machine hosting your Sandstorm server.  This is because SISSM requires local access to read the Insurgency.log file to track game state in real-time.  SISSM will NOT work in most GSP-hosted environments, not recommended for GSP use due to its complex nature.

*  Compiled for Linux and Windows servers
*  Auto-reboots ISS servers for stability, as non-intrusively as possible, with early warnings sent to your game clients
*  Adaptive Checkpoint mode Anti-rusher algorithm adjusts territorial objective capture rate, with optional offender auto-kick 
*  Auto-kicks bad-name players - you provide the list of forbidden words and player names
*  Reserved slots for approved list of players (typically Admins.txt)
*  Sent to your game clients: greetings (2 lines at start) and server rules (up to 10 items)
*  Sent to your game clients: player connects and disconnects -- with incognito option for a short list of admins
*  "First Player Downgrade" makes gameplay easier for a lone player - helps to prime the empty servers
*  "Ruleset Override" sets server properties locked by some rulesets, such as bot count in Frenzy mode
*  "Command line Administrator" allows your admins in-game control of the server including rudiementary macro execution
*  "Dynamic Bot" allows adjusting min/max bot counts per map per objective-leg -- also works with wave respawn checkpoint
*  "Tactical No-Mic" allows non-microphone users to communicate by typing 2-letter codes of common tactical phrases
*  HTML status page generator
*  Setup custom admin privilege levels each group with attributes and list of allowed commands
*  Includes RCON Terminal mini-tool for server operators and modders
*  Generates a player connection log to help facilitate after-the-fact ban
*  Configured through a single .cfg file, per server.  All plugins can be enabled/disabled individually.

Precompiled Binaries:
   https://github.com/schroeder-lvb/sissm_releases

Miscellaneous Features:

*  All functional features are implemented through plugins.  You can write your own, or modify an existing one!
*  Localization: Most SISSM messages sent to your game clients are all in SISSM's config file.  This makes localization possible

IMPORTANT NOTE to Commercial Game Server Providers (GSPs):

SISSM allows a user to specify system commands or a script in order to reboot the game server.  If your environment restricts shell access to your clients, it is extremely important to use the restricted variant of SISSM (sissm_restricted/sissm_restricted.exe).  A malicious actor may replace the reboot command with an abrbitary OS commands.  If you are compiling from the source, set SISSM_RESTRICTED macro at top of sissm.c.

