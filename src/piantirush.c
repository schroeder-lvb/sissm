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
#include <time.h>
#include <stdarg.h>
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

#include "rdrv.h"

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

    int  autoKickEarlyDestruction;       // 1 = autokick players that explodes weapons cache early
    int  destroySpeedup;                                 // speed up cache blow timer if requested
    char destroyOkPrompt[CFS_FETCH_MAX];      // prompt authorizing players to proceed w/ destruct
    char destroyDenyPrompt[CFS_FETCH_MAX];                  // prompt denying players for destruct
    char destroyHoldPrompt[CFS_FETCH_MAX];         // prompt ordering players not to destroy cache
    char earlyDestroyKickMessage[CFS_FETCH_MAX];     // the 'reason' message to show kicked player
    char earlyDestroyPlayerRequest[CFS_FETCH_MAX];    // list of request cmd for  early cache blow
    int  earlyDestroyTolerance;           // seconds of tolerance player gets kicked before expiry

    int  earlyBreachShow;              // 1 = show who entered/existed from the capture zone early

    int  autoKickEarlyBreach;     // 1 = autokick playrs that prematurely enters or taps the point
    char breachOkPrompt[CFS_FETCH_MAX];                    // prompt authorizing players to breach
    char breachDenyPrompt[CFS_FETCH_MAX];                     // prompt denying players for breach
    int  earlyBreachSpeedup;                                   // speed up zone entry if requested
    int  earlyBreachMaxTime;                       // max# of "time" allowed (seconds) before kick
    int  earlyBreachMaxTaps;                         // max# of "taps" allowed (count) before kick
    char earlyBreachPlayerRequest[CFS_FETCH_MAX];     // list of request cmd for  early zone entry
    char earlyBreachTapsKickMessage[CFS_FETCH_MAX];          // message displayed on kicked player
    char earlyBreachTimeKickMessage[CFS_FETCH_MAX];          // message displayed on kicked player
    char earlyBreachWarn[CFS_FETCH_MAX];                        // warn premature breach on screen
    char earlyBreachExit[CFS_FETCH_MAX];              // info cap rusher has existed the objective

    int  autoKickExemptLMS;            // max # of surviving players to exempt auto-kick algorithm
    char autoKickExemptMsg[CFS_FETCH_MAX];                              // exempt message when LMS 

    // These are for "strict" training modes.  When enabled ("!wax on") by admin, it extends
    // auto-kick/slow-capture timer limit so that players MUST type 'aa', '11', etc. to capture
    // an objective, irigardless of the clock.  Also note that enabling Training mode does NOT 
    // allow for early capture -- players will get 'deny' message if timer is still blocking 
    // the objective.
    // 
    int  lockIntervalSecTrainOffset,    trTimeOfs;
    int  slowIntervalSecTrainOffset,    trSlowOfs;
    int  earlyBreachSpeedupTrainOffset, trEarlyOfs;
    int  destroySpeedupTrainOffset,     trEarDesOfs;
    int  nPlayerExemptionTrainOffset,   trNPlayerOfs;      

    char trainingModeMsg[CFS_FETCH_MAX];                                  // training mode message
    // int  trainingOnReminderTime;     // time in elapsed sec to display training mode reminder text
    int  trainingOnDefaultEnable;       // from sissm.cfg option to make training mode default 'on'

} piantirushConfig;

static int _localMute = 0;
static int _fastMode  = 0;

static alarmObj *aPtr  = NULL;                      // Alarm for going back to normal capture rate
static alarmObj *dPtr  = NULL;                               // Alarm for displaying warning rules
static alarmObj *wPtr  = NULL;                               // Alarm for weapons destroy hold msg
static alarmObj *tPtr  = NULL;                              // Alarm for territorial early cap msg
static alarmObj *rPtr  = NULL;             // Delay prompt for player request for early cache blow
static alarmObj *bPtr  = NULL;                 // Delay prompt for player request for early breach
static alarmObj *xPtr  = NULL;           // training mode ("!wax on") reminder prompt when enabled
static int lastState = -1;                          // last state of capture rate: { -1, 0, 1, 2 }


// Two kickeArmed variable serve as a safety interlock so people don't get kicked
// on following conditions:
//
// kickArmed is the normal enabler that is 'armed' when warning is printed on screen
// and is 'disarmed' when a cap rate timer expires.  kickArmed2 handles the race condition
// of looking at number of alive players.  If under any condition # alive players is below
// set threshold, autokick is 'disarmed'.  Both kidkArmed and kickArmed2 must be True for
// playrers to get kicked.
// 
static int kickArmed = 0;                   // armed on warning printed, disarmed on timer expiry
static int kickArmed2 = 2;    // armed or disarmed by periodic poller according to #alive players


static char _lastObjLetter = '~';

static int  _peakAlivePlayer = -1;                // peak # of alive players on this objective leg

// Data structure to keep track of 'premature breach' cap rushers that enters territorial zone
// early (time interval count), or repeatedly "taps" the point as nuisance (count based).
//
static struct {
    char playerCharID[64];                                      // internal character ID "soldier"
    char playerName[64];                                                      // player given name
    int playerEarlyBreachCount;                 // number of times a player has "tapped" the point
    int playerEarlyBreachCumulativeTime;       // number of seconds players has occupied the point 
    int playerIsInZone;                       // indicator if players is currently inside the zone 
} territorialRushers[ SISSM_MAXPLAYERS ];


//  ==============================================================================================
//  _localSay
//
//  Wedge for apiSay - with local mute capability.
//
static int _localSay( const char * format, ... )
{
    static char buffer[API_T_BUFSIZE];
    int errCode = 0;
    char *v;

    // a wedge code to disable this plugin if non-Checkpoint mode is running
    //
    v = apiGetServerMode();
    if (! ((0==strcmp( v, "checkpoint" )) || (0==strcmp( v, "hardcore"))) )  return 0;

    if ( 0 == _localMute ) {
        va_list args;
        va_start( args, format );
        vsnprintf( buffer, (API_T_BUFSIZE >= API_MAXSAY) ? API_MAXSAY : API_T_BUFSIZE, format, args );
        errCode = apiSay( buffer );
        va_end (args);
    }
    return errCode;
}

//  ==============================================================================================
//  _localKickOrBan
//
//  Wedge for apiKicOrBan - with local disable capability.
//  This function returns non-zero if player is NOT KICKED due to exemption 
//
static int _localKickOrBan( int isBan, char *optionalPlayerName, char *playerGUID, char *reason )
{
    int errCode = 0, notKicked = 0;
    char *v;
    long autoKickExemptSecsL;

    // nonzero value here means protection window is set for non-kick. The
    // value is countdown timer in seconds, stops at 0L.
    //
    autoKickExemptSecsL = p2pGetL( "piantirush.p2p.nokickprotect", 0L );

    // a wedge code to disable this plugin if non-Checkpoint mode is running
    //
    v = apiGetServerMode();
    if (! ((0==strcmp( v, "checkpoint" )) || (0==strcmp( v, "hardcore"))) )  return 0;

    if ( 0L == autoKickExemptSecsL ) {

        // this will kick a player "for reals"
        //
        if (0 != strlen( playerGUID )) 
            errCode |= apiKickOrBan( isBan, playerGUID, reason );

        // redundancy
        //
        if ( 0 != strlen( optionalPlayerName )) 
            errCode |= apiKick( optionalPlayerName, reason );

        logPrintf( LOG_LEVEL_DEBUG, "piantirush", "Rush-AutoKick ::%s::%s::", playerGUID, optionalPlayerName );
    }
    else {
        notKicked = 1;
        logPrintf( LOG_LEVEL_INFO, "piantirush", "Rush-AutoKick Exempted by Admin ::%s::%s::", playerGUID, optionalPlayerName );
    }

    return( notKicked );
}

//  ==============================================================================================
//  _fastChecksum
//
//  Compute rudimentary 32-bit checksum of a string; for speed string length is limited to 255.
//
static unsigned long _fastChecksum( char *strIn ) 
{
    int i;
    register unsigned int checksum = 0L;
    register unsigned char *p;
    int len = (int) strlen( strIn );

    p = (unsigned char *) strIn;
    if ( len > 255 ) len = 255;
    for ( i = 0; i<len; i++ ) checksum += (unsigned int) *p++;
    return ((unsigned long) checksum); 
}


//  ==============================================================================================
//  _localSayThrottled
//
//  same as _localSay except it filters out duplicate message within N seconds
//

#define THROTTLE_HISTO  (128) 
static unsigned long throttle[THROTTLE_HISTO][2];
static int  throttleIndex = -1;

static int _localSayThrottled( const char * format, ... )
{
    static char buffer[API_T_BUFSIZE];
    int errCode = 0, i, suppressPrint;
    unsigned long checksum, timeNow;
    char *v;

    // a wedge code to disable this plugin if non-Checkpoint mode is running 
    //
    v = apiGetServerMode();
    if (! ((0==strcmp( v, "checkpoint" )) || (0==strcmp( v, "hardcore"))) )  return 0;

    if ( throttleIndex == -1 )  {
         memset( &throttle[0][0], 0, sizeof(long)*THROTTLE_HISTO*2 );
         throttleIndex = 0;
    }

    if ( 0 == _localMute ) {
        va_list args;
        va_start( args, format );
        vsnprintf( buffer, (API_T_BUFSIZE >= API_MAXSAY) ? API_MAXSAY : API_T_BUFSIZE, format, args );

        checksum = _fastChecksum( buffer );
        timeNow  = (unsigned long ) time(NULL);

        suppressPrint = 0;
        for ( i = 0; i<THROTTLE_HISTO; i++ ) {
            if ( (timeNow ) == throttle[i][0] ) {
                if ( checksum == throttle[i][1] ) {
                    suppressPrint = 1;
                    break;
                }
            }
        }

        if ( 0 == suppressPrint ) errCode = apiSay( buffer );

        throttle[ throttleIndex ][0] = timeNow;
        throttle[ throttleIndex ][1] = checksum;
        if ( (++throttleIndex) >= THROTTLE_HISTO ) throttleIndex = 0;

        va_end (args);
    }
    return errCode;
}


//  ==============================================================================================
//  _capRusherClear (internal)
//
//  Clears the data structure used to keep track of territorial cap rushers
//  This is to be called at 'start of round' (SOR), and 'start of objective' (SOO)
//  
static void _capRusherClear( void )
{
    int i;
    for (i = 0; i<SISSM_MAXPLAYERS; i++) {
        strclr( territorialRushers[i].playerCharID );        
        strclr( territorialRushers[i].playerName );
        territorialRushers[i].playerEarlyBreachCount = 0;
        territorialRushers[i].playerEarlyBreachCumulativeTime = 0;
        territorialRushers[i].playerIsInZone = 0;
    }

    kickArmed = 0;

    return;
}

//  ==============================================================================================
//  _capRusherEnteredZone (internal)
//
//  Create/update information someone has 'tapped' the point
// 
//  This is to be called from a event handler callback routine ObjTouchCB which has knowledge
//  of a player entering the 'zone' early.
//  
static void _capRusherEnterZone( char *playerCharID, char *playerName )
{
    int i;
    int foundIndex = -1; 
    int foundEmpty = -1;

    for (i = 0; i<SISSM_MAXPLAYERS; i++) {
        if ( 0 == strcmp( playerCharID, territorialRushers[i].playerCharID )) {
             foundIndex = i;
             break;
        }
        else if (( foundEmpty == -1 ) && ( 0 == territorialRushers[i].playerCharID[0] )) {
            foundEmpty = i;
        }
    }

    if ( foundIndex == -1 ) 
        foundIndex = foundEmpty;

    if ( foundIndex != -1 ) {
        i = foundIndex;
        strlcpy( territorialRushers[i].playerCharID, playerCharID, 64 );        
        strlcpy( territorialRushers[i].playerName, playerName, 64 );
        territorialRushers[i].playerIsInZone = 1;

        // territorialRushers[i].playerEarlyBreachCumulativeTime - don't update!
        // territorialRushers[i].playerIsInZone  - don't update!

        logPrintf( LOG_LEVEL_DEBUG, "piantirush", "'%s' [%s] early breach detected.", playerName, playerCharID ); 
    }
    else {
        logPrintf(LOG_LEVEL_CRITICAL, "piantirush", "**Internal error - territorialRusher table full" );
    }

    return;
}


//  ==============================================================================================
//  _capRusherExitZone (internal)
//
//  Update cap rusher structure when a player steps off the capture point ObjTouchCB.
//  Note this does NOT clear any cumulative value, only updates the playerIsInZone
//  status.
//  
static void _capRusherExitZone( char *playerCharID )
{
    int i;
    int foundIndex = -1;

    for (i = 0; i<SISSM_MAXPLAYERS; i++) {
        if ( 0 == strcmp( playerCharID, territorialRushers[i].playerCharID )) {
             foundIndex = i;
             break;
        }
    }
    if ( foundIndex != -1 ) {
        i = foundIndex;
        territorialRushers[i].playerIsInZone = 0;
        logPrintf( LOG_LEVEL_DEBUG, "piantirush", "[%s] early breach area exited.", playerCharID );  
    }
    else {
        logPrintf(LOG_LEVEL_CRITICAL, "piantirush", "**Internal Error -trying to remove nonexistent player from territorialRusher" );
    }

    return;
}

//  ==============================================================================================
//  _capRusherPeriodicCheck (internal)
//
//  This routine checks the status of the territorialRushers[] table to determine if any
//  player needs to be kicked from the server.  It is to be called periodically (1.0 Hz) 
//  by the system periodic event handler.
//  
//
static void _capRusherPeriodicCheck( void )
{
    int i;
    char *g, playerGUID[256];   // EPIC

    if ( kickArmed2 && kickArmed && piantirushConfig.autoKickEarlyBreach)  {    // check for master 'armed' status (safety interlock)
        
        for (i = 0; i<SISSM_MAXPLAYERS; i++) {
            if ( 0 != territorialRushers[i].playerCharID[0] ) {

                // check for player exceeding time occupying the zone prematurely
                // 
                if ( territorialRushers[i].playerIsInZone ) {
                    if ( territorialRushers[i].playerEarlyBreachCumulativeTime++ > piantirushConfig.earlyBreachMaxTime ) {
                        if ( NULL != ( g = rosterLookupSteamIDFromName( territorialRushers[i].playerName )) ) {
                            strlcpy( playerGUID, g, 256 );  // EPIC
                            if ( 0 == _localKickOrBan( 0, territorialRushers[i].playerName, playerGUID, piantirushConfig.earlyBreachTimeKickMessage ) ) {
                                _localSay( "'%s' auto-kicked for early breach",  territorialRushers[i].playerName );
                                logPrintf(LOG_LEVEL_WARN, "piantirush", "%s [%s] auto-kicked for early breach", territorialRushers[i].playerName, playerGUID );
                            }

                            // remove the player from cached table in case another player connects and the
                            // system re-uses the same character ID
                            //
                            territorialRushers[i].playerCharID[0] = 0;
                            territorialRushers[i].playerName[0] = 0;
                            territorialRushers[i].playerIsInZone = 0;
                            territorialRushers[i].playerEarlyBreachCount = 0;
                            territorialRushers[i].playerEarlyBreachCumulativeTime = 0;
                        }
                        break;
                    }
                }


                // check for player exceeding number of nuisance "taps" 
                //
                if (( 0 != piantirushConfig.earlyBreachMaxTaps ) &&  
                    ( territorialRushers[i].playerEarlyBreachCount++ > piantirushConfig.earlyBreachMaxTaps ) ) {  // 20211006tap
                    if ( NULL != ( g = rosterLookupSteamIDFromName( territorialRushers[i].playerName )) ) {
                        strlcpy( playerGUID, g, 256 );  // EPIC
                        if ( 0 == _localKickOrBan( 0, territorialRushers[i].playerName, playerGUID, piantirushConfig.earlyBreachTapsKickMessage ) ) {
                            _localSay( "'%s' auto-kicked for nuisance objective tapping",  territorialRushers[i].playerName );
                            logPrintf(LOG_LEVEL_WARN, "piantirush", "%s [%s] auto-kicked for excess objective tapping", territorialRushers[i].playerName, playerGUID );
                        }
                        // remove the player from cached table in case another player connects and the
                        // system re-uses the same character ID
                        //
                        territorialRushers[i].playerCharID[0] = 0;
                        territorialRushers[i].playerName[0] = 0;
                        territorialRushers[i].playerIsInZone = 0;
                        territorialRushers[i].playerEarlyBreachCount = 0;
                        territorialRushers[i].playerEarlyBreachCumulativeTime = 0;
                    }
                    break;
                }
            }
        }
    }
}


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
    char *v;

    // a wedge code to disable this plugin if non-Checkpoint mode is running
    //
    v = apiGetServerMode();
    // if (! ((0==strcmp( v, "checkpoint" )) || (0==strcmp( v, "hardcore"))) )  return;

    if (! ((0==strcmp( v, "checkpoint" )) || (0==strcmp( v, "hardcore"))) ) 
        _fastMode = 1;
 
    if ( apiPlayersGetCount() <=  ( piantirushConfig.nPlayerExemption + piantirushConfig.trNPlayerOfs ) )
        isSlowCopy = 0;

    // if we're in 'fast on' mode then always force capture speed to (0 = normal).
    // The static in _fastMode is updated from the periodic handler polling for p2p
    // variable change as set by an admin through picladmin plugin.
    // 
    if ( _fastMode ) {
        isSlowCopy = 0;
    }

    if ( isSlowCopy != 0 ) {
        alarmReset( wPtr, 10 );   // delayed prompt for cache destruction HOLD order
    }

    if (lastState != isSlowCopy) {
        switch ( isSlowCopy ) {
	case 1:    // slow rate when moderate number of people are in game
            apiGameModePropertySet( "ObjectiveCaptureTime", piantirushConfig.slowObjectiveCaptureTime );
            apiGameModePropertySet( "ObjectiveSpeedup"    , piantirushConfig.slowObjectiveSpeedup );
            _localSay( piantirushConfig.slowPrompt );

            // alarmReset( wPtr, 10 );  // delayed prompt for cache destruction HOLD order

            logPrintf(LOG_LEVEL_INFO, "piantirush", "**Set Capture Time to SLOW");
            lastState = 1;
            break;
        case 2:    // locked rate when lots of people are in game
            apiGameModePropertySet( "ObjectiveCaptureTime", piantirushConfig.lockObjectiveCaptureTime );
            apiGameModePropertySet( "ObjectiveSpeedup"    , piantirushConfig.lockObjectiveSpeedup );
            _localSay( piantirushConfig.lockPrompt );  

            // alarmReset( wPtr, 10 );   // delayed prompt for cache destruction HOLD order

            logPrintf(LOG_LEVEL_INFO, "piantirush", "**Set Capture Time to LOCK");
            lastState = 2;
            break;
        default:    // normal case: expected value '0' 
            apiGameModePropertySet( "ObjectiveCaptureTime", piantirushConfig.fastObjectiveCaptureTime );
            apiGameModePropertySet( "ObjectiveSpeedup"    , piantirushConfig.fastObjectiveSpeedup );

            kickArmed = 0;           // kick safety : do not kick

            if ( lastState != 0 ) { 
                if ( NULL != strstr( rosterGetObjectiveType(), "Capturable" )) {

                    // check if capture objective letter 'A', 'B', 'C'... has changed.  Only print
                    // fast prompt change if changed.  Letter does NOT change during the counterattack
                    // so this block of code keep extra prompt flashing on screen during C/A.
                    // The '_lastObjLetter' variable is cleared at round start.
                    //
                    if ( _lastObjLetter != rosterGetCoopObjectiveLetter() )  {
                        _localSay( piantirushConfig.fastPrompt );
                        _lastObjLetter = rosterGetCoopObjectiveLetter();
                    }
                }
                else if ( NULL != strstr( rosterGetObjectiveType(), "WeaponCache" )) {
                    if (piantirushConfig.autoKickEarlyDestruction) {
                        _localSay( piantirushConfig.destroyOkPrompt );
                    }
                }
            }
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
//  _notifyNoDestroyCB
//
//  Alarm handler to display destroy hold on weapons cache
//  Delayed transaction is necessary due to system process delay of determining if the 
//  'next' objective is a cache or territory.  It also helps to declutter what is printed
//  on game screen.
//
static int _notifyNoDestroyCB( char *str ) 
{
    if (piantirushConfig.autoKickEarlyDestruction) {
        if ( NULL != strstr( rosterGetObjectiveType(), "WeaponCache" ))  {
            _localSay( piantirushConfig.destroyHoldPrompt );
        }
    }
    return 0;
}
  

//  ==============================================================================================
//  _earlyDestroyReqCB
//
//  When a player requests (aa, req, 00, 11, etc.) to blow the cache, introduce a 5 second
//  delay to 'call HQ for permission' then print if the request is granted or denied.  The
//  delay is needed for cosmetic and anti-confusion reasons as this module needs to work with
//  existing pitacnomic module that handles aa, 00, 11, etc. prompting with the player.
//
static int _earlyDestroyReqCB( char *str )
{
    if ( alarmStatus( aPtr ) < (long) (piantirushConfig.destroySpeedup + piantirushConfig.trEarDesOfs) ) {
        _localSay( piantirushConfig.destroyOkPrompt );
        kickArmed = 0;                    // kick safety : disable kick
        alarmCancel( aPtr );
        _captureSpeed( 0 );  
    }
    else {
        _localSay( piantirushConfig.destroyDenyPrompt );
    }
    return 0;
}

//  ==============================================================================================
//  _earlyBreachReqCB
//
//  When a player requests (aa, req, 00, 11, etc.) to breach a territorial objective early
//  introduce a 5 delay to 'call HQ for permission' then print if the request is granted or denied.  The
//  delay is needed for cosmetic and anti-confusion reasons as this module needs to work with
//  existing pitacnomic module that handles aa, 00, 11, etc. prompting with the player.
//
static int _earlyBreachReqCB( char *str )
{
    if ( alarmStatus( aPtr ) < (long) piantirushConfig.earlyBreachSpeedup + piantirushConfig.trEarlyOfs ) {
        _localSay( piantirushConfig.breachOkPrompt );
        alarmReset( aPtr, 2L );   
        _capRusherClear();
        // _captureSpeed( 0 );   
    }
    else {
        _localSay( piantirushConfig.breachDenyPrompt );
    }
    return 0;
}


//  ==============================================================================================
//  _earlyTerritorialCB
//
static int _earlyTerritorialCB( char *str )
{
    // _localSay( "DEBUG: ok to capture" );
    return 0;
}

//  ==============================================================================================
//  _trainingModeReminderCB
//
static int _trainingModeReminderCB( char *str )
{
    if ( 0 != (int) strlen( piantirushConfig.trainingModeMsg ) )  {
        if ( NULL != strstr( rosterGetObjectiveType(), "WeaponCache" ) ) {
            _localSay( piantirushConfig.destroyOkPrompt );
            _localSay( piantirushConfig.trainingModeMsg );
        }
        else {
            _localSay( piantirushConfig.breachOkPrompt );
            _localSay( piantirushConfig.trainingModeMsg );
        }
    }
    return 0;
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
    if ( NULL != strcasestr( typeObj, "weaponcache" ) ) {
        strlcpy( outStr, piantirushConfig.warnRusherDestructible, 256 ); 
    }
    if ( NULL != strcasestr( typeObj, "capturable"  ) ) {
        strlcpy( outStr, piantirushConfig.warnRusherCapturable,   256 );
    }

    _localSay( outStr );
    if ( 0 != strlen( outStr )) kickArmed = 1;                       // kick safety : enable kick

    if (_fastMode) kickArmed = 0;                   // exception: in fast mode, do not kick anyone

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
    logPrintf( LOG_LEVEL_DEBUG, "piantirush", "Antirush NORMAL alarm triggered" );
    kickArmed = 0;                    // kick safety : disable kick
  
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
   
    // this block is for the offset timer/values used in "training mode" 
    // from PICLADMIN: "!wax on/off" 
    //
    if ( 0L == p2pGetL( "piantirush.p2p.training", 0L )) {
        piantirushConfig.trTimeOfs     = 0;
        piantirushConfig.trSlowOfs     = 0;
        piantirushConfig.trEarlyOfs    = 0;
        piantirushConfig.trEarDesOfs   = 0;
        piantirushConfig.trNPlayerOfs  = 0;
    }
    else {
        piantirushConfig.trTimeOfs     = piantirushConfig.lockIntervalSecTrainOffset;
        piantirushConfig.trSlowOfs     = piantirushConfig.slowIntervalSecTrainOffset;
        piantirushConfig.trEarlyOfs    = piantirushConfig.earlyBreachSpeedupTrainOffset;
        piantirushConfig.trEarDesOfs   = piantirushConfig.destroySpeedupTrainOffset;
        piantirushConfig.trNPlayerOfs  = piantirushConfig.nPlayerExemptionTrainOffset;
    }

    currentPlayerCount = apiPlayersGetCount();
    if (currentPlayerCount >= piantirushConfig.nPlayerLockThreshold ) {
        alarmReset( aPtr, piantirushConfig.lockIntervalSec + piantirushConfig.trTimeOfs );
        if ( 0L != p2pGetL( "piantirush.p2p.training", 0L ) )     // if training mode enabled "on" or "perm"
            alarmReset( xPtr, piantirushConfig.lockIntervalSec );   // training mode reminder
        // logPrintf(LOG_LEVEL_DEBUG, "piantirush", "Antirush start LOCK alarm set %d", piantirushConfig.lockIntervalSec + piantirushConfig.trTimeOfs);
        _captureSpeed( 2 );
    }
    else if (currentPlayerCount > ( piantirushConfig.nPlayerExemption + piantirushConfig.trNPlayerOfs ) )  { 
        if ( 0L != p2pGetL( "piantirush.p2p.training", 0L ) )     // if training mode enabled "on" or "perm"
            alarmReset( xPtr, piantirushConfig.slowIntervalSec );   // training mode reminder 
        alarmReset( aPtr, piantirushConfig.slowIntervalSec +  piantirushConfig.trSlowOfs);
        // logPrintf(LOG_LEVEL_DEBUG, "piantirush", "Antirush start SLOW alarm set %d", piantirushConfig.lockIntervalSec +  piantirushConfig.trSlowOfs);
        _captureSpeed( 1 );
    }
    else {
        alarmCancel( aPtr );  
        alarmCancel( xPtr );
        _captureSpeed( 0 );   
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
//  piantirushInitCB
//
//  Start of SISSM - in case it is "hot restarted" (game is running, sissm was restarted) make sure
//  the capture rate is set to "normal" until SISSM and the game are synchronized.
//
int piantirushInitCB( char *strIn )
{
    _captureSpeedForceNext();
    _captureSpeed( 0 );

    // clear the cumulative territorial zone antirush data structure
    //
    _capRusherClear();

    kickArmed = 0;

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
//  piantirushMapChangeCB
//
//  Force a capture rate update on map change
//
int piantirushMapChangeCB( char *strIn )
{
    _captureSpeedForceNext();
    return 0;
}

//  ==============================================================================================
//  piantirushGameStartCB
//
//  Force SLOW or LOCKED rate if qualified by # of players
//
int piantirushGameStartCB( char *strIn )
{
    // Disable the no-kick protection window.  "nokickprotect" is a countdown, typically
    // set to numbers like 15L seconds by picladmin !nk (!nokick) command to exempt
    // anticipated antrush auto-kicks for new players in-training, and commander calls over cache.
    //
    p2pSetL( "piantirush.p2p.nokickprotect", 0L );

    // if training mode is "on by default" in sissm1.cfg, for the p2p variable
    // to turn it on a start of each game.  Admins can still turn it off by !wax off.
    // This must be done before _startOfEverything()
    //
    if ( piantirushConfig.trainingOnDefaultEnable ) 
        p2pSetL( "piantirush.p2p.training", 1L );

    _captureSpeedForceNext();
    _startOfEverything();
    kickArmed = 0;

    return 0;
}

//  ==============================================================================================
//  piantirushGameEndCB
//
//  Force a capture rate update on end of game
//
int piantirushGameEndCB( char *strIn )
{
    // clear the training mode if "on", but not if "perm" - it is important to call this
    // before _startOfEverything() below.
    //
    if ( 1L == p2pGetL( "piantirush.p2p.training", 0L ) )  // check if "on", not "perm"
        p2pSetL( "piantirush.p2p.training", 0L );

    _captureSpeedForceNext();
    return 0;
}

//  ==============================================================================================
//  piantirushRoundStartCB
//
//  Force SLOW or LOCKED rate if qualified by # of players
//  Since this is start of round, put up a delayed warning message on screen
//  that anti-rush algorithm is active.
//
int piantirushRoundStartCB( char *strIn )
{
    // Clear the 'last' objective letter ID 'A', 'B', 'C', etc.  -- this is used to suppress 
    // extra printing of fast prompt message to screen during counterattacks.
    //
    _lastObjLetter = '~';

    kickArmed = 0;
    _captureSpeedForceNext();
    _startOfEverything();
    alarmReset( dPtr, piantirushConfig.warnRusherDelaySec );

    // Handle condition for early territorial cap rushers [TRUSH]
    //  
    if ( NULL != strstr( rosterGetObjectiveType(), "Capturable" )) {
        alarmReset( tPtr, alarmStatus( aPtr ) );    // copy from cap rate "slow down" timer 
    }
    else {
        alarmCancel( tPtr );
    }

    // clear the cumulative territorial zone antirush data structure
    //
    _capRusherClear();


    // if in training mode, announce it
    //
    if ( 0L != p2pGetL( "piantirush.p2p.training", 0L ) )  
       apiSay( piantirushConfig.trainingModeMsg );

    return 0;
}

//  ==============================================================================================
//  piantirushRoundEndCB
//
//  Force a capture rate update on end of round
//
int piantirushRoundEndCB( char *strIn )
{
    _captureSpeedForceNext();
    return 0;
}

//  ==============================================================================================
//  piantirushCapturedCB
//
//  Reset the capture rate timers to SLOW or LOCKED when the current objective is captured.  
//
int piantirushCapturedCB( char *strIn )
{
    kickArmed = 0;
    _startOfEverything(); 


    // if in training mode, announce it at Capture & RoundStart
    //
    if ( 0L != p2pGetL( "piantirush.p2p.training", 0L ) )
       apiSay( piantirushConfig.trainingModeMsg );

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
    int playersAlive;
    static int lastLivePlayerCount = -1;
    int currLivePlayerCount;
    long noProtectSecsL;

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

    // if we are running "fast" mode then turn on lcoal mute so that this plugin 
    // stays silent.
    //
    if ( currFastState ) { _localMute = 1;  _fastMode = 1; }
    else                 { _localMute = 0;  _fastMode = 0; }

    // if enabled process territorial antirush check.
    // This periodic processing increments the offender 'time' as well
    // as handle kicking upon allowed time expiry
    //
    if ( (piantirushConfig.autoKickEarlyBreach) ) {
        _capRusherPeriodicCheck();
    }

    // Process exemption of auto-kick on LMS conditions
    //
    playersAlive = apiBPPlayerCount();

    if ( playersAlive > _peakAlivePlayer ) {           // keep track of peak# of players
        _peakAlivePlayer = playersAlive;
    }
 
    if ( kickArmed && (_peakAlivePlayer > piantirushConfig.autoKickExemptLMS) ) {              
        if ( playersAlive <= piantirushConfig.autoKickExemptLMS )  {
            _localSay( piantirushConfig.autoKickExemptMsg );

            if ( NULL != strstr( rosterGetObjectiveType(), "WeaponCache" ))  {
                alarmCancel( rPtr );  // cancel any pending 'aa' request
                _localSay( piantirushConfig.destroyOkPrompt );
                kickArmed = 0;                    // kick safety : disable kick
                alarmCancel( aPtr );
                _captureSpeed( 0 );
           }

           else if ( NULL != strstr( rosterGetObjectiveType(), "Captur" ))  {
               alarmCancel( bPtr );   // cancel any pending 'aa' request  
               _localSay( piantirushConfig.breachOkPrompt );
                kickArmed = 0;                    // kick safety : disable kick
               alarmReset( aPtr, 2L );
               _capRusherClear();

           }
        }
    }
  
    // Useful information to print to the screen or log file - to determine
    // number of live players.  Only log changes to cut down on clutter.
    //
    currLivePlayerCount = apiBPPlayerCount();
    if ( lastLivePlayerCount  != currLivePlayerCount ) {
        logPrintf( LOG_LEVEL_INFO, "piantirush", "Change in live player count: %d -> %d", 
            lastLivePlayerCount, currLivePlayerCount );
        lastLivePlayerCount = currLivePlayerCount;
    }

    // State machine to handle kickArmed2 interlock that is compatible
    // with wave-respawn option (where number of players alive may go 'up' 
    // at any time.  Kickarmed2 is initialized to 2 at start of the objective. It is
    // primed when live players exceeds threshold.  After prime it is allowed to 
    // disarm when LMS condition occurs, and it is latched at this state until start 
    // of next objective.
    // 
    switch ( kickArmed2 ) {
    case 2:   // armed because objective has just begun
        if ( currLivePlayerCount > piantirushConfig.autoKickExemptLMS ) 
            kickArmed2 = 1;
        break;
    case 1:   // armed because minimum alive count exceeded the exemption threshold
        if ( currLivePlayerCount <= piantirushConfig.autoKickExemptLMS ) 
            kickArmed2 = 0;
        break;
    default:  // =0 disarmed because we are below minimum exemption threshold
        ;;    // do nothing, wait for synthetic objective event to set it back to "2"
        break;
    }

#if 0
    // Exception processing - regardless of the condition if min player exemption is set,
    // do not kick anyone as long as # player falls below the threshold.
    //
    if ( 0 != piantirushConfig.autoKickExemptLMS  ) {              
        kickArmed2 = 1;
        if ( currLivePlayerCount <= piantirushConfig.autoKickExemptLMS ) kickArmed2 = 0;
    }
    else 
        kickArmed2 = 1;
#endif

    // Service the "no kick" protection window countdown in seconds.  If it reaches 0L
    // then nokick protection becomes disabled.
    //
    noProtectSecsL =  p2pGetL( "piantirush.p2p.nokickprotect", 0L ) ;
    if ( noProtectSecsL != 0L ) {
         if ( --noProtectSecsL < 0L ) { 
             noProtectSecsL = 0L;
             logPrintf( LOG_LEVEL_INFO, "piantirush", "Nokick Protection window is now deactivated." );
         }
         else {
             logPrintf( LOG_LEVEL_INFO, "piantirush", "Nokick Protect window is active: %ld", noProtectSecsL );
         }
         p2pSetL( "piantirush.p2p.nokickprotect", noProtectSecsL );
    }

    // Service the "no wait" speedup feature (admins/moderators) 
    //
    if ( 0L != p2pGetL( "piantirush.p2p.SigNoWait", 0L )) {
        p2pSetL( "piantirush.p2p.SigNoWait", 0L );  // clear the semaphore 
        if ( alarmStatus( aPtr ) > 2L ) {
            _capRusherClear();
            alarmReset( aPtr, 2L );
        }
    }

    // If the server goes empty, make sure Training Mode and AutoKick Exemption is cleared.
    //
    if ( 0 == apiPlayersGetCount() ) {
        p2pSetL( "piantirush.p2p.training", 0L );
        p2pSetL( "piantirush.p2p.nokickprotect", 0L );
        p2pSetL( "piantirush.p2p.SigNoWait", 0L );
    }

    return 0;
}

//  ==============================================================================================
//  piantirushSigKillCB
//
//  This event handler executes whenever SISSM is terminated.  Since this can happen at 
//  any time, it is very important for SISSM to leave the game server in the "normal"
//  capture rate state.  If SISSM abruptly exited leaving the game server in LOCKED capture
/// rate, then the game becomes unplayable.
//
int piantirushSigKillCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "piantirush", "Restoring Normal Capture Rate before shutdown" );

    _captureSpeedForceNext();
    _captureSpeed( 0 );

    return 0;
}

//  ==============================================================================================
//  piantirushObjectSynth
//
//  This is a callback routine invoked when Objective change is detected synthetically
//  (by polling).  Simply schedule a warning to be printed to game (default 20 sec).
//  
int piantirushObjectSynth( char *strIn )
{
    alarmReset( dPtr, piantirushConfig.warnRusherDelaySec );

    // _startOfEverything();     
    // Handle condition for early territorial cap rushers [TRUSH]
    //  
    _capRusherClear();
    if ( NULL != strstr( rosterGetObjectiveType(), "Capturable" )) {
        alarmReset( tPtr, alarmStatus( aPtr ) );    // copy from cap rate "slow down" timer 
    }
    else {
        alarmCancel( tPtr );
    }

    // reset the variable that keeps track of peak# of "alive" player for this objective
    // leg.  This is used to track when to disarm auto-kick algorithm on LMS condition.
    //
    _peakAlivePlayer = -1;                // peak # of alive players on this objective leg
    kickArmed2 = 2;

    return 0;
}

//  ==============================================================================================
//  piantirushCacheDestroyed
//
//  This is a callback for when Cache is destroyed.  It checks number of seconds remaining on
//  timer and if a player destroyed this cache before time, he is kicked.
//  
int piantirushCacheDestroyed( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParseCacheDestroyed( strIn, 256, playerName, playerGUID, playerIP );
    // _localSay( "Cache destroyed by %s[%s]", playerName, playerGUID  );

    if ( piantirushConfig.autoKickEarlyDestruction ) {
        if ( ( _fastMode == 0 ) && ( alarmStatus( aPtr ) >= piantirushConfig.earlyDestroyTolerance )) {
            if ( kickArmed2 && kickArmed ) {
                // apiKickOrBan( 0, playerGUID, piantirushConfig.earlyDestroyKickMessage );
                if ( 0 == _localKickOrBan( 0, playerName, playerGUID, piantirushConfig.earlyDestroyKickMessage ) ) {
                    logPrintf( LOG_LEVEL_WARN, "piantirush", "%s[%s] Auto-kicked for early cache destruction", playerName, playerGUID );
                    _localSay( "'%s' auto-kicked for rushing", playerName );
                }
            }
        }
    }
    kickArmed = 0;
    return 0;
}

//  ==============================================================================================
//  piantirushChatCB
//
//  The timer for early cache-explode auto-kick can be accelerated (exempted) if a player
//  makes the effort to communicate with the team that the cache is about to be blown.
//  The pitacnomic plugin has several user defined two-letter-codes indicating early explosion
//  of cache.  This routine checks if such a code is registered, and if the timing
//  qualifies for eary explostion, resets the timer so the player will not be auto-kicked.
//  It will also schedule a "go/no-go" message to be printed on screen 5 seconds after -- the
//  reason for delay is due to potential for message prioritisation printing out-of-sequence
//  prompt that can be very confusing for the players.
// 
//  
int piantirushChatCB( char *strIn )
{
    char clientID[256], chatLine[256];
    int isQualified = 0, errCode;

    if ( piantirushConfig.autoKickEarlyDestruction ) {
        if ( NULL != strstr( rosterGetObjectiveType(), "WeaponCache" ))  {
            if ( 0 == (errCode = rosterParsePlayerChat( strIn, 256, clientID, chatLine ))) {
                if ( NULL != strstr( piantirushConfig.earlyDestroyPlayerRequest, chatLine )) {

                    // check if this player is qualified to call this command 
                    //
                    isQualified  = apiIsPlayerAliveByGUID( clientID );
                    isQualified |= apiAuthIsAttribute( clientID, "deadcmdr" );
                    isQualified |= apiIsHotRestart();
                    if ( 2 != (int) strlen( chatLine )) isQualified = 0;
                    if ( isQualified ) 
                        alarmReset( rPtr, 2 );        // give answer 2 seconds later for cosmetics 
                }
            }
        }
    }

    if ( piantirushConfig.autoKickEarlyBreach ) {
        if ( NULL != strstr( rosterGetObjectiveType(), "Captur" ))  {
            if ( 0 == (errCode = rosterParsePlayerChat( strIn, 256, clientID, chatLine ))) {
                if ( NULL != strstr( piantirushConfig.earlyBreachPlayerRequest, chatLine )) {
 
                    // check if this player is qualified to call this command 
                    //
                    isQualified  = apiIsPlayerAliveByGUID( clientID );
                    isQualified |= apiAuthIsAttribute( clientID, "deadcmdr" );
                    isQualified |= apiIsHotRestart();
                    if ( 2 != (int) strlen( chatLine )) isQualified = 0;

                    if ( isQualified )
                        alarmReset( bPtr, 2 );        // give answer 2 seconds later for cosmetics  
                }
            }
        }
    }

    return 0;
}

//  ==============================================================================================
//  piantirushObjTouchCB
//
//  This callback is invoked whenever a player enters the capture zone, OR within plantting proximity
//  to the destruction cache.  [TRUSH]
//  [2020.04.18-22.19.51:451][997]LogObjectives: Verbose: Objective OCCheckpoint_A_0: BP_Character_Player_C_0 entered.
//
int piantirushObjTouchCB( char *strIn )
{
    char *w, *n, m[64];
    char charID[64], tmpCharID[64], playerName[64]; 

    // First check if the time is within the territory  protection window zone (i.e., "too early")
    //
    if ( alarmStatus( tPtr ) > 5 ) {
        //  Get charcter ID
        //
        if ( NULL != (w = strstr( strIn, "BP_Character_Player" ))  ) {
            strlcpy( tmpCharID, w, 64 );
            if ( NULL != (n = getWord( tmpCharID, 0, ". " ) )) {
                strlcpy( charID, n, 64 );
                if ( NULL != strstr( strIn, rosterGetObjective() )) {
                    // Convert Character ID to player Name
                    //
                    strlcpy(m, apiCharacterToName( charID ), 64 );
                    if ( 0 != strlen( m )) {
                        strlcpy( playerName, m, 64 );

                        if ( (piantirushConfig.earlyBreachShow) && ( kickArmed2 && kickArmed )) {
                            _localSayThrottled( "'%s' %s", playerName, piantirushConfig.earlyBreachWarn );
                        }

                        // call the cap-rush algo submodule here
                        //
                        _capRusherEnterZone( charID, playerName );
                    }
                    else {
                        logPrintf(LOG_LEVEL_DEBUG, "piantirush", "**Internal error - unable to match char to name ::%s::", charID );
                    }
                }
            }
        } 
    }
    return 0;
}

//  ==============================================================================================
//  piantirushObjTouchCB
//
//  This callback is invoked whenever a player enters the capture zone, OR within plantting proximity
//  to the destruction cache.  [TRUSH]
//  [2020.04.18-22.19.52:390][ 53]LogObjectives: Verbose: Objective OCCheckpoint_A_0: BP_Character_Player_C_0 exited.
//
int piantirushObjUntouchCB( char *strIn )
{
    char *w, *n, m[64];
    char charID[64], tmpCharID[64], playerName[64]; 

    // First check if the time is within the territory  protection window zone (i.e., "too early")
    //
    if ( alarmStatus( tPtr ) > 5 ) {

        //  Get charcter ID
        //
        if ( NULL != (w = strstr( strIn, "BP_Character_Player" )) ) {
            strlcpy( tmpCharID, w, 64 );
            if ( NULL != (n = getWord( tmpCharID, 0, ". " ) )) {
                strlcpy( charID, n, 64 );
                if ( NULL != strstr( strIn, rosterGetObjective() )) {
                    // Convert Character ID to player Name
                    //
                    strlcpy(m, apiCharacterToName( charID ), 64 );
                    if ( 0 != strlen( m )) {
                        strlcpy( playerName, m, 64 );
                        if ( kickArmed2 && kickArmed ) _localSayThrottled( "'%s' %s", playerName, piantirushConfig.earlyBreachExit );
                        _capRusherExitZone( charID );
                    }
                }
            }
        }
    }
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
        cfsFetchStr( cP, "piantirush.lockPrompt", "Capture is locked until notified" ), CFS_FETCH_MAX);
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

    piantirushConfig.autoKickEarlyDestruction = (int)
        cfsFetchNum( cP, "piantirush.autoKickEarlyDestruction", 0 );
    piantirushConfig.destroySpeedup = (int)
        cfsFetchNum( cP, "piantirush.destroySpeedup", 90 );

    strlcpy( piantirushConfig.destroyOkPrompt,     // prompt authorizing players destroy the cache
        cfsFetchStr( cP, "piantirush.destroyOkPrompt", "Cache destruction AUTHORIZED if team concurs" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.destroyDenyPrompt,     // prompt denying players destroy the cache
        cfsFetchStr( cP, "piantirush.destroyDenyPrompt", "Early destruct DENIED at this time." ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.destroyHoldPrompt,     // prompt ordering players to halt destruction of cache
        cfsFetchStr( cP, "piantirush.destroyHoldPrompt", "*Cache destruct UNAUTHORIZED - type '22' then '11'" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.earlyDestroyKickMessage,     // 'reason' displayed to kicked player 
        cfsFetchStr( cP, "piantirush.earlyDestroyKickMessage", "Auto-kicked for Unauthorized early cache destruction" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.earlyDestroyPlayerRequest,     // command typed by players to request early destroy
        cfsFetchStr( cP, "piantirush.earlyDestroyPlayerRequest", "aa ff 11 22 ask req" ), CFS_FETCH_MAX);

    piantirushConfig.earlyDestroyTolerance = (int)          // do not kick players if # seconds prior to timer expiry
        cfsFetchNum( cP, "piantirush.earlyDestroyTolerance", 10 );

    piantirushConfig.earlyBreachShow     = (int) cfsFetchNum( cP, "piantirush.earlyBreachShow", 0 );
    piantirushConfig.autoKickEarlyBreach = (int) cfsFetchNum( cP, "piantirush.autoKickEarlyBreach", 0 );
    piantirushConfig.earlyBreachSpeedup  = (int) cfsFetchNum( cP, "piantirush.earlyBreachSpeedup", 90 );
    piantirushConfig.earlyBreachMaxTime  = (int) cfsFetchNum( cP, "piantirush.earlyBreachMaxTime", 12 );
    piantirushConfig.earlyBreachMaxTaps  = (int) cfsFetchNum( cP, "piantirush.earlyBreachMaxTaps", 7 );

    strlcpy( piantirushConfig.earlyBreachPlayerRequest,
        cfsFetchStr( cP, "piantirush.earlyBreachPlayerRequest", "aa ff 00 11 ask req"), CFS_FETCH_MAX);     
    strlcpy( piantirushConfig.earlyBreachTapsKickMessage,
        cfsFetchStr( cP, "piantirush.earlyBreachTapsKickMessage", "Auto-kicked for excessive number of nuisance objective taps"), CFS_FETCH_MAX);   
    strlcpy( piantirushConfig.earlyBreachTimeKickMessage,
        cfsFetchStr( cP, "piantirush.earlyBreachTimeKickedMessage", "Auto-kicked for early territorial zone breach"), CFS_FETCH_MAX);   

    strlcpy( piantirushConfig.breachOkPrompt,     // prompt authorizing players ok to breach
        cfsFetchStr( cP, "piantirush.breachOkPrompt", "Breach is AUTHORIZED if team concurs" ), CFS_FETCH_MAX);
    strlcpy( piantirushConfig.breachDenyPrompt,     // prompt denying players breach
        cfsFetchStr( cP, "piantirush.breachDenyPrompt", "Breach is DENIED at this time." ), CFS_FETCH_MAX);

    strlcpy( piantirushConfig.earlyBreachWarn, cfsFetchStr( cP, "piantirush.earlyBreachWarn", "premature breach warning"), CFS_FETCH_MAX);   
    strlcpy( piantirushConfig.earlyBreachExit, cfsFetchStr( cP, "piantirush.earlyBreachExit", "has left the objective"), CFS_FETCH_MAX);   

    piantirushConfig.autoKickExemptLMS = (int) cfsFetchNum( cP, "piantirush.autoKickExemptLMS", 2 );
    strlcpy( piantirushConfig.autoKickExemptMsg, cfsFetchStr( cP, "piantirush.autoKickExemptMsg", 
        "Early objective capture is AUTHORIZED due to low survivors"), CFS_FETCH_MAX);   


    // training mode vars - default adds "10 minutes" to rush timers but this is only activated if
    // "piantirush.p2p.training" is nonzero (set by picladmin)
    // Default = 600 means adds 10 minutes to objective leg; high bot servers run 15 minutes so lock first 10 minutes
    //
    piantirushConfig.nPlayerExemptionTrainOffset   = (int) cfsFetchNum( cP, "piantirush.nPlayerExemptionTrainOffset",     0 );  // 0 or -1
    strlcpy( piantirushConfig.trainingModeMsg, 
        cfsFetchStr( cP, "piantirush.trainingModeMsg", "Typing '11' before Breach/Blow is REQUIRED!!" ), CFS_FETCH_MAX);

    // training mode undocumented vars - these are undocumented parameters - not needed by most installations
    //
    piantirushConfig.lockIntervalSecTrainOffset    = (int) cfsFetchNum( cP, "piantirush.lockIntervalSecTrainOffset",    600 );
    piantirushConfig.slowIntervalSecTrainOffset    = (int) cfsFetchNum( cP, "piantirush.slowIntervalSecTrainOffset",    600 );
    piantirushConfig.earlyBreachSpeedupTrainOffset = (int) cfsFetchNum( cP, "piantirush.earlyBreachSpeedupTrainOffset", 600 );
    piantirushConfig.destroySpeedupTrainOffset     = (int) cfsFetchNum( cP, "piantirush.destroySpeedupTrainOffset",     600 );
    piantirushConfig.trainingOnDefaultEnable       = (int) cfsFetchNum( cP, "piantirush.trainingOnDefaultEnable",         0 );

    piantirushConfig.trTimeOfs     = 0;
    piantirushConfig.trSlowOfs     = 0;
    piantirushConfig.trEarlyOfs    = 0;
    piantirushConfig.trEarDesOfs   = 0;
    piantirushConfig.trNPlayerOfs  = 0;

    // Disable Training by default.  In the future this can be copied from sissm.cfg 
    // but default "off" is good for now
    //
    p2pSetL( "piantirush.p2p.training", 0L );

    // Clear the semaphore for 'no wait' speedup request coming from 
    // picladmin
    // 
    p2pSetL( "piantirush.p2p.SigNoWait", 0L );

    cfsDestroy( cP );

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

    // Setup alarms
    //
    aPtr  = alarmCreate( _normalSpeedAlarmCB );
    dPtr  = alarmCreate( _rulesAlarmCB );
    wPtr  = alarmCreate( _notifyNoDestroyCB );

    rPtr  = alarmCreate( _earlyDestroyReqCB );
    tPtr  = alarmCreate( _earlyTerritorialCB );    // Territorial Rush trap [TRUSH]
    alarmReset( rPtr, 0L );
    alarmReset( tPtr, 0L );

    bPtr = alarmCreate( _earlyBreachReqCB );
    alarmReset( bPtr, 0L );

    xPtr = alarmCreate( _trainingModeReminderCB );
    alarmReset( xPtr, 0L );

    kickArmed = 0;

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
    eventsRegister( SISSM_EV_CACHE_DESTROY,        piantirushCacheDestroyed );
    eventsRegister( SISSM_EV_CHAT,                 piantirushChatCB     );

    eventsRegister( SISSM_EV_BP_TOUCHED_OBJ,       piantirushObjTouchCB );
    eventsRegister( SISSM_EV_BP_UNTOUCHED_OBJ,     piantirushObjUntouchCB );


    return 0;
}


