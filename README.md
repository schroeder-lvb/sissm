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
*  "Command line Administrator" allows your admins a limited server control such as adjusting bot counts (ISS 1.4+ req'd)
*  HTML server status generator
*  Generates a player connection log to help facilitate after-the-fact ban
*  Configured through a single .cfg file, per server.  All plugins can be enabled/disabled individually.

Miscellaneous Features:

*  All functional features are implemented through plugins.  You can write your own, or modify an existing one!
*  Localization: All SISSM messages sent to your game clients are all in SISSM's config file.  This makes localization possible



***** IMPORTANT Cybsersecurity Note for Commercial Gamer Server Hosting Companies ******

SISSM was deisgned for self-hosting game hobbyists, who has full ownership control of his/her own system executing the dedicated game server instances (such as via the Cloud Linux VPS or a home-based server).  One of SISSM’s main function is to reboot the game server periodically to maintain stability.  SISSM's “hard-reboot” option allows the operator (who has control of sissm.cfg file) to specify an external reboot OS command or a script.  This is a necessary feature to reliably reboot a "dead" game server instance.

This is a major security problem if you are providing a shell-restricted service to your clients.   A clever exploiter may replace this with OS command string to gain shell access to your machine!

If you are using SISSM under a restricted turn-key environment:
*  Provide a separate reboot service of the game server through your web admin, and/or timed automation, and 
*  Build a modified SISSM that removes reboot command execution:  edit the file sissm.c and replace 
    strlcpy( sissmConfig.restartScript, cfsFetchStr( cP, "sissm.restartscript", "" ), CFS_FETCH_MAX ); 
with:
    strcpy(  sissmConfig.restartScript, “” );
