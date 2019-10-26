//  ==============================================================================================
//
//  Module: PIREBOOTER
//
//  Description:
//  This plugin monitors and rebotos the host game server when needed.  Conditions checked are:
//  1.  Idle > N seconds (default 20) by looking for 0 players in game
//  2.  Busy > M minutes (default 6 hours) cumulative time since last restart (including idle)
//  3.  Dead > L minutes (default 5 minutes) by checking RCON message loopback to logfile
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <unistd.h>
// #include <time.h>
// #include <stdarg.h>
// #include <sys/stat.h>
// #include <math.h>

// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netdb.h>
// #include <fcntl.h>

#include "bsd.h"
#include "log.h"
#include "events.h"
#include "cfs.h"
#include "util.h"
#include "alarm.h"

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "pirebooter.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled
    int  logUpdateSec;                // how often, in seconds, to show periodic status to logfile

    unsigned long int  rebootIdleSec;       
    unsigned long int  rebootBusySec;
    unsigned long int  rebootDeadSec;
    unsigned long int  rebootHungSec;
    char rebootDailyHM[ CFS_FETCH_MAX ];      // mandatory and forced daily reboot in 00:00 format

    unsigned long int  timeLastReboot;                                       // when last rebooted
    unsigned long int  timeFirstIdle;                                 // when first saw '0' player
    unsigned long int  timeLastProgress;              // 'hung' detection (iface ok, ghost players)

    char rebootWarning[ CFS_FETCH_MAX ];                                // warning message (queued)
    char rebootDaily[ CFS_FETCH_MAX ];                  // daily reboot warning message (immediate)

} pirebooterConfig;


//  ==============================================================================================
//  pirebooterInitConfig
//
//  Read params from the configuraiton file, and reset timers
// 
//
int pirebooterInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pirebooter.pluginstate" variable from the .cfg file
    pirebooterConfig.pluginState = (int) cfsFetchNum( cP, "pirebooter.pluginState", 0.0 );  // disabled by default
    pirebooterConfig.rebootIdleSec = (long int) cfsFetchNum( cP, "pirebooter.rebootidlesec", (double)   20*60 );
    pirebooterConfig.rebootBusySec = (long int) cfsFetchNum( cP, "pirebooter.rebootbusysec", (double) 5*60*60 );
    pirebooterConfig.rebootDeadSec = (long int) cfsFetchNum( cP, "pirebooter.rebootdeadsec", (double)    5*60 );
    pirebooterConfig.rebootHungSec = (long int) cfsFetchNum( cP, "pirebooter.reboothungsec", (double)   20*60 );

    // read the daily reboot time, e.g., "19:03".  Add colon "19:03:" so that it can be pattern
    // checked quickly against HH:MM:SS system clockstring and not get mistaken for MM:SS check.
    //
    strlcpy( pirebooterConfig.rebootDailyHM, cfsFetchStr( cP, "pirebooter.rebootDailyHM", "99:99"), CFS_FETCH_MAX );
    if ( 5 == strlen( pirebooterConfig.rebootDailyHM )) strlcat( pirebooterConfig.rebootDailyHM, ":", CFS_FETCH_MAX );

    pirebooterConfig.logUpdateSec  = (int)      cfsFetchNum( cP, "pirebooter.logUpdateSec",  (double)      60 );

    strlcpy( pirebooterConfig.rebootWarning, 
        cfsFetchStr( cP, "pirebooter.rebootWarning", "Server restart at end of game - up in 60sec" ), CFS_FETCH_MAX);
    strlcpy( pirebooterConfig.rebootDaily, 
        cfsFetchStr( cP, "pirebooter.rebootDaily", "** Server will reboot shortly - up in 60sec" ), CFS_FETCH_MAX);

    cfsDestroy( cP );
    
    pirebooterConfig.timeLastReboot = apiTimeGet();
    pirebooterConfig.timeLastProgress = apiTimeGet();
    pirebooterConfig.timeFirstIdle  = 0;

    return 0;
}


//  ==============================================================================================
//  pirebooterClientAddCB
//
//  New client constitutes 'activity' -- reset the idle timer.
//
//
int pirebooterClientAddCB( char *strIn )
{
    pirebooterConfig.timeFirstIdle  = 0;
    return 0;
}

//  ==============================================================================================
//  pirebooterClientDelCB
//
//  Departing client constitutes 'activity' -- reset the idle timer.
//
int pirebooterClientDelCB( char *strIn )
{
    pirebooterConfig.timeFirstIdle  = 0;
    return 0;
}

//  ==============================================================================================
//  pirebooterInitCB
//
//  Server init - reset the idle timer AND the time re-booted (which is NOW).
//
//
int pirebooterInitCB( char *strIn )
{
    pirebooterConfig.timeLastReboot = apiTimeGet();
    pirebooterConfig.timeLastProgress = apiTimeGet();
    pirebooterConfig.timeFirstIdle  = 0;

    return 0;
}

//  ==============================================================================================
//  pirebooterRestartCB
//
//  ...
//  ...
//
int pirebooterRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pirebooterMapChange
//
//  Map change constitutes 'activity' -- reset the idle timer.
//
//
int pirebooterMapChangeCB( char *strIn )
{
    pirebooterConfig.timeFirstIdle  = 0;
    pirebooterConfig.timeLastProgress = apiTimeGet();
    return 0;
}

//  ==============================================================================================
//  pirebooterGameStartCB
//
//  Check if reboot is pending at start of game - reboot if queued.
//
//
int pirebooterGameStartCB( char *strIn )
{
    pirebooterConfig.timeFirstIdle  = 0;
    pirebooterConfig.timeLastProgress = apiTimeGet();

    // Sometimes the server does not appear to generate the End of Game event.
    // This is a duplicate code (see pirebooterGameEndCB) as a 2nd trap
    // to make sure server gets rebooted.
    // 
    if (( pirebooterConfig.timeLastReboot + pirebooterConfig.rebootBusySec ) < apiTimeGet() ) {
        pirebooterConfig.timeLastReboot   = apiTimeGet();
        pirebooterConfig.timeFirstIdle    = 0L;
        pirebooterConfig.timeLastProgress = apiTimeGet();
        logPrintf( LOG_LEVEL_INFO, "pirebooter", "Reboot due to BUSY timer" );
        apiServerRestart();     // since the server is empty, reboot right away
    }
    return 0;
}

//  ==============================================================================================
//  pirebooterGameEndCB
//
//  Check if reboot is pending at end of game - reboot if queued.
// 
//
int pirebooterGameEndCB( char *strIn )
{
    pirebooterConfig.timeFirstIdle  = 0L;
    pirebooterConfig.timeLastProgress = apiTimeGet();

    // Check if the server has been running long, and reboot only at
    // end-of-game to be least intrusive.
    // 
    if (( pirebooterConfig.timeLastReboot + pirebooterConfig.rebootBusySec ) < apiTimeGet() ) {
        pirebooterConfig.timeLastReboot = apiTimeGet();
        pirebooterConfig.timeFirstIdle  = 0L;
        pirebooterConfig.timeLastProgress = apiTimeGet();
        logPrintf( LOG_LEVEL_INFO, "pirebooter", "Reboot due to BUSY timer" );
        apiServerRestart();     // since the server is empty, reboot right away
    }
    return 0;
}

//  ==============================================================================================
//  pirebooterRoundStartCB
//
//  The rebooter plugin does not force server restart mid-game when players are online.  
//  After reboot is queued due to expired 'busy' timer, start displaying warning messages 
//  at start of each round.  When this happens the server is rebooted after current 
//  game ends.
//
int pirebooterRoundStartCB( char *strIn )
{
    pirebooterConfig.timeFirstIdle  = 0L;
    pirebooterConfig.timeLastProgress = apiTimeGet();

    if (( pirebooterConfig.timeLastReboot + pirebooterConfig.rebootBusySec ) < apiTimeGet() )
	apiSaySys( pirebooterConfig.rebootWarning );

    return 0;
}

//  ==============================================================================================
//  pirebooterRoundEndCB
//
//  End of Round event constitutes 'activity' -- reset the idle timer.
//
int pirebooterRoundEndCB( char *strIn )
{
    pirebooterConfig.timeLastProgress = apiTimeGet();
    pirebooterConfig.timeFirstIdle  = 0L;
    return 0;
}

//  ==============================================================================================
//  pirebooterCapturedCB
//
//  The rebooter plugin does not force server restart mid-game when players are online.  
//  After reboot is queued due to expired 'busy' timer, start displaying warning messages 
//  at start of each objective.  When this happens the server is rebooted after current
//  game ends.
//  
//
int pirebooterCapturedCB( char *strIn )
{
    static unsigned long lastTime = 0L;

    pirebooterConfig.timeLastProgress = apiTimeGet();
    pirebooterConfig.timeFirstIdle  = 0L;
    
    if (( pirebooterConfig.timeLastReboot + pirebooterConfig.rebootBusySec ) < apiTimeGet() ) {
	if ( (lastTime + 10L) < apiTimeGet() ) {
	    lastTime = apiTimeGet();
	    apiSaySys( pirebooterConfig.rebootWarning );
	}
    }
    return 0;
}

//  ==============================================================================================
//  pirebooterShutdownCB
//
//  Server shut down - reset the idle timer AND the time re-booted (which is NOW).
//
int pirebooterShutdownCB( char *strIn )
{
    pirebooterConfig.timeLastReboot = apiTimeGet();
    pirebooterConfig.timeFirstIdle  = 0;
    return 0;
}

//  ==============================================================================================
//  pirebooterPeriodicCB
//
//  Periodic processing, called at 1.0 Hz rate (approximately).  This is the main algo 
//  for determining if the server requires a restart. 
//  
//
int pirebooterPeriodicCB( char *strIn )
{
    static int skipTime = 0;
    static int queueDailyReboot = 0;
    char *timeNowHuman;
    unsigned long lastValidTimeListPlayers;

    // if currently in IDLE
    //
    if ( 0 == apiPlayersGetCount()) {

	// if this is the first-idle detected, mark the time
	//
        if ( 0L == pirebooterConfig.timeFirstIdle ) {
	     pirebooterConfig.timeFirstIdle = apiTimeGet();
	}

	// if this is not the first idle, check range if reboot is required
	//
	else {
            if ( (pirebooterConfig.timeFirstIdle + pirebooterConfig.rebootIdleSec ) < apiTimeGet() ) {
                pirebooterConfig.timeLastReboot = apiTimeGet();
                pirebooterConfig.timeFirstIdle  = 0L;
                pirebooterConfig.timeLastProgress = apiTimeGet();
                logPrintf( LOG_LEVEL_INFO, "pirebooter", "Reboot due to IDLE timer" );
                apiServerRestart();     // since the server is empty, reboot right away
	    }
	}
    }

    // if currently active then invalidate the first-idle time marker
    // 
    else {
        pirebooterConfig.timeFirstIdle = 0L;
    }

    // Now check the RCON loopback interface, detect 'dead' server
    // 
    lastValidTimeListPlayers = apiGetLastRosterTime();
    if ( lastValidTimeListPlayers != 0L ) {     
        if ( (apiTimeGet() - lastValidTimeListPlayers) >  pirebooterConfig.rebootDeadSec ) {
            pirebooterConfig.timeLastReboot   = apiTimeGet();
            pirebooterConfig.timeFirstIdle    = 0L;
            pirebooterConfig.timeLastProgress = apiTimeGet();
            logPrintf( LOG_LEVEL_INFO, "pirebooter", "Reboot due to DEAD timer" );
	    apiServerRestart();
        }
    }

    // Check for "hung" server - RCON working, players 95% stuck, phantom players online
    //
    if ( (apiTimeGet() - pirebooterConfig.timeLastProgress) > pirebooterConfig.rebootHungSec ) {
        pirebooterConfig.timeLastReboot   = apiTimeGet();
        pirebooterConfig.timeFirstIdle    = 0L;
        pirebooterConfig.timeLastProgress = apiTimeGet();
        logPrintf( LOG_LEVEL_INFO, "pirebooter", "Reboot due to HUNG timer" );
        apiServerRestart();
    }

    // Log some status for debug and monitoring 
    // 
    if ( (++skipTime) > pirebooterConfig.logUpdateSec ) {
	skipTime = 0;
        if ( 0 == pirebooterConfig.timeFirstIdle ) {
            logPrintf( LOG_LEVEL_INFO, "pirebooter", "nPlayers %d; Reboot timers: idle n/a, busy %ld, dead %ld, hung %ld", 
                apiPlayersGetCount(),
                apiTimeGet() - pirebooterConfig.timeLastReboot,
                apiTimeGet() - apiGetLastRosterTime(),
                apiTimeGet() - pirebooterConfig.timeLastProgress );
	}
	else {
            logPrintf( LOG_LEVEL_INFO, "pirebooter", "nPlayers %d; Reboot timers: idle %ld, busy %ld, dead %ld, hung %ld", 
                apiPlayersGetCount(),
                apiTimeGet() - pirebooterConfig.timeFirstIdle, 
		apiTimeGet() - pirebooterConfig.timeLastReboot, 
                apiTimeGet() - apiGetLastRosterTime(),
                apiTimeGet() - pirebooterConfig.timeLastProgress );
	}
    }

    // "Abrupt" and mdandatory daily reboot - when enabled this reboots the server
    // daily at exectly specified time, without warning, mid-game.
    //
    timeNowHuman = apiTimeGetHuman();
    if ( 0 != strlen( pirebooterConfig.rebootDailyHM )) {    // if this feature is enabled...
        // check if 'triggered' for reboot
        // 
        if ( NULL != strstr( timeNowHuman, pirebooterConfig.rebootDailyHM )) {
            // wait for the next minute to actually do the reboot
            //
            queueDailyReboot++;
            if (( queueDailyReboot == 1 ) || ( queueDailyReboot == 30 )) {
                logPrintf( LOG_LEVEL_INFO, "pirebooter", "Daily Reboot is queued" );
                apiSay( pirebooterConfig.rebootDaily );
            }
        }
    }
    if ( 0 != queueDailyReboot ) {
        if ( NULL == strstr( timeNowHuman, pirebooterConfig.rebootDailyHM )) {
            queueDailyReboot = 0;
            pirebooterConfig.timeLastReboot   = apiTimeGet();         // when last rebooted
            pirebooterConfig.timeLastProgress = apiTimeGet();
            pirebooterConfig.timeFirstIdle    = 0L;
            logPrintf( LOG_LEVEL_INFO, "pirebooter", "Reboot due to DAILY timer" );
            apiServerRestart();
        }
    }

    return 0;
}


//  ==============================================================================================
//  pirebooterInstallPlugin
//
//  This method is exported and is called from the main sissm module.
//
int pirebooterInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pirebooterInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pirebooterConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pirebooterClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pirebooterClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pirebooterInitCB );
    eventsRegister( SISSM_EV_RESTART,              pirebooterRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pirebooterMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pirebooterGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pirebooterGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pirebooterRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pirebooterRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pirebooterCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pirebooterPeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             pirebooterShutdownCB );

    return 0;
}


