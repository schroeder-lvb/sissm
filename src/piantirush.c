//  ==============================================================================================
//
//  Module: PIANTIRUSH
//
//  Description:
//  Sandstorm anti-rusher algorithm for 2-stage capture rate throttling
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#define _GNU_SOURCE            // needed for strcasestr
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
#include "winport.h"   // strcasestr

#include "roster.h"
#include "api.h"
#include "p2p.h"
#include "sissm.h"

#include "piantirush.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                     // always have this in .cfg file:  0=disabled 1=enabled

    int  nPlayerExemption;                                    // turn off if less than this number
    int  nPlayerLockThreshold;                     // go into locked mode if more than this number

    char fastPrompt[CFS_FETCH_MAX];
    char fastObjectiveCaptureTime[CFS_FETCH_MAX];
    char fastObjectiveSpeedup[CFS_FETCH_MAX];

    int  slowIntervalSec;
    char slowPrompt[CFS_FETCH_MAX];
    char slowObjectiveCaptureTime[CFS_FETCH_MAX];
    char slowObjectiveSpeedup[CFS_FETCH_MAX];

    int  lockIntervalSec;
    char lockPrompt[CFS_FETCH_MAX];
    char lockObjectiveCaptureTime[CFS_FETCH_MAX];
    char lockObjectiveSpeedup[CFS_FETCH_MAX];

    char warnRusherCapturable[CFS_FETCH_MAX];
    char warnRusherDestructible[CFS_FETCH_MAX];
    int  warnRusherDelaySec;

} piantirushConfig;

static alarmObj *aPtr  = NULL;                     // Alarm for going back to normal capture rate
static alarmObj *dPtr  = NULL;                              // Alarm for displaying warning rules
static int lastState = -1;                          // last state of capture rate: { -1, 0, 1, 2 }


//  ==============================================================================================
//  _captureSpeed (internal)
//
//  Programs the server for normal (0), slow (1) or locked (2) capture rate.  This routine
//  reduces redundant programming by keeping track of previous value.  If the previous value
//  matches the new requested value, the server is not programmed.
// 
static void _captureSpeed( int isSlow )
{
    int isSlowCopy;
    isSlowCopy = isSlow;

    if ( apiPlayersGetCount() <=  piantirushConfig.nPlayerExemption )
        isSlowCopy = 0;

    if ( 0 != (int) p2pGetF( "piantirush.p2p.fast", 0.0 ) ) {
        isSlowCopy = 0;
    }

    if (lastState != isSlowCopy) {
        switch ( isSlowCopy ) {
	case 1:    // slow rate when moderate number of people are in game
            apiGameModePropertySet( "ObjectiveCaptureTime", piantirushConfig.slowObjectiveCaptureTime );
            apiGameModePropertySet( "ObjectiveSpeedup"    , piantirushConfig.slowObjectiveSpeedup );
            apiSay( piantirushConfig.slowPrompt );
            logPrintf(LOG_LEVEL_INFO, "piantirush", "**Set Capture Time to SLOW");
            lastState = 1;
            break;
        case 2:    // locked rate when lots of people are in game
            apiGameModePropertySet( "ObjectiveCaptureTime", piantirushConfig.lockObjectiveCaptureTime );
            apiGameModePropertySet( "ObjectiveSpeedup"    , piantirushConfig.lockObjectiveSpeedup );
            apiSay( piantirushConfig.lockPrompt );  
            logPrintf(LOG_LEVEL_INFO, "piantirush", "**Set Capture Time to LOCK");
            lastState = 2;
            break;
        default:    // normal case: expected value '0' 
            apiGameModePropertySet( "ObjectiveCaptureTime", piantirushConfig.fastObjectiveCaptureTime );
            apiGameModePropertySet( "ObjectiveSpeedup"    , piantirushConfig.fastObjectiveSpeedup );
            if ( lastState != 0 ) apiSay( piantirushConfig.fastPrompt );
            logPrintf(LOG_LEVEL_INFO, "piantirush", "**Set Capture Time to NORMAL");
            lastState = 0;
            break;
        }
    }
    return;
}

//  ==============================================================================================
//  _captureSpeedForceNext (internal)
//
//  Resets the state machine for capture rate change so that the next _captureSpeed() call
//  will ALWAYS be commanded to the server.  Under ordinary circumstances, _captureSpeed() call
//  filters out sending redundant (same as last) commanded configuration to the server.
//  
//  
static void _captureSpeedForceNext( void )
{
    lastState = -1;
}

//  ==============================================================================================
//  _rulesAlarmCB 
//
//  Alarm handler to display RULES for cap rushers.  
//
static int _rulesAlarmCB( char *str )
{
    char *typeObj;
    char outStr[256];

    strclr( outStr );

    // Determine if the next objective is destructible or capturable.  Send out a different
    // warning depending on the objective type.
    // 
    typeObj = rosterGetObjectiveType();
    if ( NULL != strcasestr( typeObj, "weaponcache" ) ) strlcpy( outStr, piantirushConfig.warnRusherDestructible, 256 ); 
    if ( NULL != strcasestr( typeObj, "capturable"  ) ) strlcpy( outStr, piantirushConfig.warnRusherCapturable,   256 );

    apiSay( outStr );

    return 0;
}


//  ==============================================================================================
//  _normalSpeedALarmCB 
//
//  Alarm handler to restore capture speed to "normal" when triggered.
//
static int _normalSpeedAlarmCB( char *str )
{
    _captureSpeedForceNext();
    _captureSpeed( 0 );
    _captureSpeedForceNext();
    logPrintf(LOG_LEVEL_DEBUG, "piantirush", "Antirush NORMAL alarm triggered" );
    return 0;
}


//  ==============================================================================================
//  piantirushInitConfig
//
//  Plugin variable initialization
//
int piantirushInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "piantirush.pluginstate" variable from the .cfg file
    piantirushConfig.pluginState = (int) cfsFetchNum( cP, "piantirush.pluginState", 0.0 );  // disabled by default

    piantirushConfig.nPlayerExemption = (int)           
        cfsFetchNum( cP, "piantirush.nPlayerExemption",  2 ); 
    piantirushConfig.nPlayerLockThreshold  = (int)      
        cfsFetchNum( cP, "piantirush.nPlayerLockThreshold",  6 ); 

    strlcpy( piantirushConfig.fastPrompt,               
        cfsFetchStr( cP, "piantirush.fastPrompt", "Standard capture rate unlocked" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.fastObjectiveCaptureTime, 
        cfsFetchStr( cP, "piantirush.fastObjectiveCaptureTime", "30" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.fastObjectiveSpeedup,     
        cfsFetchStr( cP, "piantirush.fastObjectiveSpeedup", "0.25" ), CFS_FETCH_MAX);

    piantirushConfig.slowIntervalSec = (int)            
        cfsFetchNum( cP, "piantirush.slowIntervalSec", 135 ); 
    strlcpy( piantirushConfig.slowPrompt,               
        cfsFetchStr( cP, "piantirush.slowPrompt", "SLOW DOWN! Objective Rushers will be banned." ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.slowObjectiveCaptureTime, 
        cfsFetchStr( cP, "piantirush.slowObjectiveCaptureTime", "90" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.slowObjectiveSpeedup,     
        cfsFetchStr( cP, "piantirush.slowObjectiveSpeedup", "0.00" ), CFS_FETCH_MAX);

    piantirushConfig.lockIntervalSec = (int)            
        cfsFetchNum( cP, "piantirush.lockIntervalSec", 210 ); 
    strlcpy( piantirushConfig.lockPrompt,               
        cfsFetchStr( cP, "piantirush.lockPrompt", "Capture locked until 11:30 on ountdown " ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.lockObjectiveCaptureTime, 
        cfsFetchStr( cP, "piantirush.lockObjectiveCaptureTime", "210" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.lockObjectiveSpeedup,     
        cfsFetchStr( cP, "piantirush.lockObjectiveSpeedup", "0.00" ), CFS_FETCH_MAX);

    strlcpy( piantirushConfig.warnRusherCapturable,     // "*Rule: Ask before entering capture zone"
        cfsFetchStr( cP, "piantirush.warnRusherCapturable", "" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.warnRusherDestructible,   // "*Rule: Ask before exploding the cache" 
        cfsFetchStr( cP, "piantirush.warnRusherDestructible", "" ), CFS_FETCH_MAX);
    piantirushConfig.warnRusherDelaySec = (int)            
        cfsFetchNum( cP, "piantirush.warnRusherDelaySec", 20 );

    cfsDestroy( cP );

    aPtr  = alarmCreate( _normalSpeedAlarmCB );
    dPtr  = alarmCreate( _rulesAlarmCB );

    return 0;
}


//  ==============================================================================================
//  _startOfEverything
//
//  This routine is called at start of game, round, and objective
//  When conditions are met, the server is programmed with SLOW or LOCKED rate of capture
//
void _startOfEverything( void )
{
    int currentPlayerCount;

    currentPlayerCount = apiPlayersGetCount();
    if (currentPlayerCount >= piantirushConfig.nPlayerLockThreshold ) {
        alarmReset( aPtr, piantirushConfig.lockIntervalSec );
        // logPrintf(LOG_LEVEL_DEBUG, "piantirush", "Antirush start LOCK alarm set %d", piantirushConfig.lockIntervalSec);
        _captureSpeed( 2 );
    }
    else  {
        alarmReset( aPtr, piantirushConfig.slowIntervalSec );
        // logPrintf(LOG_LEVEL_DEBUG, "piantirush", "Antirush start SLOW alarm set %d", piantirushConfig.lockIntervalSec);
        _captureSpeed( 1 );
    }
    return;

}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushClientAddCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushClientDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushInitCB( char *strIn )
{
    _captureSpeedForceNext();
    _captureSpeed( 0 );
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushMapChangeCB( char *strIn )
{
    _captureSpeedForceNext();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushGameStartCB( char *strIn )
{
    _captureSpeedForceNext();
    _startOfEverything();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushGameEndCB( char *strIn )
{
    _captureSpeedForceNext();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushRoundStartCB( char *strIn )
{
    _captureSpeedForceNext();
    _startOfEverything();
    alarmReset( dPtr, piantirushConfig.warnRusherDelaySec );
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushRoundEndCB( char *strIn )
{
    _captureSpeedForceNext();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushCapturedCB( char *strIn )
{
    _startOfEverything();
    return 0;
}

//  ==============================================================================================
//  piantirushPeriodicCB
//
//  1.0 Hz periodic handler 
//  Look for change in admin command that changes piantirush.p2p.fast
//
int piantirushPeriodicCB( char *strIn )
{
    static int lastFastState = -1;
    int currFastState;

    currFastState = (int) p2pGetF( "piantirush.p2p.fast", 0.0 ) ;
 
    // if last state is "slow" (default) and admin wishes to run "fast"
    // (antirush-disable), then immediately disable the slow rate via
    // the RCON
    // 
    if ( (lastFastState == 0) && (currFastState != 0) ) {
        _captureSpeedForceNext();
        _captureSpeed( 0 );
    }
    lastFastState = currFastState;

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piantirushSigKillCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "piantirush", "Restoring Normal Capture Rate before shutdown" );

    _captureSpeedForceNext();
    _captureSpeed( 0 );

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
int piantirushObjectSynth( char *strIn )
{
    alarmReset( dPtr, piantirushConfig.warnRusherDelaySec );
    return 0;
}

//  ==============================================================================================
//  piantirushInstallPlugins
//
//  This method is exported and is called from the main sissm module.
//
int piantirushInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    piantirushInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( piantirushConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           piantirushClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           piantirushClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 piantirushInitCB );
    eventsRegister( SISSM_EV_RESTART,              piantirushRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            piantirushMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           piantirushGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             piantirushGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          piantirushRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            piantirushRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   piantirushCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             piantirushPeriodicCB );
    eventsRegister( SISSM_EV_SIGTERM,              piantirushSigKillCB );
    eventsRegister( SISSM_EV_OBJECT_SYNTH,         piantirushObjectSynth );


    return 0;
}


