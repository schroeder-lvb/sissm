//  ==============================================================================================
//
//  Module: API
//
//  Description:
//  Main control/query interface for the plugins
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================
//

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include "winport.h"         // sleep/usleep function
#endif


#include "util.h"
#include "bsd.h"
#include "log.h"
#include "cfs.h"
#include "ftrack.h"
#include "events.h"
#include "rdrv.h"
#include "alarm.h"
#include "sissm.h"                                              // required for sissmGetConfigPath
#include "roster.h"

#include "api.h"
#include "p2p.h"

#define API_MAX_GROUPS                  ( 6 )                    // max number of privilege groups 

#define API_LOG2RCON_DELAY_MICROSEC  (250000)          // system delay between log to rcon (tuned) 
#define API_LISTPLAYERS_PERIOD           (10)     // #seconds periodic for listserver roster fetch

#define API_ROSTER_STRING_MAX        (256*64)      // max size of contatenated roster string +EPIC
#define API_LINE_STRING_MAX             (256)   

#define API_MAXMAPS                     (512)    // max maps listed in mapcycle.txt

//  ==============================================================================================
//  Data definition  
//
//
//
static rdrvObj *_rPtr = NULL;                    // RCON driver handle - interface to game server
static alarmObj *_apiPollAlarmPtr  = NULL;      // used to periodically poll roster (listplayers)

static int _charTrackingActive = 0;       // is character tracking is enalbed for Insurgency.log?

//  Store string concatenated roster for two consecutive iterations - previous & current.
//  The difference produces player connection and disconnection status
//  
char rosterPrevious[ API_ROSTER_STRING_MAX ];    
char rosterCurrent[ API_ROSTER_STRING_MAX ];
static unsigned long lastRosterSuccessTime = 0L;  // marks last time listplayer read was success 

char gameStateCurrent[ API_ROSTER_STRING_MAX ];
char gameStatePrevious[ API_ROSTER_STRING_MAX ];
static unsigned long lastGameStateSuccessTime = 0L;  // marks last time activeobjective was success 

//  Table of admins list - can be used by any plugins to identify if 
//  a transction is originated from an admin.  See: apiIdListCheck().
//
char rootguids[ API_LINE_STRING_MAX ];                     // separate list for root owner GUIDs
char rootname[ API_LINE_STRING_MAX ];                               // name of root group 'root'

char everyoneCmds[ API_LINE_STRING_MAX ];             // list of commands enabled for 'everyone'
char everyoneMacros[ API_LINE_STRING_MAX ];             // list of macros enabled for 'everyone'
char everyoneAttr[ API_LINE_STRING_MAX ];           // list of attributes enabled for 'everyone'

char soloAdminCmds[ API_LINE_STRING_MAX ];           // list of commands enabled for 'soloadmin'
char soloAdminMacros[ API_LINE_STRING_MAX ];           // list of macros enabled for 'soloadmin'
char soloAdminAttr[ API_LINE_STRING_MAX ];         // list of attributes enabled for 'soloadmin'

static char removeCodes[ API_LINE_STRING_MAX ];   // color codes to optionally remove (circleus)
static char sayPrefix[ API_LINE_STRING_MAX ];            // replacement ADMIN: prefix (circleus) 
static char sayPostfix[ API_LINE_STRING_MAX ];            // postfix for each message e.g., </>

struct {
    char groupname[ API_LINE_STRING_MAX ];           // name of the group e.g., "sradmin, admin"
    char groupcmds[ API_LINE_STRING_MAX ];         // allowed commands e.g., "rcon help version"
    char groupmacros[ API_LINE_STRING_MAX ];             // allowed macros "execute <macroname>"
    char groupattr[ API_LINE_STRING_MAX ];                        // privileges e.g.,  "priport"
    char groupguid[ API_LINE_STRING_MAX ];            // filepath of GUIDs "/home/my/admins.txt"
    idList_t groupIdList;                                                         // Admins list 
} groups[ API_MAX_GROUPS ];

// static idList_t adminIdList;                                                  // Admins list 
// static char adminListFilePath[ API_LINE_STRING_MAX ];        // full file path to admins.txt

static wordList_t badWordsList;                                               // Bad words list
static char badWordsFilePath[ API_LINE_STRING_MAX ];            // full file path to admins.txt

static char mapcycleFilePath[ API_LINE_STRING_MAX ];          // full file path to mapcycle.txt

struct {
    char mapName[ 256];    // map name required for traveler
    char reqName[ 256 ];   // map name requested by player (typed)
    char scenario[ 256 ];  // scenario name
    char traveler[ 256 ];  // traveler name (derived)
    int  dayNight;         // 0 = day, 1 = night
    int  secIns;           // -1 = unknown/n.a., 0 = security, 1 = insurgent 
    char gameMode[ 40 ];   // game mode (checkpoint, checkpointhardcore, etc.)
} mapCycleList[ API_MAXMAPS ];

char mapNameMerged[ API_MAXMAPS ][40];

char mutAllowed[ 16000 ];                             // allowed mutators list, space separated
char mutAllowedList[ API_MUT_MAXLIST ][ API_MUT_MAXCHAR ];             // list version of the above
char mutDefault[   256 ];          // default mutator(s) - should match what is on the launcher

// Reboot reason & time, for analytics and web display
//
static char lastRebootReason[256];                          // short description of last reboot
static unsigned long lastRebootTime;                               // store time of last reboot

// Data used for translating CharacterID and Playernames
// This is used by piantirush for detecting cap rushers on 
// territorial point
//
struct {
    char charID[ 256 ];                       // system internal ID e.g., BP_Character_Player_C_0
    char playerName[ 256 ];                        // player commmon name e.g., "robert", "alice"
} soldierTranslate[ API_MAXENTITIES ];

static char _cachedName[256];

#define API_OBJECTIVES_MAX    (64)

static char _objectiveListCache[API_OBJECTIVES_MAX][ API_LINE_STRING_MAX ];

#if FINALCOUNTER_REDUCE
// Crash protection GameModeProperties for limiting botcounts on Checkpoint/HardcoreCheckpoint 
// Final Counterattack
// 
int maxBotsCounterAttack = 30;                         // max bots per wave on final counterattack
double finalCacheBotQuotaMultiplier = 0;                     // 0=disable, typical value 1.2 - 1.5
#endif


//  ==============================================================================================
//  apiRemoveCodes
//
//  Removes list of embedded substring codes from a string
//
void apiRemoveCodes( char *apiSayString )
{
    int j = 0;
    char *w;
    char removeWord[ API_LINE_STRING_MAX ];

    if ( 0 != strlen( removeCodes ) ) {
        while ( 1 == 1 ) {
            w = getWord( removeCodes, j++, " " );
            if ( w == NULL ) break;
            if ( 0 == strlen( w ) ) break;
            strlcpy( removeWord, w, API_LINE_STRING_MAX );
            strRemoveInPlace( apiSayString, removeWord );
        };
    };
    return;
}


//  ==============================================================================================
//  apiWordListRead
//
//  Reads a list of words from a file - words must be entered one line each.
//  This function returns number of words read.  If the file fails to open, -1 is returned.
//
int apiWordListRead( char *listFile, wordList_t wordList )
{
    int i;
    char tmpLine[1024];
    FILE *fpr;

    for (i=0; i<WORDLISTMAXELEM; i++) strclr( wordList[i] );

    i = -1;
    if (NULL != (fpr = fopen( listFile, "rt" ))) {
        i = 0;
        while (!feof( fpr )) { 
            if (NULL != fgets( tmpLine, 1024, fpr )) {
                tmpLine[ strlen( tmpLine ) - 1] = 0;
                strTrimInPlace( tmpLine );
                if ( 0 != strlen( tmpLine ) ) {
                    // strToLowerInPlace( tmpLine ); 
                    strlcpy( wordList[ i ], tmpLine, WORDLISTMAXSTRSZ );
                    if ( (++i) > WORDLISTMAXELEM) break;
                }
            }
        }
        fclose( fpr );
    }
    return( i );
}


//  ==============================================================================================
//  apiWordListCheck
//
//  Checks if a a word is contained in the string.  This function returns a 1 if found,
//  0 if not found.
//
int apiWordListCheck( char *stringTested, wordList_t wordList )
{
    int i;
    int isMatch = 0;

    for (i=0; i<WORDLISTMAXELEM; i++) {

        if ( wordList[i][0] == 0 ) break;
        if ( NULL != strcasestr( stringTested, wordList[i] )) {
            isMatch = 1;
            break;
        }
    }
    return( isMatch );
}

//  ==============================================================================================
//  apiWordListCheckExact
//
//  Checks if a a word is exact match of the string.  This function returns a 1 if found,
//  0 if not found.
//
int apiWordListCheckExact( char *stringTested, wordList_t wordList )
{
    int i;
    int isMatch = 0;

    for (i=0; i<WORDLISTMAXELEM; i++) {

        if ( wordList[i][0] == 0 ) break;
        if ( 0 == strncmp( stringTested, wordList[i], WORDLISTMAXSTRSZ ) ) {
            isMatch = 1;
            break;
        }
    }
    return( isMatch );
}



//  ==============================================================================================
//  apiIdListRead
//
//  Reads a list of STEAMID64 from a file.  This can be used to check for admins, moderators,
//  or any other group of clients such as friends, watch-list, etc.
//  This function returns number of clients read.  If file fails to open, -1 is returned.
//  The input list format is compatible wtih Sandstorm Admins.txt.
//
int apiIdListRead( char *listFile, idList_t idList )
{
    int  i, idType;
    char tmpLine[1024], testGUID[256]; //  *w;
    FILE *fpr;

    for (i=0; i<IDLISTMAXELEM; i++) strclr( idList[i] );

    // EPIC

    i = -1;
    if (NULL != (fpr = fopen( listFile, "rt" ))) {
        i = 0;
        while (!feof( fpr )) {
            if (NULL != fgets( tmpLine, 1024, fpr )) {
                tmpLine[ strlen( tmpLine ) - 1] = 0;

                strlcpy( testGUID, tmpLine, IDEPICIDLEN+1 );           // 65-char EPIC ID + terminator
                if ( 2 == ( idType = rosterIsValidGUID( testGUID ) )) {
                    strlcpy( idList[i++], testGUID, IDEPICIDLEN+1 );
                }
                else {
                    strlcpy( testGUID, tmpLine, IDSTEAMID64LEN+1 );   // 17-char Steam ID + terminator
                    if ( 1 == ( idType = rosterIsValidGUID ( testGUID ))) { 
                        strlcpy( idList[i++], testGUID, IDSTEAMID64LEN+1 );
                    }
                } 
                if ( i >= (IDLISTMAXELEM-1) ) break;    // check for overflow
            }
        }
        fclose( fpr );
    }
    return( i );
}


//  ==============================================================================================
//  apiBadNameCheck
//
//  Checks if bad name.  This function returns a 1 if bad, 0 if ok
//
int apiBadNameCheck( char *nameIn, int exactMatch )
{
    if ( exactMatch ) 
        return (apiWordListCheckExact( nameIn, badWordsList )); 
    else
        return (apiWordListCheck( nameIn, badWordsList )); 
}


//  ==============================================================================================
//  apiIdListCheck
//
//  Checks if a client GUID is contained in the idList.  This function returns a 1 if found,
//  0 if not found.
//
int apiIdListCheck( char *connectID, idList_t idList )
{
    int i;
    int isMatch = 0;

    for (i=0; i<IDLISTMAXELEM; i++) {
        if (0 == strcmp( idList[i], connectID )) {
            isMatch = 1;
            break;
        }
    }
    return( isMatch );
}


//  ==============================================================================================
//  apiGetLastRosterTime
//
//  Returns last successful roster fetch from RCON.  This is used by plugins (such as
//  pirebooter) to detect server crash, that may lead to system restart.
//
unsigned long apiGetLastRosterTime( void )
{
    return( lastRosterSuccessTime );
}

//  ==============================================================================================
//  _apiSyntheticDelDispatch
//
//  Synthetic (self generated) event that dispatches event handles of subscribed plugins
//  when a player has disconnected.  This is a  callback routine from the roster module.
//
int _apiSyntheticDelDispatch(char *playerName, char *playerIP, char *playerGUID )
{
    char strOut[API_LINE_STRING_MAX];

    snprintf( strOut, API_LINE_STRING_MAX, "~SYNTHDEL~ %s %s %s", playerGUID, playerIP, playerName );
    eventsDispatch( strOut  );
    return 0;
}


//  ==============================================================================================
//  _apiSyntheticAddDispatch
//
//  Synthetic (self generated) event that dispatches event handles of subscribed plugins
//  when a player has connected.  This is a callback routine from the roster module.
//
int _apiSyntheticAddDispatch(char *playerName, char *playerIP, char *playerGUID ) 
{
    char strOut[API_LINE_STRING_MAX];

    snprintf( strOut, API_LINE_STRING_MAX, "~SYNTHADD~ %s %s %s", playerGUID, playerIP, playerName );
    eventsDispatch( strOut  );
    return 0;
}


//  ==============================================================================================
//  _apiSyntheticObjDispatch
//
//  Synthetic (self generated) event that dispatches event handles of subscribed plugins
//  when an active objective has changed (Co-op mode only)
//
//  If parsing is successful, this routine generates 'strOut' going to individual event 
//  handler, with space delimited 7-word string that looks like:
//
//     ~SYNTHOBJ~  // 0
//     ObjectiveCapturable   // 1
//     /Game/Maps/Compound/Outskirts_Checkpoint_Security  // 2
//     Outskirts_Checkpoint_Security // 3
//     PersistentLevel // 4
//     OCCheckpoint_A // 5
//
int _apiSyntheticObjDispatch(char *activeObjectRaw )   
{
    char strOut[API_LINE_STRING_MAX], *w;
    int i, errCode = 0;

    // Repackage the log line into something we can use -
    // a space delimited parameters that we can parse easily
    //
    strlcpy( strOut, "~SYNTHOBJ~", API_LINE_STRING_MAX ); 
    for (i=0; i<5; i++) {
        w = getWord( activeObjectRaw, i, " =\"\'.:" );
        if  ( w != NULL )  {
            strlcat( strOut, " ", API_LINE_STRING_MAX );
            strlcat( strOut, w,   API_LINE_STRING_MAX );
        }
        else  {
            errCode = 1;
            break;
        }
    }

    // Update some internal variables and finally call a Synthetic 
    // dispatcher to signal the plugins // something has updated.
    //
    if ( !errCode ) {
        // Update the database with ActiveObject content just decoded.
        // 
        rosterSetMapName( getWord( strOut, 3, " " ));
        rosterSetObjective( getWord( strOut, 5, " " ), getWord( strOut, 1, " " ));
        logPrintf( LOG_LEVEL_DEBUG, "api", "Updating DB with ActiveObject ::%s::%s::%s::", 
            getWord( strOut, 3, " " ), getWord( strOut, 5, " " ), getWord( strOut, 1, " " ) );

        // Generate a Synthetic event signalling change in ActiveObject content
        // 
        eventsDispatch( strOut );                        // dispatch subscribed plugin Callbacks 
    }

    return 0;
}

//  ==============================================================================================
//  _apiFetchGameState (local function)
//
//  Read GameState by reading gamemodeproperty "activeobjective", update the internal
//  cache variable, dispatch a change event.   This routine is called from both map change 
//  event as well as periodic (for "hot-restart" correction).
//
static int _apiFetchGameState( void )
{
    int errCode = 1;

    strlcpy( gameStateCurrent, apiGameModePropertyGet( "activeobjective" ), API_ROSTER_STRING_MAX );
    if ( 0 != strlen( gameStateCurrent ) ) {
        lastGameStateSuccessTime = apiTimeGet();   // record last time value read succesfully
        errCode = 0;
        if ( 0 != strncmp( gameStatePrevious, gameStateCurrent, API_ROSTER_STRING_MAX )) {
            strlcpy( gameStatePrevious, gameStateCurrent, API_ROSTER_STRING_MAX );
            logPrintf( LOG_LEVEL_INFO, "api", "ActiveObjective ::%s::", gameStatePrevious );
            _apiSyntheticObjDispatch( gameStatePrevious );
        }
    }
    return errCode;
}



//  ==============================================================================================
//  _apiPollAlarmCB (local function)
//
//  Call-back function dispatched by self-resetting periodic alarm (system alarm dispatcher),
//  however, this routine is also called 1) when Client-Add (connection) event activated, 
//  and 2) when client-Del (disconection) event is activated (see below).  
//
//  This method uses RCON interface to fetch the player roster list from
//  the game server, determines if any clients have connected or disconnected, then generates
//  a synthetic event to invoke registered plugin callbacks.
//
int _apiPollAlarmCB( char *strIn )
{
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE];
    int  bytesRead, errCode = 0;

    logPrintf( LOG_LEVEL_RAWDUMP, "api", "Roster update alarm callback ::%s::", strIn );

    // Fetch the roster
    //
    snprintf( rconCmd, API_T_BUFSIZE, "listplayers" );
    memset( rconResp, 0, API_R_BUFSIZE );
    errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );
    if ( !errCode ) {
        rosterParse( rconResp, bytesRead );
        // logPrintf( LOG_LEVEL_DEBUG, "api", "Listplayer success, player count is %d", rosterCount());
        lastRosterSuccessTime = apiTimeGet();             // record this to check for dead servers
	strlcpy( rosterCurrent, rosterPlayerList( 4, "\011" ), API_ROSTER_STRING_MAX );
	rosterSyntheticChangeEvent( rosterPrevious, rosterCurrent, _apiSyntheticDelDispatch ); 
	rosterSyntheticChangeEvent( rosterCurrent, rosterPrevious, _apiSyntheticAddDispatch ); 
	strlcpy( rosterPrevious, rosterCurrent, API_ROSTER_STRING_MAX );

    }
    else {
        logPrintf( LOG_LEVEL_INFO, "api", 
            "Listplayer retrieve failure on apiPollAlarmCB, playercount is %d", rosterCount() );
    }

    // Fetch the GameState using activeobjective gamemodeproperties  
    //   
    _apiFetchGameState();

    // do a cached-read of server name from RCON.  Once successful, it will read
    // from the cache (not RCON) thereafter
    //
    apiGetServerNameRCON( 0 );

    // reset the alarm for the next iteration
    //
    alarmReset( _apiPollAlarmPtr, API_LISTPLAYERS_PERIOD );

    return 0;
}

//  ==============================================================================================
//  _apiChatCB
//
//  Call-back function for player typing something into the chat box - for the API module 
//  this is used to aid debug/developemt only.
// 
int _apiChatCB( char *strIn )
{
    
#if SISSM_TEST

    // This block of code allows a single developer to test multiplayer 
    // condition by faking the active player count reported by this module.
    //
    if ( NULL != strstr( strIn, "debug0000" )) {
        _overrideAliveCount = 0;
       apiSay("Debug Mode alive=0");
    }

    if ( NULL != strstr( strIn, "debug0001" )) {
        _overrideAliveCount = 1;
       apiSay("Debug Mode alive=1");
    }

    if ( NULL != strstr( strIn, "debug0002" )) {
        _overrideAliveCount = 2;
       apiSay("Debug Mode alive=2");
    }

    if ( NULL != strstr( strIn, "debug0003" )) {
        _overrideAliveCount = 3;
       apiSay("Debug Mode alive=3");
    }
    if ( NULL != strstr( strIn, "debug0006" )) {
        _overrideAliveCount = 6;
       apiSay("Debug Mode alive=6");
    }

#endif
    
    return 0;
}


	
//  ==============================================================================================
//  _apiPlayerConnectedCB
//
//  Call-back function dispatched by system event dispatcher when the game logfile 
//  indicates a new player connection.
//
int _apiPlayerConnectedCB( char *strIn )
{
    logPrintf( LOG_LEVEL_RAWDUMP, "api", "Player Connected callback ::%s::", strIn );

    // Add delay here because there is a time lag between log file reporting 'add connection'
    // until the new player shows up on RCON roster read via the listplayer command.
    // The delay becomes more pronounced on a busy (CPU loaded) server state.
    //
    usleep( API_LOG2RCON_DELAY_MICROSEC );

    // Update the roster which also resets the schedule for next refresh
    // This routine resets the next alarm time 
    // 
    _apiPollAlarmCB("PlayerConnected");

    return 0;
}


//  ==============================================================================================
//  _apiPlayerDisconnectedCB
//
//  Call-back function dispatched by system event dispatcher when the game logfile 
//  indicates a player has disconnected.
//
int _apiPlayerDisconnectedCB( char *strIn )
{
    logPrintf( LOG_LEVEL_RAWDUMP, "api", "Player Disconnected callback ::%s::", strIn );

    // Update the roster which also resets the schedule for next refresh
    // This routine resets the next alarm time
    //
    _apiPollAlarmCB("PlayerDisconnected");

    return 0;
}

//  ==============================================================================================
//  _apiTravelCB
//
//  Call-back function dispatched when the game system log file indicates a travel.
//  Simply update the name which can be read back from Plugins via the API at later time.
//
int _apiTravelCB( char *strIn )
{
    rosterSetTravel( strIn );
    return 0;
}

//  ==============================================================================================
//  _apiSessionLogCB
//
//  Call-back function dispatched when the game system log file indicates a start of 
//  session logging (game that can be played-back usign session ID at later time).
//
int _apiSessionLogCB( char *strIn )
{
    char _currSession[API_LINE_STRING_MAX];

    rosterParseSessionID( strIn, API_LINE_STRING_MAX, _currSession );
    rosterSetSessionID( _currSession );

    logPrintf( LOG_LEVEL_INFO, "api", "SessionID: ::%s::", _currSession );
    return 0;
}


//  ==============================================================================================
//  _apiBPCharInitCB
//
//  Call-back for CharacterID-to-PlayerName Translation function
//  Initialize the internal table - to be called at start of app
//  and at map change.
//
int _apiBPCharInitCB( char *strIn )
{
    int i;

    strclr( _cachedName );
    for ( i=0; i<API_MAXENTITIES; i++ ) {
        strclr( soldierTranslate[ i ].charID );
        strclr( soldierTranslate[ i ].playerName );
    }
    logPrintf( LOG_LEVEL_INFO, "api", "BPChar Translation Table cleared" );
    return 0;
}

//  ==============================================================================================
//  apiBPIsActive
// 
//  True if server operator has enabled the character tracking in Insurgency.log file
// 
//
int apiBPIsActive( void )
{  
    return ( _charTrackingActive );
}


//  ==============================================================================================
//  apiBPPlayerCount
//
//  Count how many human player BPChars are active in the table.
//
int apiBPPlayerCount( void )
{
    int i;
    int countPlayers = 0;

    for ( i=0; i<API_MAXENTITIES; i++ ) {
        if (0 != strlen( soldierTranslate[ i ].playerName ) ) countPlayers++;
    }

#if SISSM_TEST
    if ( _overrideAliveCount ) countPlayers = _overrideAliveCount;
#endif

    return countPlayers;
}

//  ==============================================================================================
//  _apiBPCharNameCB (internal)
//
//  Call-back for CharacterID-to-PlayerName Translation function 'SS_SUBSTR_BP_PLAYER_CONN'
//  This function parses the log string of the form:
//    [2020.04.17-15.16.06:068][601]LogGameMode: Verbose: RestartPlayerAtPlayerStart PlayerName
//  and updates the static var _cacheName with 'PlayerName'
//
int _apiBPCharNameCB( char *strIn )
{
    char *w, *n = NULL;
    if ( NULL != (w = strstr( strIn, SS_SUBSTR_BP_PLAYER_CONN ) )) {
        if (NULL != ( n = &w[ strlen( SS_SUBSTR_BP_PLAYER_CONN ) ])) {
            if ( n != NULL ) {
                strlcpy( _cachedName, n, 256 );
                strClean( _cachedName );   // in-line remove trailing CR or LF 
                // printf("\nStored to cachedName ::%s::\n", n);
            }
        }
    }
    return 0;
}



//  ==============================================================================================
//  _apiBPCharPlayerCB
//
//  Call-back for CharacterID-to-PlayerName Translation function 'SS_SUBSTR_BP_CHARNAME'
//  This function parses the log string of the form:
//    [2020.04.17-15.16.06:101][601]LogGameMode: Verbose: PAWNREUSE: 'INSPlayerController_0' cached new pawn 'BP_Character_Player_C_0'.
//  and inserts CharacterID and PlayerName pair into the lookup table.
//
int _apiBPCharPlayerCB( char *strIn )
{
    char *w, *n = NULL;
    int i;

    if ( NULL != (w = strstr( strIn, SS_SUBSTR_BP_CHARNAME ) )) {
        _charTrackingActive = 1;      // indicate character tracking is enalbed for Insurgency.log
        w = strstr( strIn, "BP_Character_Player" );
        if ( w != NULL ) {
            if ( NULL != (n = getWord( w, 0, " .'" )) ) {
                ;;
                // printf("\nConnect ::%s::%s\n", n, _cachedName );
            }
        }
    }

    if (( 0 != strlen( _cachedName )) && ( n != NULL ) ) {

        // First pass, remove preexisting or duplicate characterID and playerName
        //
        for ( i = 0; i<API_MAXENTITIES; i++ )  {
            if ( 0 == strcmp( n, soldierTranslate[ i ].charID )) {
                strclr( soldierTranslate[ i ].charID );
                strclr( soldierTranslate[ i ].playerName );
            }
            if ( 0 == strcmp( _cachedName, soldierTranslate[ i ].playerName )) {
                strclr( soldierTranslate[ i ].charID );
                strclr( soldierTranslate[ i ].playerName );
            }
        }

        // insert into the translation table
        //
        for ( i = 0; i<API_MAXENTITIES; i++ )  {
            if ( 0 == strlen( soldierTranslate[ i ].charID ) ) {
                strlcpy( soldierTranslate[ i ].playerName, _cachedName, 256 );
                strlcpy( soldierTranslate[ i ].charID,               n, 256 );
                logPrintf( LOG_LEVEL_INFO, "api", "BPChar %s bound to '%s'", n, _cachedName );
                break;
            }
        }
    }

    strclr( _cachedName );
    return 0;
}


//  ==============================================================================================
//  _apiBPCharDisconnectCB
//
//  Call-back for CharacterID-to-PlayerName Translation function 'SS_SUBSTR_BP_PLAYER_DISCONN'
//
//  This function parses the log string of the form:
//    [2020.04.17-15.44.53:971][572]LogGameMode: Verbose: PAWNREUSE: BP_Character_Player_C_0 role 3 Unpossessed
//  and removes the matching CharacterID-Playername record from the lookup table.
//
int _apiBPCharDisconnectCB( char *strIn )
{
    char *w, *n = NULL;
    int i;

    if ( NULL != (w = strstr( strIn, SS_SUBSTR_BP_PLAYER_DISCONN ) )) {
        w = strstr( strIn, "BP_Character_Player" );
        if ( w != NULL ) {
            if ( NULL != (n = getWord( w, 0, " .'" )) ) {
                // printf("\nDisconnect ::%s::\n", n);
                ;
            }
        }
    }

    if ( n != NULL ) {

        // Remove from translation database
        // Check all entries and remove duplicates to be safe
        //
        for ( i = 0; i<API_MAXENTITIES; i++ )  {
            if ( 0 == strcmp( n, soldierTranslate[ i ].charID )) {
                logPrintf( LOG_LEVEL_INFO, "api", "BPChar '%s' unbound from '%s'", n, soldierTranslate[ i ].playerName );
                strclr( soldierTranslate[ i ].charID     );
                strclr( soldierTranslate[ i ].playerName );
            }
        }
    }

    return 0;
}

//  ==============================================================================================
//  _apiBPCharTouchCB
//
//  Call-back function dispatched when a character enters a territorial (or weapons cache) zone.
//  This CB is subject to poortly designed map where this event can oscillate at rate of 
//  60 events per second.   
//
int _apiBPCharTouchCB( char *strIn )  
{
    return 0;
}
  
//  ==============================================================================================
//  _apiBPCharUntouchCB
//
//  Call-back function dispatched when a character leaves a territorial (or weapons cache) zone.
//  This CB is subject to poortly designed map where this event can oscillate at rate of 
//  60 events per second.  
//
int _apiBPCharUntouchCB( char *strIn )
{
    return 0;
}


//  ==============================================================================================
//  _apiMapChangeCB
//
//  Call-back function dispatched when the game system log file indicates a map change.
//  Simply update the name which can be read back from Plugins via the API at later time.
//
int _apiMapChangeCB( char *strIn )
{
    char _currMap[API_LINE_STRING_MAX];
    int   i;
    int errCode;

    // There are two ways to find the current map.  The code below works by watching
    // the Insurgency.log file which works for all game modes.  For Co-op this is 
    // later superceded by synthetic map change event, reading data from activeobjectives
    // gamemodeproperty.
    //
    rosterParseMapname( strIn, API_LINE_STRING_MAX, _currMap );
    strClean( _currMap );
    rosterSetMapName( _currMap );

    _apiBPCharInitCB( strIn );

    // on map change, clear the objectives
    //
    for (i=0; i<API_OBJECTIVES_MAX; i++) strclr( _objectiveListCache[i]) ;
    logPrintf(LOG_LEVEL_INFO, "api", "Map change to ::%s:: and resetting the internal objectives cache", _currMap );


    // Update the change in GameState.  This may fail on some slower servers, so the following
    // routine is also called by the periodic CB. 
    // 
    errCode = _apiFetchGameState();
    logPrintf( LOG_LEVEL_INFO, "api", "MapChange ActiveObjective update::%s:: and Error ::%d::", gameStatePrevious, errCode );

    return 0;
}



//  ==============================================================================================
//  apiNameToCharacter
//
//  Translate playerName ("Robert") to internal Character ID ("BP_Character_Player_C_0")
//  If translation is not successful, empty string is returned.
//
char *apiNameToCharacter( char *playerName )
{
    int i;
    static char w[ 256 ];

    strclr( w );
    for ( i = 0; i<API_MAXENTITIES; i++ )  {
        if ( 0 == strcmp( playerName, soldierTranslate[ i ].playerName )) {
            strlcpy( w, soldierTranslate[ i ].charID, 256 );
            break;
        }
    }
    return( w );
}

//  ==============================================================================================
//  apiCharacterToName
//
//  Translate internal Character ID ("BP_Character_Player_C_0") to playerName ("Robert")
//  If translation is not successful, empty string is returned.
//
char *apiCharacterToName( char *characterID )
{
    int i;
    static char w[ 256 ];

    strclr( w );
    for ( i = 0; i<API_MAXENTITIES; i++ )  {
        if ( 0 == strcmp( characterID, soldierTranslate[ i ].charID )) {
            strlcpy( w, soldierTranslate[ i ].playerName, 256 );
            break;
        }
    }
    return( w );
}


//  ==============================================================================================
//  apiIsPlayerAliveByName
//
//  Returns 0 if player is dead, otherwise alive.
//  If the server operators has not configured character tracking in Insurgency.log then
//  always return "true."
//
int apiIsPlayerAliveByName( char *playerName )
{
    int isAlive = 1;

    if  ( apiBPIsActive() ) {
        if ( 0 == strlen( apiNameToCharacter( playerName ) ) )  {
            isAlive = 0;
        }
    }
    return( isAlive );
}

//  ==============================================================================================
//  apiIsPlayerAliveByGUID
//
//  Returns 0 if player is dead, otherwise alive.
//  If the server operators has not configured character tracking in Insurgency.log then
//  always return "true."
//
int apiIsPlayerAliveByGUID( char *playerGUID )
{
    int isAlive = 1;
    char *playerName;

    if  ( apiBPIsActive() ) {
        playerName = rosterLookupNameFromGUID( playerGUID );       // returns empty if not found
        if ( 0 != strlen( playerName ) )  {
            if ( 0 == strlen( apiNameToCharacter( playerName )) )  {
                isAlive = 0;
            }
        }
    }
    return( isAlive );
}


//  ==============================================================================================
//  _apiMapObjectiveCB
//
//  Call-back function dispatched when the game system log file indicates a map change.
//  Simply update the name which can be read back from Plugins via the API at later time.
//
int _apiMapObjectiveCB( char *strIn )
{
    int i;
    char *w;

    for (i = 0; i<API_OBJECTIVES_MAX; i++) {

        if ( 0 == strlen( _objectiveListCache[i] )) {

            w = getWord( strIn, 1, "'" );
            if ( w != NULL ) {
                strlcpy( _objectiveListCache[i], w, API_LINE_STRING_MAX );
                logPrintf(LOG_LEVEL_INFO, "api", "Reading map objectives list ::%s:: index %d", _objectiveListCache[i], i );
            }
            break;

        }
    }
    return 0;
}

//  ==============================================================================================
//  apiLookupObjectiveLetterFromCache
//
//  Make an exact match lookup of objective letter ('A', 'B', 'C'...) from label coming from
//  the Insurgency.log file.  This mechanism was implemented because many 3rd party MOD
//  maps do not comply with the objective labeling conventions used by NWI.  This look up uses
//  a pre-game dump that appears on Insurgency.log at start of each map to obtain the sequence #.
//  If exact match is not found, ' ' (space) will be returned.
// 
//  During SISSM hot-restart, this routine may fail to lookup the objective letter until the
//  next map change.
//
char apiLookupObjectiveLetterFromCache( char *objectiveName ) 
{
    char retChar = ' ';
    int i;

    for ( i = 0; i<API_OBJECTIVES_MAX; i++ ) {
       if ( 0 == strcmp( objectiveName, _objectiveListCache[i] )) {
           retChar = i + 'A';
           break;
       }
       if ( 0 == strlen( _objectiveListCache[i] )) {
           break;
       }
    }
    return retChar;
};


//  ==============================================================================================
//  _apiRoundStartCB
//
//  Call-back function dispatched when the round starts
int _apiRoundStartCB( char  *strIn )
{
    int errCode;

    // Update the change in GameState.  This may fail on some slower servers, so the following
    // routine is also called by the periodic CB. 
    // 
    errCode = _apiFetchGameState();
    logPrintf( LOG_LEVEL_INFO, "api", "RoundStart ActiveObjective update::%s:: and Error ::%d::", gameStatePrevious, errCode );

    return 0;
}



//  ==============================================================================================
//  apiInit
//
//  One-time initialization of the API module.  Like the plugins, this module installs several
//  event and alarm handlers for internal use.
//
int apiInit( void )
{
    cfsPtr cP;
    char varImg[256];
    char   myIP[API_LINE_STRING_MAX], myRconPassword[API_LINE_STRING_MAX];
    int    count, i, myPort, badWordsCount, mapsCount;;
    char   serverName[API_LINE_STRING_MAX];

    // Read the "sissm" systems configuration variables
    //
    cP = cfsCreate( sissmGetConfigPath() );

    myPort = (int) cfsFetchNum( cP, "sissm.RconPort", 27015 );            
    strlcpy( myIP, cfsFetchStr( cP, "sissm.RconIP", "127.0.0.1"), API_LINE_STRING_MAX );
    strlcpy( myRconPassword, cfsFetchStr( cP, "sissm.RconPassword", ""), API_LINE_STRING_MAX );
    // strlcpy( adminListFilePath, cfsFetchStr( cP, "sissm.adminListFilePath", "Admins.txt"), API_LINE_STRING_MAX );

    strlcpy( serverName, cfsFetchStr( cP, "sissm.ServerName", "Unknown Server" ), API_LINE_STRING_MAX );

    // read the Bad Words filename
    //
    strlcpy( badWordsFilePath, cfsFetchStr( cP, "sissm.badWordsFilePath", "" ), CFS_FETCH_MAX );

    // read the Mapcycle filepath
    //
    strlcpy( mapcycleFilePath, cfsFetchStr( cP, "sissm.mapcycleFilePath", "" ), CFS_FETCH_MAX );


    // read the AUTH related permission block
    //
    strlcpy( rootguids,       cfsFetchStr( cP, "sissm.rootguids", ""),      API_LINE_STRING_MAX );
    strlcpy( rootname,        cfsFetchStr( cP, "sissm.rootname",  ""),      API_LINE_STRING_MAX );

    strlcpy( everyoneCmds,    cfsFetchStr( cP, "sissm.everyonecmds", "" ),  API_LINE_STRING_MAX );
    strlcpy( everyoneMacros,  cfsFetchStr( cP, "sissm.everyonemcrs", "" ),  API_LINE_STRING_MAX );
    strlcpy( everyoneAttr,    cfsFetchStr( cP, "sissm.everyoneattr", "" ),  API_LINE_STRING_MAX );

    strlcpy( soloAdminCmds,   cfsFetchStr( cP, "sissm.soloadmincmds", "" ), API_LINE_STRING_MAX );
    strlcpy( soloAdminMacros, cfsFetchStr( cP, "sissm.soloadminmcrs", "" ), API_LINE_STRING_MAX );
    strlcpy( soloAdminAttr,   cfsFetchStr( cP, "sissm.soloadminattr", "" ), API_LINE_STRING_MAX );

    for (i=0; i<API_MAX_GROUPS; i++) {
        snprintf( varImg, 256, "sissm.groupname[%d]", i);
            strlcpy( groups[i].groupname, cfsFetchStr( cP, varImg, "" ), API_LINE_STRING_MAX );
        snprintf( varImg, 256, "sissm.groupcmds[%d]", i);
            strlcpy( groups[i].groupcmds, cfsFetchStr( cP, varImg, "" ), API_LINE_STRING_MAX );
        snprintf( varImg, 256, "sissm.groupmcrs[%d]", i);
            strlcpy( groups[i].groupmacros, cfsFetchStr( cP, varImg, "" ), API_LINE_STRING_MAX );
        snprintf( varImg, 256, "sissm.groupattr[%d]", i);
            strlcpy( groups[i].groupattr, cfsFetchStr( cP, varImg, "" ), API_LINE_STRING_MAX );
        snprintf( varImg, 256, "sissm.groupguid[%d]", i);
            strlcpy( groups[i].groupguid, cfsFetchStr( cP, varImg, "" ), API_LINE_STRING_MAX );

        if (( 0 != strlen( groups[i].groupguid ) ) && ( 0 != strlen( groups[i].groupname ) )) {
            count = apiIdListRead( groups[i].groupguid, groups[i].groupIdList );
            logPrintf(LOG_LEVEL_INFO, "api", "Read Group List ::%s:: count %d", groups[i].groupname, count );
        }
    }

    // Used with Circleus' "Advanced Chat" mod
    //
    strlcpy( removeCodes, cfsFetchStr( cP, "sissm.removeCodes", ""), API_LINE_STRING_MAX );
    strlcpy( sayPrefix,   cfsFetchStr( cP, "sissm.sayPrefix",  ""),  API_LINE_STRING_MAX );
    strlcpy( sayPostfix,  cfsFetchStr( cP, "sissm.sayPostfix", ""),  API_LINE_STRING_MAX );

#if FINALCOUNTER_REDUCE
    // Checkpoint/Hardcorecheckpoint Crash Protection Limiter // asdf
    //
    maxBotsCounterAttack = (int) cfsFetchNum( cP, "sissm.MaxBotsCounterAttack", 30.0 ); 
    finalCacheBotQuotaMultiplier = (double) cfsFetchNum( cP, "sissm.FinalCacheBotQuotaMultiplier", 1.2 );
#endif

    //  Close the configuration file reader
    //
    cfsDestroy( cP );

    // Set map to unknown
    //
    rosterSetMapName( "Unknown" );

    // Initialize the RCON TCP/IP interface
    //
    _rPtr = rdrvInit( myIP, myPort, myRconPassword );

    // Setup callbacks for player entering and leaving
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,  _apiPlayerConnectedCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,  _apiPlayerDisconnectedCB );
    eventsRegister( SISSM_EV_MAPCHANGE,   _apiMapChangeCB );
    eventsRegister( SISSM_EV_TRAVEL,      _apiTravelCB );
    eventsRegister( SISSM_EV_SESSIONLOG,  _apiSessionLogCB );
    eventsRegister( SISSM_EV_ROUND_START, _apiRoundStartCB );

    // These callbacks are used for detecting players entering
    // and leaving territorial capture zone (piantirush)
    //
    eventsRegister( SISSM_EV_BP_PLAYER_CONN,    _apiBPCharNameCB );
    eventsRegister( SISSM_EV_BP_PLAYER_DISCONN, _apiBPCharDisconnectCB );
    eventsRegister( SISSM_EV_BP_CHARNAME,       _apiBPCharPlayerCB );
    eventsRegister( SISSM_EV_BP_TOUCHED_OBJ,    _apiBPCharTouchCB );
    eventsRegister( SISSM_EV_BP_UNTOUCHED_OBJ,  _apiBPCharUntouchCB );

    // Event to handle cahching of map objective list 
    //
    eventsRegister( SISSM_EV_MAP_OBJECTIVE, _apiMapObjectiveCB );

    // This hook is mostly used for DEBUG use, non-production
    //
    eventsRegister( SISSM_EV_CHAT,                _apiChatCB );

    // Setup Alarm (periodic callbacks) for fetching roster from RCON
    // 
    _apiPollAlarmPtr = alarmCreate( _apiPollAlarmCB );
    alarmReset( _apiPollAlarmPtr, API_LISTPLAYERS_PERIOD );

    // Clear the Roster module that keeps track of players
    //
    rosterInit();
    rosterSetServerName( serverName );
    lastRosterSuccessTime = apiTimeGet();      
    rosterReset();

    // Read the admin list
    //
    // adminCount = apiIdListRead( adminListFilePath, adminIdList );
    // logPrintf( LOG_LEVEL_CRITICAL, "api", "Admin list %d clients from file %s", adminCount, adminListFilePath );

    // Read the Bad Words list
    //
    badWordsCount = apiWordListRead( badWordsFilePath, badWordsList );
    logPrintf( LOG_LEVEL_CRITICAL, "api", "BadWords list %d words from file %s", badWordsCount, badWordsFilePath );

    // Read the Mapcycle.txt
    //
    mapsCount = apiMapcycleRead( mapcycleFilePath );
    logPrintf( LOG_LEVEL_CRITICAL, "api", "Maplist read %d maps from file %s", mapsCount, mapcycleFilePath);


    // Clear the internal roster & gamestate tracking variables
    //
    strclr( rosterPrevious );
    strclr( rosterCurrent  );
    strclr( gameStateCurrent );
    strclr( gameStatePrevious );

    // Clear the last reboot reason/time 
    //
    lastRebootTime = apiTimeGet();
    strlcpy( lastRebootReason, "Restart", 256 );

    // Clear the BPChar translation table
    //
    _apiBPCharInitCB( "" );

    // on init clear the cache of objectives names
    //
    for (i=0; i<API_OBJECTIVES_MAX; i++) strclr( _objectiveListCache[i]) ;

    return( _rPtr == NULL );
}


//  ==============================================================================================
//  apiDestroy
//
//  Terminate this API module
//
int apiDestroy( void )
{
    rdrvDestroy( _rPtr );
    _rPtr = NULL; 
    return 0;
}

//  ==============================================================================================
//  apiGetLastRebootTime / apiGetLastRebootReason
//  
//  Read back last reboot time & reason
//
unsigned long apiGetLastRebootTime( void )
{  
    return( lastRebootTime );
}
char *apiGetLastRebootReason( void )
{  
    return( lastRebootReason );
}


//  ==============================================================================================
//  apiServeRestart
//
//  Called from a Plugin, this method restarts the game server. 
//  The lastRosterSuccessTime is reset so that the invoked restart will zero out the 'busy'
//  timeout, tracked by pirebooter plugin and other equivalent up-time aware plugin.
//
int apiServerRestart( char *rebootReason )
{
    // Notify and force disconnect on all active players on roster
    //
    apiKickAll( rebootReason );
    sleep( 2 );    // wait for handshake
   
    // Save the reason & time for analytics and web 
    //
    strlcpy( lastRebootReason, rebootReason, 256 );
    lastRebootTime = apiTimeGet();
   
    // invoke system restart
    // 
    sissmServerRestart();
    lastRosterSuccessTime = apiTimeGet();      
    return 0;
}

//  ==============================================================================================
//  apiTimeGet
//
//  Called from a Plugin, this method fetches EPOC time, resolution 1 second.
//
unsigned long int apiTimeGet( void )
{
    return( (unsigned long) time(NULL) );
}

//  ==============================================================================================
//  apiTimeGetHuman
//
//  This method fetches human readable time.  if timeMark is 0L, it will fetch the current
//  time from system call, otherwise timeMark is used for target time.
//
char *apiTimeGetHuman( unsigned long timeMark )
{
    static char humanTime[API_LINE_STRING_MAX];
    time_t current_time;

   
    if ( timeMark == 0L ) 
        current_time = time( NULL );
    else
        current_time = (time_t) timeMark;

    strlcpy( humanTime, ctime( &current_time ), API_LINE_STRING_MAX );

    return( humanTime );
}

//  ==============================================================================================
//  _apiGameModePropertySet
//
//  Called from a Plugin, this method sets gamemodeproperty (cvar) on game server.
//
int _apiGameModePropertySet( char *gameModeProperty, char *value )
{
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE];
    int bytesRead, errCode;

    snprintf( rconCmd, API_T_BUFSIZE, "gamemodeproperty %s %s", gameModeProperty, value );
    errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );

    return( errCode );
}


//  ==============================================================================================
//  apiGameModePropertySet
//
//  Crash Protection wrapper for _apiGameModePropertySet.
//
//  Interim(?) routine that reduces FinalCacheBotQuotaMultiplier so that NWI server will not
//  crash on final objective when MaximumEnemies is set too high.  This routine should be called
//  just prior to each apiGameModePropertySet( "MaximumEnemies" ...) calls.
//  
int apiGameModePropertySet( char *gameModeProperty, char *value )
{
    int errCode = 0;

#if FINALCOUNTER_REDUCE
    double reducedMultiplier;
    char multiplierStr[80];
    int newMaximumEnemy = 0;

    if ( 0 == strncasecmp( gameModeProperty , "maximumenemies", 20 )) {

        if ( 1 != sscanf( value, "%d", &newMaximumEnemy ) ) {
            logPrintf( LOG_LEVEL_CRITICAL, "api", "Internal error parsing args for apiGameModePropertySetCP"  );
            newMaximumEnemy = 4;
        }

        if ( finalCacheBotQuotaMultiplier != 0.0 ) {  // check if disabled

            if (( finalCacheBotQuotaMultiplier * newMaximumEnemy ) > maxBotsCounterAttack ) {
                reducedMultiplier = (double) maxBotsCounterAttack / (double) newMaximumEnemy;
                if ( reducedMultiplier > 2.0 ) reducedMultiplier = 2.0;
                if ( reducedMultiplier < 0.1 ) reducedMultiplier = 0.1;
                snprintf( multiplierStr, 80, "%3.1lf", reducedMultiplier );
                _apiGameModePropertySet( "FinalCacheBotQuotaMultiplier", multiplierStr ) ;
            }
            else {   // if disabled, poke the default value 
                snprintf( multiplierStr, 80, "%3.1lf", finalCacheBotQuotaMultiplier );
                _apiGameModePropertySet( "FinalCacheBotQuotaMultiplier", multiplierStr );
            }

        }
        logPrintf( LOG_LEVEL_INFO, "api", "FinalCacheBotQuotaMultipler changed to %s", multiplierStr  );
    }
#endif

    errCode = _apiGameModePropertySet( gameModeProperty, value );
    return errCode;

}


//  ==============================================================================================
//  apiGameModePropertyGet
//
//  Called from a Plugin, this method reads gamemodeproperty (cvar) from the game server.
//
char *apiGameModePropertyGet( char *gameModeProperty )
{
    static char value[16*1024];
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE], *w;
    int bytesRead, errCode;

    strclr( value );

    snprintf( rconCmd, API_T_BUFSIZE, "gamemodeproperty %s", gameModeProperty );
    if ( 0 == (errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead ))) {
        if ( ((size_t) bytesRead) > strlen( gameModeProperty )) {
            // extract the value field.  This command will fail esp during
            // map change when rcon response is not honored, so check for this!
            w = getWord( rconResp, 1, "\"" );    
            if ( w != NULL ) 
                strlcpy( value, w, sizeof( value ));   
        }
    }

    return( value );
}

//  ==============================================================================================
//  apiSay
//
//  Called from a Plugin, this method sends text to in-game screen of all players, 
//  prefixed by the string "Admin:".  This "say" can be muted via the p2p var 'sayDisable'.
//  For showing critical information, use apiSaySys().
//
int apiSay( const char * format, ... )
{
    static char buffer[API_T_BUFSIZE];
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE];
    int bytesRead, errCode;
    int triplePrint = 0;

    va_list args;
    va_start( args, format ); 
    vsnprintf( buffer, (API_T_BUFSIZE >= API_MAXSAY) ? API_MAXSAY : API_T_BUFSIZE, format, args );

    apiRemoveCodes( buffer );                         // remove color codes if mod is not installed

    if ( snprintf( rconCmd, API_T_BUFSIZE, "say %s%s%s", sayPrefix, buffer, sayPostfix ) < 0) 
        logPrintf( LOG_LEVEL_WARN, "api", "Overflow warning at apiSay" );
    if ( 0 != strlen( buffer ) )  {  // say only when something to be said
        if ( buffer[0] == '*' ) triplePrint = 1;
        if ( 0 == (int) p2pGetF("api.p2p.sayDisable", 0.0 ) ) {
            errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead ); 
            if ( triplePrint ) errCode |= rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead ); 
            if ( triplePrint ) errCode |= rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead ); 
        }
    }
    va_end (args);
    return errCode;
}

//  ==============================================================================================
//  apiSaySys
//
//  Same as apiSay, except this routine is called to carry system critical information
//  that cannot be masked by api.p2p.sayDisable.
//
int apiSaySys( const char * format, ... )
{
    static char buffer[API_T_BUFSIZE];
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE];
    int bytesRead, errCode;
    int triplePrint = 0;

    va_list args;
    va_start( args, format ); 
    vsnprintf( buffer, (API_T_BUFSIZE >= API_MAXSAY) ? API_MAXSAY : API_T_BUFSIZE, format, args );

    apiRemoveCodes( buffer );                         // remove color codes if mod is not installed

    if ( snprintf( rconCmd, API_T_BUFSIZE, "say %s%s%s", sayPrefix, buffer, sayPostfix ) < 0 )
        logPrintf( LOG_LEVEL_WARN, "api", "Overflow warning at apiSaySys" );

    if ( 0 != strlen( buffer ) )  {  // say only when something to be said
        if ( buffer[0] == '*' ) triplePrint = 1;
        errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );
        if ( triplePrint ) errCode |= rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead ); 
        if ( triplePrint ) errCode |= rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead ); 
    }
    va_end (args);
    return errCode;
}
  
//  ==============================================================================================
//  apiKickOrBan
//
//  Called from a Plugin, this method is used to kick or ban a player by SteamGUID64 identifier.
//  Optional *reason string may be attached, or call with empty string if not needed ("").
//  The player must be actively in game or else the command will fail.
//
int apiKickOrBan( int isBan, char *playerGUID, char *reason )
{
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE], playerName[256];
    int bytesRead, errCode;

    if ( isBan ) {
        snprintf( rconCmd, API_T_BUFSIZE, "banid \"%s\" -1 \"%s\"", playerGUID, reason );
        errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );
    }
    else {
        snprintf( rconCmd, API_T_BUFSIZE, "kick \"%s\" \"%s\"", playerGUID, reason );
        errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );

        // redundant 'name' kicked after 'guid' kick -- for EPIC
        //
        strlcpy( playerName, rosterLookupNameFromGUID( playerGUID ), 256 ); 
        if ( 0 != strlen( playerName ) ) {
            snprintf( rconCmd, API_T_BUFSIZE, "kick \"%s\" \"%s\"", playerName, reason );
            errCode |= rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );
        }
    }

    logPrintf( LOG_LEVEL_WARN, "api", "apiKickOrBan ::%s::", rconCmd );
 
    return errCode;
}

//  ==============================================================================================
//  apiKick
//
//  Called from a Plugin, this method is used to kick player by SteamGUID64 or player name.
//  Optional *reason string may be attached, or call with empty string if not needed ("").
//  The player must be actively in game or else the command will fail.
//
int apiKick( char *playerNameOrGUID, char *reason )
{
    char rconCmd[API_T_BUFSIZE], rconResp[API_R_BUFSIZE];
    int bytesRead, errCode;

    snprintf( rconCmd, API_T_BUFSIZE, "kick \"%s\" \"%s\"", playerNameOrGUID, reason );
    errCode = rdrvCommand( _rPtr, 2, rconCmd, rconResp, &bytesRead );
    logPrintf( LOG_LEVEL_WARN, "api", "apiKick ::%s::", rconCmd );
 
    return errCode;
}


//  ==============================================================================================
//  apiKickAll
//
//  Called from a Plugin, this method is used to kick all players from the server.
//  Optional *reason string may be attached, or call with empty string if not needed ("").
//  This function is useful before the server is rebooted by plugin automation or via the
//  admin command.
//
int apiKickAll( char *reason )
{
    static char playerList[8192]; 
    char playerGUID[256], *p;
    int playerIndex = 0;

    strlcpy( playerList, rosterPlayerList( 5, ":" ), 8192 );

    // HAL9000 loop, kill all live humans
    //
    while ( 1 == 1 ) {

        if ( NULL == ( p = getWord( playerList, playerIndex++, ":" ) )) 
            break;

        strlcpy( playerGUID, p, 256 );
        apiKickOrBan(0, playerGUID, reason );
    }

    return 0;
}


//  ==============================================================================================
//  apiRcon
//
//  Called from a Plugin, this provides a Low-level RCON send-receive exchange.
//
int apiRcon( char *commandOut, char *statusIn )
{
    int bytesRead;
    rdrvCommand( _rPtr, 2, commandOut, statusIn, &bytesRead );
    return( bytesRead );
}

//  ==============================================================================================
//  apiPlayersGetCount
//
//  Called from a plugin, this routine returns number players connected in-game.  
//  This count is derived from internally maintained roster list, which is updated 
//  periodically (time), or whenever an ADD or DEL client event is generated by 
//  monitoring the system game log file.
//  
//  In the event of RCON interface error, this routine returns the last known, last successful
//  roster count value.
//
int apiPlayersGetCount( void )
{
#if (SISSM_TEST == 0)
      return ( rosterCount() );  
#else
    if ( _overrideAliveCount ) return( _overrideAliveCount );
    else return ( rosterCount() );
#endif
}

//  ==============================================================================================
//  apiPlayersRoster
//
//  Called from a plugin, this routine returns a single contatenated string of all connected 
//  players information such as name, GUID, IP, and score.  For different output format options
//  see rosterPlayerList() in roster.c.  You may specify your custom delimiter string between
//  individual player data.
//
char *apiPlayersRoster( int infoDepth, char *delimiter )
{
    return ( rosterPlayerList(infoDepth, delimiter) );
}


//  ==============================================================================================
//  apiGetServerName
//
//  Called from a plugin, this routine returns the server name specified in the sissm.log file
//  (it is not read from the game server!)
//
char *apiGetServerName( void )
{
    return ( rosterGetServerName() );
}


//  ==============================================================================================
//  apiGetMapName
//
//  Called from a plugin, this routine returns the current map name.  Currently, the map name
//  returned here is not valid until after the first map change as executed since the restart.
//
char *apiGetMapName( void )
{
    return ( rosterGetMapName() );
}

//  ==============================================================================================
//  apiGetSessionID
//
//  Called from a plugin, this routine returns the current map name.  Currently, the map name
//  returned here is not valid until after the first map change as executed since the restart.
//
char *apiGetSessionID( void )
{
    return ( rosterGetSessionID() );
}

//  ==============================================================================================
//  apiAuthIsAllowedCommand
//
//  Checks privilegte if command (e.g., picladmin) can be executed by an admin via the guid.
//
//
int apiAuthIsAllowedCommand( char *playerGUID, char *command )
{
    int i, isAuthorized = 0;

    // check if playerGUID is 'root'
    //
    if ( NULL != strstr( rootguids, playerGUID )) {
        isAuthorized = 1;
    }
    
    // check if command is from 'RCON' if so grant 'root' priv
    //
    else if ( 0 == strcmp( "00000000000000000", playerGUID ) ) {
        isAuthorized = 1;
    }

    // check if command is for 'everyone'
    //
    else if ( NULL != strstr( everyoneCmds, command ))  {
        isAuthorized = 1;
    }

    // check the priv tables to see if guid-command pair is authorized
    //
    else {
        for (i=0; i<API_MAX_GROUPS; i++) {
            if ( NULL != strstr( groups[i].groupcmds, command )) {
                if ( apiIdListCheck( playerGUID, groups[i].groupIdList ) ) {
                    isAuthorized = 1;
                    break;
                }
            }
        }
    }

    // check if a player is solo, if so grant special privilege "soloadmin"
    //
    if ( !isAuthorized ) {
        if ( NULL != strstr( soloAdminCmds, command )) {
            if ( 1 == apiPlayersGetCount() ) {
                isAuthorized = 1;
            }
        }
    }
 
    return( isAuthorized );
}

//  ==============================================================================================
//  apiAuthIsAllowedMacro
//
//  Checks privilegte if macro can be executed by an admin via the guid.
//
//
int apiAuthIsAllowedMacro( char *playerGUID, char *command )
{
    int i, isAuthorized = 0;

    // check if playerGUID is 'root'
    //
    if ( NULL != strstr( rootguids, playerGUID )) {
        isAuthorized = 1;
    }

    // check if command is for 'RCON'
    //
    else if ( 0 == strcmp( "00000000000000000", playerGUID ) ) {
        isAuthorized = 1;
    }

    // check if command is for 'everyone'
    //
    else if ( NULL != strstr( everyoneMacros, command ))  {
        isAuthorized = 1;
    }

    // check the priv tables to see if guid-command pair is authorized
    //
    else {
        for (i=0; i<API_MAX_GROUPS; i++) {
            if ( NULL != strstr( groups[i].groupmacros, command )) {
                if ( apiIdListCheck( playerGUID, groups[i].groupIdList ) ) {
                    isAuthorized = 1;
                    break;
                }
            }
        }
    }

    // check if a player is solo, if so grant special privilege "soloadmin"
    //
    if ( !isAuthorized ) {
        if ( NULL != strstr( soloAdminMacros, command )) {
            if ( 1 == apiPlayersGetCount() ) {
                isAuthorized = 1;
            }
        }
    }
    return( isAuthorized );
}


//  ==============================================================================================
//  apiAuthIsAttribute
//
//  Checks if specific privilegte is granted to an admin via the guid.
//
int apiAuthIsAttribute( char *playerGUID, char *attribute )
{
    int i, isAuthorized = 0;

    // check if playerGUID is 'root'
    //
    if ( NULL != strstr( rootguids, playerGUID )) {
        isAuthorized = 1;
    }
    else {
        for (i=0; i<API_MAX_GROUPS; i++) {
            if ( NULL != strstr( groups[i].groupattr, attribute)) {
                if ( apiIdListCheck( playerGUID, groups[i].groupIdList ) ) {
                    isAuthorized = 1;
                    break;
                }
            }
        }
    }
    // check if a player is solo, if so grant special privilege "soloadmin"
    //
    if ( !isAuthorized ) {
        if ( NULL != strstr( soloAdminAttr, attribute )) {
            if ( 1 == apiPlayersGetCount() ) {
                isAuthorized = 1;
            }
        }
    }
    return( isAuthorized );
}


//  ==============================================================================================
//  apiAuthGetRank
//
//  Checks if specific privilegte is granted to an admin via the guid.
//
char *apiAuthGetRank( char *playerGUID )
{
    static char rank[ 256 ];
    int i;

    // check if playerGUID is 'root'
    //
    strclr( rank );
    if ( NULL != strstr( rootguids, playerGUID )) {
        strlcpy( rank, rootname, 256 );
    }
    else {
        for (i=0; i<API_MAX_GROUPS; i++) {
            if ( apiIdListCheck( playerGUID, groups[i].groupIdList ) ) {
                strlcpy( rank, groups[i].groupname, 256 );
                break;
            }
        }
    }
    return( rank );
}


//  ==============================================================================================
//  apiIsAdmin
//
//  Returns 1 if the GUID is in a group with 'admin' attribute, 0 if not 
//  
//  
int apiIsAdmin( char *connectID )
{
    return ( apiAuthIsAttribute( connectID, "admin" ));
}


//  ==============================================================================================
//  apiGetServerMode
//
//  Returns current game mode 
//  
//  Example content of gameStatePrevious:
//  "ObjectiveCapturable'/Game/Maps/Tell/Tell_Checkpoint_Security.Tell_Checkpoint_Security:PersistentLevel.OCCheckpoint_A'" 
//
char *apiGetServerMode( void )
{
    static char gameMode[256];
    int i;
    static char recognizedModes[16][64] = { 
        "hardcore",   "checkpoint", "frenzy",    "outpost",    "frontline", 
        "domination", "push",       "firefight", "deathmatch", "*" 
    };

    strlcpy( gameMode, "unknown", 256 );

    for ( i=0 ; ; i++ ) {
        if ( recognizedModes[i][0] == '*' ) break;
        if ( NULL != strcasestr( gameStatePrevious, recognizedModes[i] ) ) {
            strlcpy( gameMode, recognizedModes[i], 256 );
            break;
        }
    }
    return( gameMode );
}


//  ==============================================================================================
//  apiGetServernameRCON
//
//  Returns a hostname.  This function always returns a pointer to a valid string (no need for
//  NULL checks from the caller).  If successful, the server hostname is returned.  On interface
//  failure, string "UNKNOWN" is returned.
//  
//  
char *apiGetServerNameRCON( int forceCacheRefresh )
{
    static char hostName[256];
    static unsigned long timeLastRead = 0;

    // check to see if this is a first-time call, or caller is forcing a refrehs
    //
    if ( (timeLastRead == 0L) || forceCacheRefresh ) {
    
        // fetch from RCON.  apiGameModePropertyGet returns an empty string
        // (not NULL) on interface error.
        //
        strlcpy( hostName, apiGameModePropertyGet( "serverhostname" ), 256 ); 
        if ( strlen( hostName ) != 0 )  {
            timeLastRead = apiTimeGet();
            logPrintf( LOG_LEVEL_INFO, "api", "RCON successful read of hostname ::%s::", hostName );
        }
        else 
            strlcpy( hostName, "Unknown", 256 );
    }
    return ( hostName );
}


//  ==============================================================================================
//  apiMapcycleRead
//
//  Reads system Mapcycle.txt - it must be formatted such that:
//  1) metatag #SISSM mapname=<mapname> is included
//  2) full scenario format is used
//
int apiMapcycleRead( char *mapcycleFile )
{
    FILE *fpr;
    int   i, j;
    char  tmpLine[256], lastMapName[256], lastReqName[256];
    char  *w, *w2, *v, *u, scenarioName[256];

    strclr( lastMapName );
    for (i = 0; i<API_MAXMAPS; i++) {
        strclr( mapCycleList[i].mapName );
        strclr( mapCycleList[i].reqName );
        strclr( mapCycleList[i].scenario );
        strclr( mapCycleList[i].traveler );
        strclr( mapCycleList[i].gameMode );
        mapCycleList[i].dayNight =  0;
        mapCycleList[i].secIns   = -1;

        strclr( mapNameMerged[i] );
    }

    // mutator meta:  #SISSM.allowmutator, and #SISSM.defaultmutator
    //
    strclr( mutAllowed );
    strclr( mutDefault );

    i = 0;
    if ( NULL != (fpr = fopen( mapcycleFile, "rt" ))) {
        while ( !feof( fpr ) ) {
            if (NULL != fgets( tmpLine, 255, fpr )) {

                // part 1:  parse the "#SISSM.mapname" line
                //
                // printf("\nDEBUG - reading %s", tmpLine );
                if ( tmpLine == strcasestr( tmpLine, "#SISSM.mapname" )) {
                    if ( NULL != (w = getWord( tmpLine, 1, "= ,;:\015\012" ))) {
                        strlcpy( lastMapName, w, 256 );
                        strlcpy( lastReqName, w, 256 );
                        if ( NULL != (w2 = getWord( tmpLine, 2, "= ,;:\015\012" ))) {
                            if (0 != strlen( w2 )) strlcpy( lastReqName, w2, 256 );
                        }
                        // printf("\nDEBUG - reading map name ::%s::", lastMapName );

                        // create a merged list for "maplist" command
                        //
                        for ( j = 0; j<API_MAXMAPS; j++ ) {
                            if ( 0 == strcasecmp( mapNameMerged[j], lastReqName ) ) 
                                break;
                            if ( 0 == strlen( mapNameMerged[j] ) ) {
                                strlcpy( mapNameMerged[j], lastReqName, 40 ); 
                                break;
                            }
                        }
                    }
                }

                // part 2:  parse the scenario name and the game mode (checkpoint, hardcorecheckpoint, etc.)
                //
                else if ( tmpLine == strcasestr( tmpLine, "(Scenario=\"" )) {

                    strlcpy( scenarioName,  getWord( tmpLine, 1, "=\"" ), 255 );

                    if ( (0 != strlen( scenarioName )) && (0 != strlen( lastMapName )) )  {
                        strlcpy( mapCycleList[i].scenario, scenarioName, 256 );
                        strlcpy( mapCycleList[i].mapName,  lastMapName,  256 );
                        strlcpy( mapCycleList[i].reqName,  lastReqName,  256 );
                        strlcpy( mapCycleList[i].traveler, lastMapName,  256 );
                        strncat( mapCycleList[i].traveler, "?Scenario=", 256 );
                        strncat( mapCycleList[i].traveler, scenarioName, 256 );

                        // parse the gamemode
                        //
                        if ( NULL != ( v = strcasestr( tmpLine, "mode=\"" )  )) {
                            if ( NULL != (u = getWord( v, 1, "\"" )) ) {
                                strncat( mapCycleList[i].traveler, "?Game=", 256 );
                                strncat( mapCycleList[i].traveler, u,        256 );
                                strlcpy( mapCycleList[i].gameMode, u,         40 );
                            }
                        }

                        // parse the day/night
                        //
                        if ( NULL != strcasestr( tmpLine, "lighting=\"night\"" )) { 
                            mapCycleList[i].dayNight = 1;
                            strncat( mapCycleList[i].traveler, "?Lighting=Night", 256 );
                        }
                        else {
                            strncat( mapCycleList[i].traveler, "?Lighting=Day", 256 );
                        }

                        mapCycleList[i].secIns = -1;
                        if ( NULL != strcasestr( scenarioName, "insurgent"  )) 
                            mapCycleList[i].secIns = 1;
                        if ( NULL != strcasestr( scenarioName, "security"   )) 
                            mapCycleList[i].secIns = 0;
 
                        logPrintf( LOG_LEVEL_INFO, "api", "Mapcycle %03d tag %s traveler ::Sec/Ins:%1d Day/Night:%1d ::%s:: GameMode ::%s::", 
                            i,
                            mapCycleList[i].reqName,  mapCycleList[i].secIns,  mapCycleList[i].dayNight, 
                            mapCycleList[i].traveler, mapCycleList[i].gameMode );

                        if ( ++i >= API_MAXMAPS ) break; 

                    }
                }
                else if ( tmpLine == strcasestr( tmpLine, "#SISSM.allowmutator" )) {
                    j = 1;
                    while ( 1 == 1 ) {
                        u = getWord( tmpLine, j++, " ,=:\012\015" );
                        if ( u == NULL )        break;
                        if ( 0 == strlen( u ) ) break;
                        strlcat( mutAllowed, u,   16000 );
                        strlcat( mutAllowed, " ", 16000 );
                    } 
                }
                else if ( tmpLine == strcasestr( tmpLine, "#SISSM.defaultmutator" )) {
                    j = 1;
                    while ( 1 == 1 ) {
                        u = getWord( tmpLine, j++, " ,=:\012\015" );
                        if ( u == NULL )        break;
                        if ( 0 == strlen( u ) ) break;
                        strlcat( mutDefault, u,   256 );
                        strlcat( mutDefault, " ", 256 );
                    } 
                }
            }
        }
    }
 
// asdf
    // Expand the space-separated single string mutAllowed to list mutAllowedList
    //
    strclr( mutAllowedList[0] );
    for (j = 0; j<API_MUT_MAXLIST; i++ ) { 
        w = getWord( mutAllowed, j, " ," );
        if ( w != NULL ) 
            strlcpy( mutAllowedList[j], w, API_MUT_MAXCHAR );
        else {
            strclr( mutAllowedList[j] );
            break;
        }              
    }

#if 0
    if ( i != 0 ) {
        printf("\nMaps ------------------------\n");
        for ( j = 0; j<API_MAXMAPS; j++ ) {
            if ( 0 == strlen( mapNameMerged[j] ) ) break;
            printf("::%s:: ",  mapNameMerged[j] );
        }
        printf("\nMutators ------------------------");
        printf("\nMutAllowed ::%s::",    mutAllowed );
        printf("\nMutDefault ::%s::\n",  mutDefault );
    }
#endif

    return( i );
}

//  ==============================================================================================
//  apiMapList
//
//  Return a space separated list of available maps 
//  Information is derivedd from meta-tags in Mapcycle
//
char *apiMapList( void )
{
    int j;
    static char mapList[4096];

    strclr( mapList );
    for ( j = 0; j<API_MAXMAPS; j++ ) {
        if ( 0 == strlen( mapNameMerged[j] ) ) break;
        strncat( mapList, mapNameMerged[j], 4095 );
        strncat( mapList, " ",              4095 );
    }
    return( mapList );
}


//  ==============================================================================================
//  apiIsSupportedGameMode
//
//  Look up if candidateMode is a supported gamemode as listed in Mapcycle.txt
//
int apiIsSupportedGameMode( char *candidateMode  )
{
    int isGameMode = 0;
    int j;

    for ( j = 0; j<API_MAXMAPS; j++ ) {
        if ( 0 == strcasecmp( mapCycleList[j].gameMode, candidateMode ) ) {
            isGameMode = 1;
            break;
        }
    }
    return( isGameMode );
}

//  ==============================================================================================
//  _apiIsSupportedMutator
//
//  Look up if mutator is supported, and if so, return the full valid name from 
//  a partial match.  This function always returns a pointer to a valid string.
//  No-match will simply return empty (zero length) string.
//
#if MUTATOR_SELECT
static char *_apiIsSupportedMutator( char *candidateMutator )
{
// asdf
    static char fullMutatorName[API_MUT_MAXCHAR];

// mutAllowed


    strclr( fullMutatorName );

    return fullMutatorName;
}
#endif


//  ==============================================================================================
//  apiMapChange
//
//  Look up valid traveler and change map
//
int apiMapChange( char *mapName, char *gameMode, int secIns, int dayNight, char *mutatorsList )
{
    int i, errCode = 1;
    char strOut[300], strResp[300];

    for (i = 0; i<API_MAXMAPS; i++ ) {

        if ( 0 == strlen( mapCycleList[i].reqName ))  
            break;

        // For map change to execute, the mapCycleList[].travel element must match input parameters 
        // on 1) day-night, 2) ins-security, and 3) either gamemode must be 'blank' or must match what is specified
        //
        if ( NULL != strcasestr( mapCycleList[i].reqName, mapName ) ) {
            if ( mapCycleList[i].dayNight == dayNight ) {
                if (( mapCycleList[i].secIns == -1 ) || ( mapCycleList[i].secIns == secIns )) {
                    if ( ( 0 == strlen( gameMode )) || ( 0 == strcasecmp( mapCycleList[i].gameMode, gameMode )) ) {
                        snprintf( strOut, 300, "travel %s", mapCycleList[i].traveler );

  // asdf
  // remove options=

                        if ( 0 != strlen( mutatorsList )) {
 

// asdf loop through allowed mutators for 
// correct and complete name substitution


                            strlcat( strOut, "?mutators=", 300 );
                            strlcat( strOut, mutatorsList, 300 );
                        }
                        logPrintf( LOG_LEVEL_INFO, "api", "*** Admin command map change ::%s::", strOut );
                        apiRcon( strOut, strResp );
                        errCode = 0;
                        break;
                    }
                } 
            }
        } 
    }

    if ( errCode ) {
        logPrintf( LOG_LEVEL_INFO, "api", 
            "Admin issued invalid map change ::%s::%d::%d", 
            mapName, secIns, dayNight );
    }
                        
    return( errCode );
}


//  ==============================================================================================
//  apiMutList
//
//  Return a space-separated list of available mutators
//  Information is derivedd from meta-tags in Mapcycle
//
char *apiMutList( void )
{
    return( mutAllowed );
}


#if 0

//  ==============================================================================================
//  apiMutList
//
//  Return a space-separated list of available mutators
//  Information is derivedd from meta-tags in Mapcycle
//
char *apiMutQueue( char *mutNext )
{
    if ( NULL == mutNext ) { 
        strlcpy( mutQueue, "none" , 256 );
    }
    else if ( 0 == strlen( mutNext ) ) { 
        strlcpy( mutQueue, "none", 256 ) ;
    }
    else {
        j = 0;
        while ( 1 == 1 ) {
            strlcpy( mutQueue, getWord( mutNext, j++, " ," ), 256 ); 
            if ( 0 == strlen( w = getWord( mutNext, j++, " ," , 256 ))) { break; }
            strncat( mutQueue, "," );
            strncat( mutQueue, " " );

        

    }
}

#endif

