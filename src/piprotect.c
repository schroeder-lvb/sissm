//  ==============================================================================================
//
//  Module: PIProtect
//
//  Description:
//  Look for player causing Mesh "Boneindex" error, and optionally autokick to keep the server
//  from crashing.  This is detectable by "LogGameMode: Verbose: RestartPlayerAt..."
//  reported every 17ms for over a minute (>60 sec).  Auto-kick is triggered if the count
//  exceeds the threshold in one second sample (via the 1Hz periodic callback).
//
//  Two messages monitored are:
//  
//  [2022.05.28-02.26.24:185][ 38]LogGameMode: Verbose: RestartPlayerAtPlayerStart PlayerName
//  [2022.06.04-20.54.13:643][ 48]LogGameMode: Verbose: RestartPlayerAtTransform PlayerName
//
//  For this plugin to work, LogGameMode must be set to Verbose in Engine.ini!!
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2022.06.06
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
// #include <unistd.h>
// #include <time.h>
// #include <stdarg.h>
// #include <sys/stat.h>

// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netdb.h>
// #include <fcntl.h>

#include "winport.h"
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

#include "piprotect.h"

//  ==============================================================================================
//  Data definition 
//

#define PIPROTECT_MAXNAMESIZE (80)
#define PIPROTECT_MAXNUMNAMES (32)

#define PIPROTECTERRMATCH1 "RestartPlayerAtPlayerStart"
#define PIPROTECTERRMSG1   "[2022.05.28-02.26.24:185][ 38]LogGameMode: Verbose: RestartPlayerAtPlayerStart"

#define PIPROTECTERRMATCH2 "RestartPlayerAtTransform"
#define PIPROTECTERRMSG2   "[2022.06.04-20.54.13:643][ 48]LogGameMode: Verbose: RestartPlayerAtTransform"

static int  nameCharOffset1, nameCharOffset2;
static char errPlayerName[ PIPROTECT_MAXNUMNAMES ][PIPROTECT_MAXNAMESIZE];
static int  errPlayerCount  = 0;
static int  blackOutCounter = 0;

#define PIPROT_MAXLASTOBJTABLE  120

static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int  enableStartFail;                                // start failure detect algorithm (1 of 2)
    int  countThreshold;                            // count threshold per second to initiate kick
    int  blackoutWindowSec;                   // #secs to go inactive after player has been kicked 
    char kickReason[ CFS_FETCH_MAX ];                   // message presented to client when kicked

    int  enableBlockJoin;                                         // block high risk join (2 of 2)
    char joinLimit[PIPROT_MAXLASTOBJTABLE][CFS_FETCH_MAX];       // side/map/objective nbots adjustments

} piprotectConfig;




//  ==============================================================================================
//  piprotectInitConfig
//
//
int piprotectInitConfig( void )
{
    cfsPtr cP;
    int i;
    char varArray[256];

    cP = cfsCreate( sissmGetConfigPath() );

    // read "piprotect.pluginstate" variable from the .cfg file
    piprotectConfig.pluginState = 
        (int) cfsFetchNum( cP, "piprotect.pluginState", 0.0 );  // disabled by default

    // algorithm #1 - detected repeated false starts and kick
    //
    piprotectConfig.enableStartFail = 
        (int) cfsFetchNum( cP, "piprotect.enableStartFail",     1.0 );  // 0=disable 1=enable
    piprotectConfig.countThreshold = 
        (int) cfsFetchNum( cP, "piprotect.countThreshold",     16.0 );  // 10 pkt/sec = 40ms/msg
    piprotectConfig.blackoutWindowSec = 
        (int) cfsFetchNum( cP, "piprotect.blackoutWindowSec",  10.0 );  // 10sec blackout after kick 


    // algorithm #2 - block join on high risk objectives/counterattacks
    //
    piprotectConfig.enableBlockJoin = 
        (int) cfsFetchNum( cP, "piprotect.enableBlockJoin",     1.0 );  // 0=disable 1=enable
    for ( i=0; i<PIPROT_MAXLASTOBJTABLE; i++ ) {
        // Read the join limit table
        //
        snprintf( varArray, 256, "piprotect.joinLimit[%d]", i );
        strlcpy( piprotectConfig.joinLimit[i], cfsFetchStr( cP, varArray,  "" ), CFS_FETCH_MAX);
    }

    strlcpy( piprotectConfig.kickReason, cfsFetchStr( cP, "piprotect.kickReason", "Server Internal Error" ), CFS_FETCH_MAX);

    cfsDestroy( cP );

    return 0;
}




//  ==============================================================================================
//  piprotectClientAddCB
//
//
static int  _joinLimit( char *objName, char *mapName, char objLetter ) {
    int joinLimit = 99;    // returns default high limit if no match
    int i;
    char *w, numStr[16];

    // check if current map/side/objective is the final objective
    // if so return a non-zero value
    //
    for ( i=0; i<PIPROT_MAXLASTOBJTABLE; i++ ) {

        // if entry was not found, do not process
        // 
        if ( 0 == (int) strlen( piprotectConfig.joinLimit[i] ) ) break;

        // if the map name/side matches 
        //
        if ( 0 == strcasecmp( getWord( piprotectConfig.joinLimit[i], 0, " "), mapName )) { 

            // check if this objective is limited 
            //
            if ( NULL != ( w = getWord( piprotectConfig.joinLimit[i], 1, " ") )) {

                if ( objLetter == w[0] ) {  // case sensitive; lower = normal, upper = counterattack
                    //
                    // match found - extract the join limit
                    //
                    if ( NULL != (w = getWord ( piprotectConfig.joinLimit[i], 2, " ") )) {
                        strlcpy( numStr, w, 16 );
                        if ( 1 != sscanf( numStr, "%d", &joinLimit )) joinLimit = 99;
                    }
                }
            }
        }
    }
    return joinLimit;
}

int piprotectClientAddCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];
    char gameMode[256], objName[256], mapName[256], objLetter;
    int limitPlayers;

    rosterParsePlayerConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_CRITICAL, "piprotect", "Add Client ::%s::%s::%s::", playerName, playerGUID, playerIP );


    // get current game mode - only process if 'checkpoint' or 'hardcore' 
    // 
    strlcpy( gameMode, apiGetServerMode(), 256 );
    if ( (0 == strcasecmp( gameMode, "hardcore" ) ) || (0 == strcasecmp( gameMode, "checkpoint" )) ) {

        // get current map/objective information
        //
        strlcpy( objName, rosterGetObjective(), 256 );       // e.g., "OCCheckpoint_A"
        strlcpy( mapName, rosterGetMapName(), 255 );   // e.g., "Farmhouse_Checkpoint_Security"
        objLetter = rosterGetCoopObjectiveLetter();          // e.g., 'A', 'B', 'C', ... ' '

        objLetter = tolower( objLetter ) ; 
        if ( apiIsCounterAttack() )             // non-zero if in counterattack state
            objLetter = toupper( objLetter );

        // Check join limit criteria - kick players if join limit criteria is met
        // 
        limitPlayers = _joinLimit( objName, mapName, objLetter ) ;
        if ( apiPlayersGetCount() >= limitPlayers ) {
            if ( 1 == rosterIsValidGUID( playerGUID ) ) {        // if Steam
                apiKick( playerGUID, "Join temporarily blocked for this objective - try again in few minutes" );
                apiSay( "Unable to connect '%s' for this objective", playerName );
                logPrintf( LOG_LEVEL_WARN, "piprotect", "Protective Kick: '%s'  Map: '%s' Obj: '%c' Limit %d", playerName, mapName, objLetter, limitPlayers );
            }
            else if ( 2 == rosterIsValidGUID( playerGUID ) ) {   // if EPIC
                apiKick( playerName, "Join temporarily blocked for this objective - try again in few minutes" );
                apiSay( "Unable to connect '%s' for this objective", playerName );
                logPrintf( LOG_LEVEL_WARN, "piprotect", "Protective Kick: '%s'  Map: '%s' Obj: '%c' Limit %d", playerName, mapName, objLetter, limitPlayers );
            }
            else {
                logPrintf( LOG_LEVEL_WARN, "piprotect", "Protective kick failed: '%s'  Map: '%s' Obj: '%c' Limit %d", playerName, mapName, objLetter, limitPlayers );
            }
        }
        else {
            logPrintf( LOG_LEVEL_WARN, "piprotect", "Join permitted: '%s'  Map: '%s' Obj: '%c' Limit %d", playerName, mapName, objLetter, limitPlayers );
        }
    }
   
    return 0;
}

#if 0

//  ==============================================================================================
//  piprotectClientDelCB
//
//
int piprotectClientDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectInitCB
//
//
int piprotectInitCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectRestartCB
//
int piprotectRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectMapChangeCB
//
//
int piprotectMapChangeCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectGameStartCB
//
//
int piprotectGameStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectGameEndCB
//
//
int piprotectGameEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectRoundStartCB
//
//
int piprotectRoundStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectRoundEndCB
//
//
int piprotectRoundEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectCaptureCB
//
//
int piprotectCapturedCB( char *strIn )
{
    return 0;
}


//  ==============================================================================================
//  piprotectShutdownCB
//
//
int piprotectShutdownCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectClientSynthDelCB
//
//
int piprotectClientSynthDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectClientSynthAddCB
//
//
int piprotectClientSynthAddCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectChatCB           
//
//  This callback is invoked whenever any player enters a text chat message.                  
//
int piprotectChatCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotectSigtermCB           
//
//
int piprotectSigtermCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotect
//
//
int piprotectWinLose( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotect
//
//
int piprotectTravel( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotect
//
//
int piprotectSessionLog( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piprotect
//
//
int piprotectObjectSynth( char *strIn )
{
    return 0;
}

#endif 

//  ==============================================================================================
//  piprotect
//
//
int piprotectDetectedCB( char *strIn )
{
    if ( errPlayerCount < PIPROTECT_MAXNUMNAMES ) {
        if ( NULL != strstr( strIn, PIPROTECTERRMATCH1 )) {
            strlcpy( errPlayerName[ errPlayerCount ], &strIn[nameCharOffset1], PIPROTECT_MAXNAMESIZE );
            strClean( errPlayerName[ errPlayerCount ] );
            strTrimInPlace( errPlayerName[ errPlayerCount++ ] );
            logPrintf( LOG_LEVEL_WARN, "piprotect", "Connect DetectedCB#1 ::%s::%d::",  errPlayerName[ errPlayerCount-1 ], errPlayerCount-1 );
        }
        else if ( NULL != strstr( strIn, PIPROTECTERRMATCH2 ) ) {
            strlcpy( errPlayerName[ errPlayerCount ], &strIn[nameCharOffset2], PIPROTECT_MAXNAMESIZE );
            strClean( errPlayerName[ errPlayerCount ] );
            strTrimInPlace( errPlayerName[ errPlayerCount++ ] );
            logPrintf( LOG_LEVEL_WARN, "piprotect", "Connect DetectedCB#2 ::%s::%d::",  errPlayerName[ errPlayerCount-1 ], errPlayerCount-1 );
        }
    }
    return 0;
}


//  ==============================================================================================
//  piprotectPeriodicCB
//
//
int piprotectPeriodicCB( char *strIn )
{
    int i, j = 0;
    static char kickName[ PIPROTECT_MAXNAMESIZE ];

    // logPrintf( LOG_LEVEL_WARN, "piprotect", "PERIODIC in counterattack %d", apiIsCounterAttack() );

    // Step 0:  Init
    //
    strclr( kickName );

    // Step 1:  analyze the table 
    //
    if ( errPlayerCount > piprotectConfig.countThreshold ) {

        // get the first name from the list
        strlcpy( kickName, errPlayerName[0], PIPROTECT_MAXNAMESIZE );

        // loop through all stored names and count the matches
        for (i = 1; i<errPlayerCount; i++ ) {

            // match counter
            if (0 == strcmp( errPlayerName[i], kickName )) j++;
            // if crash is imminent, break for the next phase (kick)
            if (j > piprotectConfig.countThreshold)        break;
        }
    }

    // Step 2:  kick bad connectors, if any
    //
    if ( j > piprotectConfig.countThreshold ) {
        apiKick( kickName, piprotectConfig.kickReason );
        logPrintf( LOG_LEVEL_WARN, "piprotect", "**System internal error - protective kick by name ::%s::", kickName );
    }

    // Step 3:  reset the analysis table
    //
    errPlayerCount  = 0;

    return 0;
}


//  ==============================================================================================
//  piprotectInstallPlugin
//
//  Install callbacks 
//
int piprotectInstallPlugin( void )
{
    int i;

    // Read the plugin-specific variables from the .cfg file 
    // 
    piprotectInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( piprotectConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 

    if ( piprotectConfig.enableBlockJoin ) 
        eventsRegister( SISSM_EV_CLIENT_ADD,          piprotectClientAddCB );
    // eventsRegister( SISSM_EV_CLIENT_DEL,           piprotectClientDelCB );
    // eventsRegister( SISSM_EV_INIT,                 piprotectInitCB );
    // eventsRegister( SISSM_EV_RESTART,              piprotectRestartCB );
    // eventsRegister( SISSM_EV_MAPCHANGE,            piprotectMapChangeCB );
    // eventsRegister( SISSM_EV_GAME_START,           piprotectGameStartCB );
    // eventsRegister( SISSM_EV_GAME_END,             piprotectGameEndCB );
    // eventsRegister( SISSM_EV_ROUND_START,          piprotectRoundStartCB );
    // eventsRegister( SISSM_EV_ROUND_END,            piprotectRoundEndCB );
    // eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   piprotectCapturedCB );
    // eventsRegister( SISSM_EV_SHUTDOWN,             piprotectShutdownCB );
    // eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     piprotectClientSynthAddCB );
    // eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     piprotectClientSynthDelCB );
    // eventsRegister( SISSM_EV_CHAT,                 piprotectChatCB     );
    // eventsRegister( SISSM_EV_SIGTERM,              piprotectSigtermCB  );
    // eventsRegister( SISSM_EV_WINLOSE,              piprotectWinLose );
    // eventsRegister( SISSM_EV_TRAVEL,               piprotectTravel );
    // eventsRegister( SISSM_EV_SESSIONLOG,           piprotectSessionLog );
    // eventsRegister( SISSM_EV_OBJECT_SYNTH,         piprotectObjectSynth );
    eventsRegister( SISSM_EV_PERIODIC,                piprotectPeriodicCB );
   
    if ( piprotectConfig.enableStartFail ) 
        eventsRegister( SISSM_EV_BP_PLAYER_CONN,   piprotectDetectedCB );  // was MESHERR

    nameCharOffset1 = (int) strlen( PIPROTECTERRMSG1 ) + 1;
    nameCharOffset2 = (int) strlen( PIPROTECTERRMSG2 ) + 1;

    blackOutCounter = 0;
    errPlayerCount  = 0;

    for ( i = 0; i<PIPROTECT_MAXNUMNAMES; i++ ) 
        strclr( errPlayerName[ i ] );

    return 0;
}

//  ==============================================================================================
//  test
//
//  remove later
//
void piprotect_fakeMain( void )
{
    int i;
    char strIn[255];

    // init sim
    nameCharOffset1 = (int) strlen( PIPROTECTERRMSG1 ) + 1;
    nameCharOffset2 = (int) strlen( PIPROTECTERRMSG2 ) + 1;

    blackOutCounter = 0;
    errPlayerCount  = 0;

    for ( i = 0; i<PIPROTECT_MAXNUMNAMES; i++ )
        strclr( errPlayerName[ i ] );

    piprotectConfig.pluginState = 1;
    piprotectConfig.countThreshold = 3;
    piprotectConfig.blackoutWindowSec = 3;

    strcpy(piprotectConfig.kickReason, "test" );

    // run

    for (i = 0; i<1000; i++ ) {
        strcpy( strIn, "[2022.05.28-02.26.24:185][ 38]LogGameMode: Verbose: RestartPlayerAtPlayerStart testPlayerName" );
        piprotectDetectedCB( strIn );
        strcpy( strIn, "[2022.06.04-20.54.13:643][ 48]LogGameMode: Verbose: RestartPlayerAtTransform testPlayerName" );
        piprotectDetectedCB( strIn );
    }
    piprotectPeriodicCB( strIn );
    piprotectPeriodicCB( strIn );
    piprotectPeriodicCB( strIn );
 
    return;

}

