//  ==============================================================================================
//
//  Module: PIDYNBOTS
//
//  Description:
//      Dynamic AI (bot) Adjustments - automatically re-scales bot count range, reprograms
//      difficulty level according to side/map/objective.
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.11.13
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#define _GNU_SOURCE

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

#include "p2p.h"
#include "bsd.h"
#include "log.h"
#include "events.h"
#include "cfs.h"
#include "util.h"
#include "alarm.h"
#include "winport.h"   // strcasestr

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "pidynbots.h"

#define PIDYN_MAXADJUSTERS  (64)


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int MinimumEnemies; 
    int MaximumEnemies; 
    double AIDifficulty;
    char Adjuster[PIDYN_MAXADJUSTERS][CFS_FETCH_MAX];

    int enableObjAdj;                       // 1 to enable objective adj; turn off if using webmin
    char showInGame[CFS_FETCH_MAX];                        // set to "" to disable in-game display

} pidynbotsConfig;


//  ==============================================================================================
//  pidynbotsInitConfig
//
//  Read parameters from the .cfg file 
//
int pidynbotsInitConfig( void )
{
    cfsPtr cP;
    int i;
    char varArray[256];

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pidynbots.pluginstate" variable from the .cfg file
    pidynbotsConfig.pluginState = (int) cfsFetchNum( cP, "pidynbots.pluginState", 0.0 );  // disabled by default

    pidynbotsConfig.MinimumEnemies  = (int) cfsFetchNum( cP, "pidynbots.MinimumEnemies",  3.0 );  // minimumenemies
    pidynbotsConfig.MaximumEnemies  = (int) cfsFetchNum( cP, "pidynbots.MaximumEnemies", 18.0 );  // maximumenemies
    pidynbotsConfig.AIDifficulty = (double) cfsFetchNum( cP, "pidynbots.AIDifficulty",    0.5 );  // AIDifficulty

    pidynbotsConfig.enableObjAdj = (int) cfsFetchNum( cP, "pidynbots.enableObjAdj", 0.0 );  
    strlcpy( pidynbotsConfig.showInGame, cfsFetchStr( cP, "pidynbots.showInGame", "" ), CFS_FETCH_MAX );  

    // Read the adjustment table
    //
    for ( i=0; i<PIDYN_MAXADJUSTERS; i++ ) {
        snprintf( varArray, 256, "pidynbots.Adjuster[%d]", i );
        strlcpy( pidynbotsConfig.Adjuster[i], cfsFetchStr( cP, varArray,  "" ), CFS_FETCH_MAX);
    }

    cfsDestroy( cP );
    return 0;
}

//  ==============================================================================================
//  _computeBotParams  
//
static int _computeBotParams( void )
{
    int minBots, maxBots, i;
    double aiDifficulty;
    char *objName, *mapName, objLetter;
    char strBuf[256], *w;
    int adjust = 0, errCode = 0, offsetIndex;

    // default parameters
    // 
    minBots      = pidynbotsConfig.MinimumEnemies;
    maxBots      = pidynbotsConfig.MaximumEnemies;
    aiDifficulty = pidynbotsConfig.AIDifficulty;

    // get current side, map, and objective
    //
    objName   = rosterGetObjective();                    // e.g., "OCCheckpoint_A"
    mapName   = rosterGetMapName();                      // e.g., "Farmhouse_Checkpoint_Security"
    objLetter = rosterGetCoopObjectiveLetter();          // e.g., 'A', 'B', 'C', ... ' '

    logPrintf( LOG_LEVEL_INFO, "pidynbots", "ObjName ::%s:: MapName ::%s:: objLetter ::%c::", 
        objName, mapName, objLetter );

    // search the table if override is found (look for matching map name)
    //
    for (i=0; i<PIDYN_MAXADJUSTERS; i++) {

        if ( NULL != strcasestr( pidynbotsConfig.Adjuster[i], mapName )) {

            // get min/max/dif values 
            //
            if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], 1, " " ))) 
                errCode |= ( 1 != sscanf( w, "%d", &minBots ));
            if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], 2, " " ))) 
                errCode |= ( 1 != sscanf( w, "%d", &maxBots ));
            if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], 3, " " ))) 
                errCode |= ( 1 != sscanf( w, "%lf", &aiDifficulty ));

            // if override was found, look for per-objective adjustments
            //
            if (( !errCode  ) && ( (objLetter >= 'A') && (objLetter <= 'Z') )) {
                offsetIndex = objLetter - 'A';
                offsetIndex += 4;        
                if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], offsetIndex, " " ))) {
                    errCode |= ( 1 != sscanf( w, "%d", &adjust ));
                    if ( !errCode ) maxBots += adjust;
                }
            }

            // validate to keep the server from crashing
            // 
            if ( !errCode) {
                if ( minBots > maxBots ) errCode = 1;
                if ( maxBots > 40 )      errCode = 1;
                if ( minBots < 0  )      errCode = 1;
            }           
 
            break;
        }
    }

    // poke the new values
    //
    if ( !errCode )  {

        if ( 0L == p2pGetL( "picladmin.p2p.botAdminControl", 0L )) {  // check if in-game admin took control
            logPrintf( LOG_LEVEL_INFO, "pidynbots", "Adjusting bots %d:%d difficulty %lf", 
                minBots, maxBots, aiDifficulty );

            snprintf( strBuf, 256, "%d", minBots );
                apiGameModePropertySet( "minimumenemies", strBuf );
            snprintf( strBuf, 256, "%d", maxBots );
                apiGameModePropertySet( "maximumenemies", strBuf );
            snprintf( strBuf, 256, "%lf", aiDifficulty );
                 apiGameModePropertySet( "aidifficulty",   strBuf ) ;

            // if enabled in config, update the players by showing new params in-game
            //
            if ( 0 != strlen( pidynbotsConfig.showInGame ) ) {
                apiSay("%s %d:%d Difficulty %6.3lf", pidynbotsConfig.showInGame, minBots*2, maxBots*2, aiDifficulty*10.0 );
            }

        }
        else {
            logPrintf( LOG_LEVEL_INFO, "pidynbots", "Adjusting bots bypassed due to Admin override %d:%d difficulty %lf", 
                minBots, maxBots, aiDifficulty );
        }
    }
    return 0;  
}



//  ==============================================================================================
//  Unused Functions
//
int pidynbotsClientAddCB( char *strIn ) { return 0; }
int pidynbotsClientDelCB( char *strIn ) { return 0; }
int pidynbotsCapturedCB( char *strIn )  { return 0; }
int pidynbotsPeriodicCB( char *strIn )  { return 0; }
int pidynbotsChatCB( char *strIn )      { return 0; }


//  ==============================================================================================
//  pidynbotsInitCB
//
//  This callback is invoked whenever SISSM is started or reset.
//
int pidynbotsInitCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Init Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsRestartCB
//
//  This callback is invoked just before restart/reboot is issued to the game server.
//  Expect comms blackout for 10-30 seconds after this while the game server reboots.
//
int pidynbotsRestartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Restart Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsMapChangeCB
//
//  This callback is invoked whenever a map change is detected.
//
int pidynbotsMapChangeCB( char *strIn )
{
    char mapName[256];

    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Map Change Event ::%s::", strIn );
    rosterParseMapname( strIn, 256, mapName );
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Map name ::%s::", mapName );
    return 0;
}

//  ==============================================================================================
//  pidynbotsGameStartCB
//
//  This callback is invoked whenever a Game-Start event is detected.
//
int pidynbotsGameStartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Game Start Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsGameEndCB
//
//  This callback is invoked whenever a Game-End event is detected.
//
int pidynbotsGameEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Game End Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsRoundStartCB
//
//  This callback is invoked whenver a Round-Start event is detected.
//
int pidynbotsRoundStartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Round Start Event ::%s::", strIn );

    _computeBotParams();

    return 0;
}

//  ==============================================================================================
//  pidynbotsRoundEndCB
//
//  This callback is invoked whenever a End-of-round event is detected.
//
int pidynbotsRoundEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Round End Event ::%s::", strIn );
    return 0;
}


//  ==============================================================================================
//  pidynbotsShutdownCB
//
//  This callback is invoked whenever a game shutdown is detected, typically as a result of 
//  restart request, or operator terminating the server.   This event is not a SISSM shutdown.
//
int pidynbotsShutdownCB( char *strIn )
{
    logPrintf( LOG_LEVEL_DEBUG, "pidynbots", "Shutdown Callback ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsClientSynthDelCB
//
//  This is a slower but more reliable version of Player deletion event.  Unlike ClientDEL event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pidynbotsClientSynthDelCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];
    rosterParsePlayerSynthDisConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Synthetic DEL Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );

    return 0;
}

//  ==============================================================================================
//  pidynbotsClientSynthAddCB
//
//  This is a slower but more reliable version of Player add event.  Unlike ClientADD event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pidynbotsClientSynthAddCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Synthetic ADD Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );
    // apiSay( "pidynbots: Connected %s", playerName );

    return 0;
}

//  ==============================================================================================
//  pidynbotsSigtermCB           
//
//  This callback is invoked when SISSM is terminated abnormally (either by OS or operator
//  pressing ^C.  Since the game server is assumed running, plugins should take necessary action
//  to leave the server in 'playable state' in absense of SISSM.
//
int pidynbotsSigtermCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "SISSM Termination CB" );
    return 0;
}

//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsWinLose( char *strIn )
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

    // apiSay( "pidynbots: %s", outStr );
    logPrintf( LOG_LEVEL_INFO, "pidynbots", outStr );

    return 0;
}

//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsTravel( char *strIn )
{
    char *mapName, *scenario, *mutator;
    int humanSide;

    mapName = rosterGetMapName();
    scenario = rosterGetScenario();
    mutator = rosterGetMutator();
    humanSide = rosterGetCoopSide();

    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Change map to ::%s:: Scenario ::%s:: Mutator ::%s:: Human ::%d::", mapName, scenario, mutator, humanSide );
    // apiSay( "pidynbots: Test Map-Scenario %s::%s::%s::%d::", mapName, scenario, mutator, humanSide );

    return 0;
}

//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsSessionLog( char *strIn )
{
    char *sessionID;

    sessionID = rosterGetSessionID();

    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Session ID ::%s::", sessionID );
    // apiSay( "pidynbots: Session ID %s", sessionID );

    return 0;
}

//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsObjectSynth( char *strIn )
{
    char *obj, *typ; 

    obj = rosterGetObjective();
    typ = rosterGetObjectiveType();

    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Roster Objective is ::%s::, Type is ::%s::", obj, typ ); 
    if ( pidynbotsConfig.enableObjAdj ) _computeBotParams();

    return 0;
}


//  ==============================================================================================
//  pidynbotsInstallPlugin
//
//  This method is exported and is called from the main sissm module.
//
int pidynbotsInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pidynbotsInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pidynbotsConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pidynbotsClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pidynbotsClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pidynbotsInitCB );
    eventsRegister( SISSM_EV_RESTART,              pidynbotsRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pidynbotsMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pidynbotsGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pidynbotsGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pidynbotsRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pidynbotsRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pidynbotsCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pidynbotsPeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             pidynbotsShutdownCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pidynbotsClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pidynbotsClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 pidynbotsChatCB     );
    eventsRegister( SISSM_EV_SIGTERM,              pidynbotsSigtermCB  );
    eventsRegister( SISSM_EV_WINLOSE,              pidynbotsWinLose );
    eventsRegister( SISSM_EV_TRAVEL,               pidynbotsTravel );
    eventsRegister( SISSM_EV_SESSIONLOG,           pidynbotsSessionLog );
    eventsRegister( SISSM_EV_OBJECT_SYNTH,         pidynbotsObjectSynth );

    return 0;
}

