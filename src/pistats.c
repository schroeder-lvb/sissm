//  ==============================================================================================
//
//  Module: PISTATS
//
//  Description:
//  Functional:  Generate data for Player Statistics
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2020.03.27
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
#include "p2p.h"

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "pistats.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char stringParameterExample[CFS_FETCH_MAX];
    char stringParameterExample2[CFS_FETCH_MAX];
    char stringParameterExample3[CFS_FETCH_MAX];
    int  numericParameterExample;

} pistatsConfig;


//  ==============================================================================================
//  pistatsInitConfig
//
//  Read parameters from the .cfg file 
//
int pistatsInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pistats.pluginstate" variable from the .cfg file
    pistatsConfig.pluginState = (int) cfsFetchNum( cP, "pistats.pluginState", 0.0 );  // disabled by default

    // read "pistats.StringParameterExample" variable from the .cfg file
    strlcpy( pistatsConfig.stringParameterExample,  cfsFetchStr( cP, "pistats.StringParameterExample",  "game starting!" ), CFS_FETCH_MAX);
    strlcpy( pistatsConfig.stringParameterExample2, cfsFetchStr( cP, "pistats.StringParameterExample2", "round starting!" ), CFS_FETCH_MAX);
    strlcpy( pistatsConfig.stringParameterExample3, cfsFetchStr( cP, "pistats.StringParameterExample3", "to the next objective!" ), CFS_FETCH_MAX);

    // read "pistats.NumericParameterExample" variable from the .cfg file
    pistatsConfig.numericParameterExample = (int) cfsFetchNum( cP, "pistats.NumericParameterExample", 3.1415926 );

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  pistatsClientAddCB
//
//  Add client event handler 
//  This only works reliably when EasyAC is on.  For slower but reliable event use
//  the Syntehtic-Add Callback.
//
int pistatsClientAddCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pistats", "Add Client ::%s::%s::%s::", playerName, playerGUID, playerIP );

    if ( apiPlayersGetCount() >= 8 ) {   // if the last connect player is #8 (last slot if 8 port server)
        if ( 0 != strcmp(playerGUID, "76561000000000000" )) {    // check if this is the server owner 
	    apiKickOrBan( 0, playerGUID, "Sorry the last port reserved for server owner only" );
            apiSay ( "pistats: Player %s kicked server full", playerName );
	}
        apiSay ( "pistats: Welcome server owner %s", playerName );
    }
    else {
        apiSay ( "pistats: Welcome %s", playerName );
    }

    return 0;
}

//  ==============================================================================================
//  pistatsClientDelCB
//
//  Del client event handler 
//  This only works reliably when EasyAC is on.  For slower but reliable event use
//  the Syntehtic-Del Callback.
//
int pistatsClientDelCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerDisConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pistats", "Del Client ::%s::%s::%s::", playerName, playerGUID, playerIP );

    apiSay ( "pistats: Good bye %s", playerName );

    return 0;
}

//  ==============================================================================================
//  pistatsInitCB
//
//  This callback is invoked whenever SISSM is started or reset.
//
int pistatsInitCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "Init Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pistatsRestartCB
//
//  This callback is invoked just before restart/reboot is issued to the game server.
//  Expect comms blackout for 10-30 seconds after this while the game server reboots.
//
int pistatsRestartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "Restart Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pistatsMapChangeCB
//
//  This callback is invoked whenever a map change is detected.
//
int pistatsMapChangeCB( char *strIn )
{
    char mapName[256];

    logPrintf( LOG_LEVEL_INFO, "pistats", "Map Change Event ::%s::", strIn );
    rosterParseMapname( strIn, 256, mapName );
    logPrintf( LOG_LEVEL_INFO, "pistats", "Map name ::%s::", mapName );
    return 0;
}

//  ==============================================================================================
//  pistatsGameStartCB
//
//  This callback is invoked whenever a Game-Start event is detected.
//
int pistatsGameStartCB( char *strIn )
{
    char newCount[256];

    logPrintf( LOG_LEVEL_INFO, "pistats", "Game Start Event ::%s::", strIn );

    apiGameModePropertySet( "minimumenemies", "4" );
    apiGameModePropertySet( "maximumenemies", "4" );

    strlcpy( newCount, apiGameModePropertyGet( "minimumenemies" ), 256) ;

    // in-game announcement start of game
    apiSay( "pistats: %s -- 2 waves of %s bots", pistatsConfig.stringParameterExample, newCount );  

    return 0;
}

//  ==============================================================================================
//  pistatsGameEndCB
//
//  This callback is invoked whenever a Game-End event is detected.
//
int pistatsGameEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "Game End Event ::%s::", strIn );
    apiSay( "pistats: End of game" );
    return 0;
}

//  ==============================================================================================
//  pistatsRoundStartCB
//
//  This callback is invoked whenver a Round-Start event is detected.
//
int pistatsRoundStartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "Round Start Event ::%s::", strIn );
    apiSay( pistatsConfig.stringParameterExample2 ); // in-game announcement start of round
    return 0;
}

//  ==============================================================================================
//  pistatsRoundEndCB
//
//  This callback is invoked whenever a End-of-round event is detected.
//
int pistatsRoundEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "Round End Event ::%s::", strIn );
    apiSay( "pistats: End of Round" );
    return 0;
}

//  ==============================================================================================
//  pistatsCaptureCB
//
//  This callback is invoked whenever an objective is captured.  Note multiple call of this
//  may occur for each objective.  To generate a single event, following example filters
//  all but the first event within N seconds.
//
int pistatsCapturedCB( char *strIn )
{
    static unsigned long int lastTimeCaptured = 0L;
    // logPrintf( LOG_LEVEL_INFO, "pistats", "Captured Objective Event ::%s::", strIn );
    logPrintf( LOG_LEVEL_INFO, "pistats", "Captured Objective Event" );

    // system generates multiple 'captured' report, so it is necessary
    // to add a 10-second window filter to make sure only one gets fired off.
    //
    if ( lastTimeCaptured + 10L < apiTimeGet() ) {

	apiSay( pistatsConfig.stringParameterExample3 );
        lastTimeCaptured = apiTimeGet();
	// apiServerRestart();

    }

    return 0;
}

//  ==============================================================================================
//  pistatsPeriodicCB
//
//  This callback is invoked at 1.0Hz rate (once a second).  Due to serial processing,
//  exact timing cannot be guaranteed.
//
int pistatsPeriodicCB( char *strIn )
{
    logPrintf( LOG_LEVEL_DEBUG, "pistats", "Periodic Callback ::%s::", strIn );
    return 0;
}


//  ==============================================================================================
//  pistatsShutdownCB
//
//  This callback is invoked whenever a game shutdown is detected, typically as a result of 
//  restart request, or operator terminating the server.   This event is not a SISSM shutdown.
//
int pistatsShutdownCB( char *strIn )
{
    logPrintf( LOG_LEVEL_DEBUG, "pistats", "Shutdown Callback ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pistatsClientSynthDelCB
//
//  This is a slower but more reliable version of Player deletion event.  Unlike ClientDEL event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pistatsClientSynthDelCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthDisConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pistats", "Synthetic DEL Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );
    apiSay( "pistats: Disconnect %s", playerName );

    return 0;
}

//  ==============================================================================================
//  pistatsClientSynthAddCB
//
//  This is a slower but more reliable version of Player add event.  Unlike ClientADD event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pistatsClientSynthAddCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pistats", "Synthetic ADD Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );
    apiSay( "pistats: Connected %s", playerName );

    return 0;
}

//  ==============================================================================================
//  pistatsChatCB           
//
//  This callback is invoked whenever any player enters a text chat message.                  
//
int pistatsChatCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "Client chat ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pistatsSigtermCB           
//
//  This callback is invoked when SISSM is terminated abnormally (either by OS or operator
//  pressing ^C.  Since the game server is assumed running, plugins should take necessary action
//  to leave the server in 'playable state' in absense of SISSM.
//
int pistatsSigtermCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pistats", "SISSM Termination CB" );
    return 0;
}

//  ==============================================================================================
//  pistats
//
//
int pistatsWinLose( char *strIn )
{
    int isTeam0, humanSide;
    char outStr[256];

    humanSide = rosterGetCoopSide();
    isTeam0   = (NULL != strstr( strIn, "Team 0" ));

    switch ( humanSide ) {
    case 0:
        if ( !isTeam0 ) strlcpy( outStr, "Co-op Humans Win", 256 );
        break;
    case 1:
        if (  isTeam0 ) strlcpy( outStr, "Co-opHumans Lose", 256 );
        break;
    default:
        strlcpy( outStr, "PvP WinLose", 256 );
        break;
    }

    apiSay( "pistats: %s", outStr );
    logPrintf( LOG_LEVEL_INFO, "pistats", outStr );

    return 0;
}

//  ==============================================================================================
//  pistats
//
//
int pistatsTravel( char *strIn )
{
    char *mapName, *scenario, *mutator;
    int humanSide;

    mapName = rosterGetMapName();
    scenario = rosterGetScenario();
    mutator = rosterGetMutator();
    humanSide = rosterGetCoopSide();

    logPrintf( LOG_LEVEL_INFO, "pistats", "Change map to ::%s:: Scenario ::%s:: Mutator ::%s:: Human ::%d::", 
        mapName, scenario, mutator, humanSide );
    apiSay( "pistats: Test Map-Scenario %s::%s::%s::%d::", mapName, scenario, mutator, humanSide );

    return 0;
}

//  ==============================================================================================
//  pistats
//
//
int pistatsSessionLog( char *strIn )
{
    char *sessionID;

    sessionID = rosterGetSessionID();

    logPrintf( LOG_LEVEL_INFO, "pistats", "Session ID ::%s::", sessionID );
    apiSay( "pistats: Session ID %s", sessionID );

    return 0;
}

//  ==============================================================================================
//  pistats
//
//
int pistatsObjectSynth( char *strIn )
{
    char *obj, *typ; 

    obj = rosterGetObjective();
    typ = rosterGetObjectiveType();

    logPrintf( LOG_LEVEL_INFO, "pistats", "Roster Objective is ::%s::, Type is ::%s::", obj, typ ); 
    apiSay( "pistats: Roster Objective is ::%s::, Type is ::%s::", obj, typ ); 

    return 0;
}


//  ==============================================================================================
//  ...
//
//  ...
//  This method is exported and is called from the main sissm module.
//
int pistatsInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pistatsInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pistatsConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pistatsClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pistatsClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pistatsInitCB );
    eventsRegister( SISSM_EV_RESTART,              pistatsRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pistatsMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pistatsGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pistatsGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pistatsRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pistatsRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pistatsCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pistatsPeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             pistatsShutdownCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pistatsClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pistatsClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 pistatsChatCB     );
    eventsRegister( SISSM_EV_SIGTERM,              pistatsSigtermCB  );
    eventsRegister( SISSM_EV_WINLOSE,              pistatsWinLose );
    eventsRegister( SISSM_EV_TRAVEL,               pistatsTravel );
    eventsRegister( SISSM_EV_SESSIONLOG,           pistatsSessionLog );
    eventsRegister( SISSM_EV_OBJECT_SYNTH,         pistatsObjectSynth );

    return 0;
}


