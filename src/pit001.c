//  ==============================================================================================
//
//  Module: PIT001
//
//  Description:
//  Functional example template for plugin developers
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

#include "pit001.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char stringParameterExample[CFS_FETCH_MAX];
    char stringParameterExample2[CFS_FETCH_MAX];
    char stringParameterExample3[CFS_FETCH_MAX];
    int  numericParameterExample;

} pit001Config;


//  ==============================================================================================
//  pit001InitConfig
//
//  Read parameters from the .cfg file 
//
int pit001InitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pit001.pluginstate" variable from the .cfg file
    pit001Config.pluginState = (int) cfsFetchNum( cP, "pit001.pluginState", 0.0 );  // disabled by default

    // read "pit001.StringParameterExample" variable from the .cfg file
    strlcpy( pit001Config.stringParameterExample,  cfsFetchStr( cP, "pit001.StringParameterExample",  "game starting!" ), CFS_FETCH_MAX);
    strlcpy( pit001Config.stringParameterExample2, cfsFetchStr( cP, "pit001.StringParameterExample2", "round starting!" ), CFS_FETCH_MAX);
    strlcpy( pit001Config.stringParameterExample3, cfsFetchStr( cP, "pit001.StringParameterExample3", "to the next objective!" ), CFS_FETCH_MAX);

    // read "pit001.NumericParameterExample" variable from the .cfg file
    pit001Config.numericParameterExample = (int) cfsFetchNum( cP, "pit001.NumericParameterExample", 3.1415926 );

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  pit001ClientAddCB
//
//  Add client event handler 
//  This only works reliably when EasyAC is on.  For slower but reliable event use
//  the Syntehtic-Add Callback.
//
int pit001ClientAddCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pit001", "Add Client ::%s::%s::%s::", playerName, playerGUID, playerIP );

    if ( apiPlayersGetCount() >= 8 ) {   // if the last connect player is #8 (last slot if 8 port server)
        if ( 0 != strcmp(playerGUID, "76561000000000000" )) {    // check if this is the server owner 
	    apiKickOrBan( 0, playerGUID, "Sorry the last port reserved for server owner only" );
            apiSay ( "pit001: Player %s kicked server full", playerName );
	}
        apiSay ( "pit001: Welcome server owner %s", playerName );
    }
    else {
        apiSay ( "pit001: Welcome %s", playerName );
    }

    return 0;
}

//  ==============================================================================================
//  pit001ClientDelCB
//
//  Del client event handler 
//  This only works reliably when EasyAC is on.  For slower but reliable event use
//  the Syntehtic-Del Callback.
//
int pit001ClientDelCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerDisConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pit001", "Del Client ::%s::%s::%s::", playerName, playerGUID, playerIP );

    apiSay ( "pit001: Good bye %s", playerName );

    return 0;
}

//  ==============================================================================================
//  pit001InitCB
//
//  This callback is invoked whenever SISSM is started or reset.
//
int pit001InitCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "Init Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pit001RestartCB
//
//  This callback is invoked just before restart/reboot is issued to the game server.
//  Expect comms blackout for 10-30 seconds after this while the game server reboots.
//
int pit001RestartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "Restart Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pit001MapChangeCB
//
//  This callback is invoked whenever a map change is detected.
//
int pit001MapChangeCB( char *strIn )
{
    char mapName[256];

    logPrintf( LOG_LEVEL_INFO, "pit001", "Map Change Event ::%s::", strIn );
    rosterParseMapname( strIn, 256, mapName );
    logPrintf( LOG_LEVEL_INFO, "pit001", "Map name ::%s::", mapName );
    return 0;
}

//  ==============================================================================================
//  pit001GameStartCB
//
//  This callback is invoked whenever a Game-Start event is detected.
//
int pit001GameStartCB( char *strIn )
{
    char newCount[256];

    logPrintf( LOG_LEVEL_INFO, "pit001", "Game Start Event ::%s::", strIn );

    apiGameModePropertySet( "minimumenemies", "4" );
    apiGameModePropertySet( "maximumenemies", "4" );

    strlcpy( newCount, apiGameModePropertyGet( "minimumenemies" ), 256) ;

    // in-game announcement start of game
    apiSay( "%s -- 2 waves of %s bots", pit001Config.stringParameterExample, newCount );  

    return 0;
}

//  ==============================================================================================
//  pit001GameEndCB
//
//  This callback is invoked whenever a Game-End event is detected.
//
int pit001GameEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "Game End Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pit001RoundStartCB
//
//  This callback is invoked whenver a Round-Start event is detected.
//
int pit001RoundStartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "Round Start Event ::%s::", strIn );
    apiSay( pit001Config.stringParameterExample2 ); // in-game announcement start of round
    return 0;
}

//  ==============================================================================================
//  pit001RoundEndCB
//
//  This callback is invoked whenever a End-of-round event is detected.
//
int pit001RoundEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "Round End Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pit001CaptureCB
//
//  This callback is invoked whenever an objective is captured.  Note multiple call of this
//  may occur for each objective.  To generate a single event, following example filters
//  all but the first event within N seconds.
//
int pit001CapturedCB( char *strIn )
{
    static unsigned long int lastTimeCaptured = 0L;
    // logPrintf( LOG_LEVEL_INFO, "pit001", "Captured Objective Event ::%s::", strIn );
    logPrintf( LOG_LEVEL_INFO, "pit001", "Captured Objective Event" );

    // system generates multiple 'captured' report, so it is necessary
    // to add a 10-second window filter to make sure only one gets fired off.
    //
    if ( lastTimeCaptured + 10L < apiTimeGet() ) {

	apiSay( pit001Config.stringParameterExample3 );
        lastTimeCaptured = apiTimeGet();
	// apiServerRestart();

    }

    return 0;
}

//  ==============================================================================================
//  pit001PeriodicCB
//
//  This callback is invoked at 1.0Hz rate (once a second).  Due to serial processing,
//  exact timing cannot be guaranteed.
//
int pit001PeriodicCB( char *strIn )
{
    logPrintf( LOG_LEVEL_DEBUG, "pit001", "Periodic Callback ::%s::", strIn );
    return 0;
}


//  ==============================================================================================
//  pit001ShutdownCB
//
//  This callback is invoked whenever a game shutdown is detected, typically as a result of 
//  restart request, or operator terminating the server.   This event is not a SISSM shutdown.
//
int pit001ShutdownCB( char *strIn )
{
    logPrintf( LOG_LEVEL_DEBUG, "pit001", "Shutdown Callback ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pit001ClientSynthDelCB
//
//  This is a slower but more reliable version of Player deletion event.  Unlike ClientDEL event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pit001ClientSynthDelCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthDisConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pit001", "Synthetic DEL Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );

    return 0;
}

//  ==============================================================================================
//  pit001ClientSynthAddCB
//
//  This is a slower but more reliable version of Player add event.  Unlike ClientADD event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pit001ClientSynthAddCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pit001", "Synthetic ADD Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );

    return 0;
}

//  ==============================================================================================
//  pit001ChatCB           
//
//  This callback is invoked whenever any player enters a text chat message.                  
//
int pit001ChatCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "Client chat ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pit001SigtermCB           
//
//  This callback is invoked when SISSM is terminated abnormally (either by OS or operator
//  pressing ^C.  Since the game server is assumed running, plugins should take necessary action
//  to leave the server in 'playable state' in absense of SISSM.
//
int pit001SigtermCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pit001", "SISSM Termination CB" );
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  This method is exported and is called from the main sissm module.
//
int pit001InstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pit001InitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pit001Config.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pit001ClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pit001ClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pit001InitCB );
    eventsRegister( SISSM_EV_RESTART,              pit001RestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pit001MapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pit001GameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pit001GameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pit001RoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pit001RoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pit001CapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pit001PeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             pit001ShutdownCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pit001ClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pit001ClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 pit001ChatCB     );
    eventsRegister( SISSM_EV_SIGTERM,              pit001SigtermCB  );

    return 0;
}



