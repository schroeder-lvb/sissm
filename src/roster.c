//  ==============================================================================================
//
//  Module: ROSTER
//
//  Description:
//  Game state extraction, player information parsing
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================
//

#define _GNU_SOURCE    // strcasestr

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bsd.h"
#include "log.h"
#include "util.h"
#include "roster.h"
#include "api.h"

#include "winport.h"   // strcasestr

static rconRoster_t masterRoster[ROSTER_MAX];
static char rosterServerName[256], rosterMapName[256], rosterTravel[256], rosterSessionID[256];
static char rosterObjective[256], rosterObjectiveType[256];

//  ==============================================================================================
//  rosterIsValidGUID
//
//  Returns non-zero if string is a valid 17-character SteamGUID
//
int rosterIsValidGUID( char *testGUID )
{
    int i, isValid = 1;

    if (17 != strlen( testGUID )) {
        isValid = 0;
    }
    else {
        for (i=0; i<17; i++) {
            if ( !isdigit( testGUID[i] )) {
                isValid = 0;
                break;
            }
        }
    }
    return( isValid );
}

//  ==============================================================================================
//  rosterSetSessionID
//
//  Set current map name
//
void rosterSetSessionID( char *sessionInfo )
{
    strlcpy( rosterSessionID, sessionInfo, 256 );     
    return;
}

//  ==============================================================================================
//  rosterGetSessionID
//
//  Get current map name
//
char *rosterGetSessionID( void )
{
    return( rosterSessionID );
}

//  ==============================================================================================
//  rosterSetObjective
//
//  Set current (active) objective and type (COOP only)
//
//
void rosterSetObjective( char *objectiveName, char *objectiveType )
{
    strlcpy( rosterObjective,     objectiveName, 256 );     
    strlcpy( rosterObjectiveType, objectiveType, 256 );     
    return;
}

//  ==============================================================================================
//  rosterGetObjective
//
//  Get current (active) objective (COOP only)
//
//  Returned values:
//    Obj_WeaponCache_Ins_C     Security destroying Insurgent cache
//    Obj_WeaponCache_Sec_C     Insurgent destroying Security cache
//    ObjectiveCapturable       Capture Objective
//
char *rosterGetObjective( void )
{
    return( rosterObjective );
}

//  ==============================================================================================
//  rosterGetObjectiveType
//
//  Get current (active) objective (COOP only)
//
char *rosterGetObjectiveType( void )
{
    return( rosterObjectiveType );
}

//  ==============================================================================================
//  rosterSetMapName
//
//  Set current map name
//
void rosterSetMapName( char *mapName )
{
    strlcpy( rosterMapName, mapName, 256 );     
    return;
}

//  ==============================================================================================
//  rosterGetMapName
//
//  Get current map name
//
char *rosterGetMapName( void )
{
    return( rosterMapName );
}

//  ==============================================================================================
//  rosterSetTravel
//
//  Set current map name
//
void rosterSetTravel( char *travelName )
{
    strlcpy( rosterTravel, travelName, 256 );     
    return;
}

//  ==============================================================================================
//  rosterGetCoopSide
//
//  Returns the human team side for PvE coop game (-1=unknown or PvP, 0=Security, 1=Insurgents)
//
int rosterGetCoopSide( void )
{
    int retVal = -1;

    if ( NULL != strstr( rosterTravel, "Checkpoint_Security" ))   retVal = 0;
    if ( NULL != strstr( rosterTravel, "Checkpoint_Insurgents" )) retVal = 1;

    return( retVal );
}

//  ==============================================================================================
//  rosterGetCoopObjectiveLetter
//
//  Returns the active (next) objective, i.e., 'A', 'B', 'C'..., or ' ' if error
//
char rosterGetCoopObjectiveLetter( void )
{
    char retVal = ' ';
    char *w = NULL;

    strClean( rosterObjective );
    if ( NULL == (w = strstr( rosterObjective, "ODCheckpoint_" ) ) ) {
        if ( NULL == (w = strstr( rosterObjective, "OCCheckpoint_" ) ) )  {
            retVal = ' ';
        }
    }

    if ( w != NULL ) {
        retVal = w[ 13 ];  // extract 'A' from "OxCheckpoint_A_0"
    }

    // Third party maps do not necessarily follow the NWI objective convension.
    // This alternate lookup method uses dummp of objective name after map change
    // found in Insurgency.log file.
    //
    if ( retVal == ' ' ) {
        retVal = apiLookupObjectiveLetterFromCache( rosterObjective );
        logPrintf( LOG_LEVEL_INFO, "roster", "Using objective list cache and found objective letter ::%c:: from ::%s::", retVal, rosterObjective );
    }
    else {
        logPrintf( LOG_LEVEL_INFO, "roster", "Using parser and found objective letter ::%c::", retVal );
    }

    // At start of the server, the data from activeobjective seems unreliable.
    // This is a kluge to assume unreadable objective is 'A'
    //
    if ( retVal == ' ' ) {
        retVal = 'A';
        logPrintf( LOG_LEVEL_INFO, "roster", "Unnown Objective letter - Assuming ::%c::", retVal );
    }

    return( retVal );
}

//  ==============================================================================================
//  rosterGetScenario
//
//  Return the scenario traveler string
// 
//  Reference:
//  [2019.10.04-01.40.21:979][973]LogGameMode: ProcessServerTravel: Ministry?Scenario=Scenario_Ministry_Checkpoint_Security?Mutators=PistolsOnly
//
char *rosterGetScenario( void )
{
    char *pivot;
    static char nullString[2] = {0, 0};

    if (NULL == ( pivot = strstr( rosterTravel, "ProcessServerTravel: " ))) 
        pivot = nullString;
    else  
        pivot += strlen( "ProcesServerTravel: " );
    return( pivot );
}

//  ==============================================================================================
//  rosterGetMutator
//
//  Return the mutator stack
//
char *rosterGetMutator( void )
{
    char *pivot;
    static char nullString[2] = {0, 0};

    if (NULL == ( pivot = strstr( rosterTravel, "?Mutators=" ))) 
        pivot = nullString;
    else  
        pivot += strlen( "?Mutators=" );
    return( pivot );
}

//  ==============================================================================================
//  rosterSetServerName
//
//  set server name (cosmetic)
//
void rosterSetServerName( char *serverName )
{
    strlcpy( rosterServerName, serverName, 256 );     
    return;
}

//  ==============================================================================================
//  rosterGetServerName
//
//  get server name (cosmetic)
//
char *rosterGetServerName( void )
{
    return( rosterServerName );;
}

//  ==============================================================================================
//  _copy3 (internal)
//
//  Special strcpy variant that checks for " | " prefix and bypass if present.  This is 
//  a shortcut method for parsing player roster obtained from RCON listplayer command.
//
static int _copy3( char *dst, char *src, int maxChars )
{
    int validFlag = 0;

    if ((src != NULL) && (dst != NULL)) {
        validFlag = 1;
        if      (0 == strncmp( src, " | ", 3)) strlcpy( dst, &src[3], maxChars );
        else if (0 == strncmp( src, "| ",  2)) strlcpy( dst, &src[2], maxChars );
        else if (0 == strncmp( src, " |",  2)) strlcpy( dst, &src[2], maxChars );
        else                                   strlcpy( dst,  src,    maxChars );  
    }
    return( validFlag );
}


//  ==============================================================================================
//  rosterReset
//
//  Reset and clear only the player fields of the roster database
//
void rosterReset( void )
{
    int i;

    for (i=0; i<ROSTER_MAX; i++) {
        strclr( masterRoster[i].netID );
        strclr( masterRoster[i].playerName );
        strclr( masterRoster[i].steamID );
        strclr( masterRoster[i].IPaddress );
        strclr( masterRoster[i].score );
    }
}

//  ==============================================================================================
//  rosterInit
//
//  Initialize the entire roster database 
//
void rosterInit( void )
{
    strclr( rosterServerName    );
    strclr( rosterMapName       );
    strclr( rosterSessionID     );
    strclr( rosterObjective     );
    strclr( rosterObjectiveType );
    rosterReset();
    return;
}


//  ==============================================================================================
//  tabDumpDebug (internal, test only)
//
//  Development tool for parsing RCON roster list with tab as a delimiter.  It is left here
//  because it is handy for maintaining parser code, debugging.  THe argument 'buf' is the
//  exact image of what is read from RCON after listplayers command is issued.  The argument
//  'n' is the bytecount returned from the TCP/IP read procedure.
//
int tabDumpDebug( unsigned char *buf, int n )
{
    int i;
    char *headStr, *recdStr;

    headStr = strstr( (char * ) &buf[32], "========================" );         // start separator
    recdStr = &headStr[80];                                      // look for start of first record

    i = 0;
    while ( 1 == 1 ) {
        printf("::%s::\n", getWord( recdStr, i++, "\011"));
        if (i > 500) break;
    }
    return 0;
}


//  ==============================================================================================
//  rosterParse
//
//  Parser routine called as to process listplayer RCON response.  It updates/builds the
//  internal current players database.  "buf" is the raw RCON response data and "n" is 
//  bytecount returned by TCP/IP read buffer length.
//
//  Case 1:  valid record with one or more players -> update DB with players
//  Case 2:  valid record with zero players -> update DB with zero player
//  Case 3:  invalid record -> don't update DB (assume Interface Error)
//
int rosterParse( char *buf, int n )
{
    int i, j, validFlag;
    char *headStr, *recdStr, *atLeastOne;
    char tmpStr[64], *w;

    validFlag = 1;  i = 0;  j = -1;
    
    headStr = strstr( &buf[32], "========================" );    // look for start of separator == valid input
    if ( headStr != NULL ) {
        recdStr = &headStr[80];                                  // look for start of first record
        atLeastOne = strstr( recdStr, "7656" );                  // must have at least one person

        j = 0;
        rosterReset();

        if ( NULL != atLeastOne ) {
            while ( 1 == 1 ) {

                // parse through data fields that are tab-delimited
                // _copy3 routine eliminates some field artifacts and checks if getWord return a NULL.
                //
                if (validFlag) validFlag = _copy3( masterRoster[j].netID,      getWord( recdStr, i++, "\011"), ROSTER_FIELD_MAX); 
                if (validFlag) validFlag = _copy3( masterRoster[j].playerName, getWord( recdStr, i++, "\011"), ROSTER_FIELD_MAX); 
                if (validFlag) validFlag = _copy3( masterRoster[j].steamID,    getWord( recdStr, i++, "\011"), ROSTER_FIELD_MAX); 
                if (validFlag) validFlag = _copy3( masterRoster[j].IPaddress,  getWord( recdStr, i++, "\011"), ROSTER_FIELD_MAX); 
                if (validFlag) validFlag = _copy3( masterRoster[j].score,      getWord( recdStr, i++, "\011"), ROSTER_FIELD_MAX); 

                // getWord returning a NULL indicates end of buffer, resulting in validFlag being cleared.. Exit the loop.
                //
                if (!validFlag) break;

                // Process Sandstorm 1.11 format change 
                // 
                if ( NULL != ( w = strstr( masterRoster[j].steamID, "7656" )) ) {
                    strlcpy( tmpStr, w,  32 );
                    strlcpy( masterRoster[j].steamID, tmpStr, 32 );
                }

                // This block was added as safety in case the data is corrupted.  
                // It has strict checking for valid Steam GUID before the data is saved.
                //
                if ( rosterIsValidGUID( masterRoster[j].steamID )) {   // safety stop
                    if ( (++j) >= ROSTER_MAX) break;
                }

            }
        }
        else {
            logPrintf( LOG_LEVEL_RAWDUMP, "roster", "Received empty or malformed listplayer rcon response size %d ::%s::", n, recdStr );
        }
        strclr( masterRoster[j].netID );
        strclr( masterRoster[j].playerName );
        strclr( masterRoster[j].steamID );
        strclr( masterRoster[j].IPaddress );
        strclr( masterRoster[j].score );

    }
    else {
        logPrintf( LOG_LEVEL_WARN, "roster", "Received non-divider listplayer rcon response size %d", n );
    }

    return( j ); 
}

//  ==============================================================================================
//  rosterCount
//
//  Counts and returns number of active human players in the current database snapshot.
//
int rosterCount( void )
{
#if SISSM_TEST
    extern int _overrideAliveCount;  // from API
#endif
    int i, count = 0;

    for (i=0; i<ROSTER_MAX; i++) {
        if ( strlen( masterRoster[i].netID ) ) {
            if (( rosterIsValidGUID(  masterRoster[i].steamID)) && ( 0 != strlen( masterRoster[i].IPaddress )) )  { 
                count++;
            }
        }
    }
#if SISSM_TEST
    if ( _overrideAliveCount ) count = _overrideAliveCount;
#endif
    return count;
}

//  ==============================================================================================
//  rosterLookupNameFromIP
//
//  Uses the database to translate player IP# and returns player name string.  Empty string
//  (not NULL) is returned if data is not found.
//
char *rosterLookupNameFromIP( char *playerIP )
{
    static char playerName[256];
    int i;
    strclr( playerName );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( 0 == strcmp( masterRoster[i].IPaddress, playerIP )) {
            strlcpy( playerName, masterRoster[i].playerName, 256 ); 
            break;
        }
    }
    return( playerName );
}


//  ==============================================================================================
//  rosterLookupNameFromGUID
//
//  Uses the database to translate player GUID to player name string.  Empty string
//  (not NULL) is returned if data is not found.
//
char *rosterLookupNameFromGUID( char *playerGUID )
{
    static char playerName[256];
    int i;
    strclr( playerName );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( 0 == strcmp( masterRoster[i].steamID, playerGUID )) {
            strlcpy( playerName, masterRoster[i].playerName, 256 );
            break;
        }
    }
    return( playerName );
}


//  ==============================================================================================
//  rosterLookupIPFromGUID
//
//  Uses the database to lookup player IP# from GUID.  Empty string
//  (not NULL) is returned if data is not found.
//
char *rosterLookupIPFromGUID( char *playerGUID )
{
    static char playerIP[256];
    int i;
    strclr( playerIP );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( 0 == strcmp( masterRoster[i].steamID, playerGUID )) {
            strlcpy( playerIP, masterRoster[i].IPaddress, 256 );
            break;
        }
    }
    return( playerIP );
}


//  ==============================================================================================
//  rosterLookupSteamIDFromName
//
//  Uses the database to translate player name to SteamID.  Empty string (not NULL)
//  is returned if data is not found.
//
char *rosterLookupSteamIDFromName( char *playerName )
{
    static char steamID[256];
    int i;

    strclr( steamID );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( 0 == strcmp( masterRoster[i].playerName, playerName )) {
            strlcpy( steamID, masterRoster[i].steamID, 256 ); 
            break;
        }
    }
    return( steamID );
}

//  ==============================================================================================
//  rosterLookupSteamIDFromPartialName
//
//  Uses the database to translate player partial name to SteamID.  Empty string (not NULL)
//  is returned if data is not found OR target cannot be uniquely identified.
//
char *rosterLookupSteamIDFromPartialName( char *partialName, int minChars  )
{
    static char steamID[256];
    int i, matchCount = 0;

    if ( (int) strlen( partialName ) >= minChars ) {
        strclr( steamID );
        for (i=0; i<ROSTER_MAX; i++) {
            if ( NULL != strcasestr( masterRoster[i].playerName, partialName )) {
                strlcpy( steamID, masterRoster[i].steamID, 256 ); 
                matchCount++;
                break;
            }
        }
    }
    if ( matchCount != 1 ) strclr( steamID );

    return( steamID );
}


//  ==============================================================================================
//  rosterLookupIPFromName
//
//  Uses the database to translate player name to IP#.  Empty string (not NULL)
//  is returned if data is not found.
//
char *rosterLookupIPFromName( char *playerName )
{
    static char playerIP[256];
    int i;
    strclr( playerIP );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( 0 == strcmp( masterRoster[i].playerName, playerName )) {
            strlcpy( playerIP, masterRoster[i].IPaddress, 256 ); 
            break;
        }
    }
    return( playerIP );
}



//  ==============================================================================================
//  rosterPlayerList
//
//  This routine returns a formatted player roster dump in one of several options.  This routine
//  is used for many plugins for player lookup, reporting, web generation, etc.
//
//
//  infoDepth:  0=name 1=name+guid, 2=name+guid+ip, 3=name+score, 4=machine-formatted-easy-parse fmt
//  delimiter:  string you want between the names, e.g., " ", " : ",  "," , or even a tab character.
//
char *rosterPlayerList( int infoDepth, char *delimiter )
{
    static char playerList[4096], single[256];
    int i;
    strclr( playerList );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( strlen( masterRoster[i].netID ) ) {
            if ( ( rosterIsValidGUID( masterRoster[i].steamID )) && ( 0 != strlen( masterRoster[i].IPaddress )) )  {   
		switch ( infoDepth ) {
		case 1:   //  includes name + steamID for identifying names with alt-charsets, less privacy
                    snprintf( single, 256, "%s[%s]%s", masterRoster[i].playerName, masterRoster[i].steamID, delimiter );
		    break;
		case 2:   // security web display that includes name:SteamID::IP
                    snprintf( single, 256, "%s[%s:%s]%s", 
                        masterRoster[i].playerName, masterRoster[i].steamID, masterRoster[i].IPaddress, delimiter );
		    break;
		case 3:   // alternative web display that includes name:score
                    snprintf( single, 256, "%s[%s]%s", masterRoster[i].playerName, masterRoster[i].score, delimiter );
		    break;
		case 4:   // used for synthetic event handler, for reliable but less responsive player conect/disconnect
                    snprintf( single, 256, "%s %s %s%s", 
                        masterRoster[i].steamID, reformatIP( masterRoster[i].IPaddress ), masterRoster[i].playerName, delimiter );
		    break;
		case 5:   // SteamID list only
                    snprintf( single, 256, "%s%s", masterRoster[i].steamID, delimiter );
		    break;
	        default:  // (case 0) default miniamlist public dispaly, player name only
                    snprintf( single, 256, "%s%s", masterRoster[i].playerName, delimiter );
		}
                strlcat( playerList, single, 4096 );
            }
        }
    }
    return( playerList );
}


//  ==============================================================================================
//  rosterDump (test)
//
//  ...
//
void rosterDump( int humanFlag, int npcFlag )
{
    int i, isNPC, isPrintable;

    for (i=0; i<ROSTER_MAX; i++) {
        if ( strlen( masterRoster[i].netID ) ) {
            isNPC = 0;
            isPrintable = 0;
            if (0==strcmp( masterRoster[i].netID, "0")) isNPC = 1;

            if (npcFlag && isNPC)    isPrintable = 1;
            if (humanFlag && !isNPC) isPrintable = 1;

            printf("%d::%s::%s::%s::%s::%s\n", 
                isPrintable,
                masterRoster[i].netID, 
                masterRoster[i].playerName,
                masterRoster[i].steamID,
                masterRoster[i].IPaddress,
                masterRoster[i].score );
        }
    }
    return;
}


//  ==============================================================================================
//  rosterParseCacheDestroyed
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the Cache Destruction event.
//
//  [2020.03.05-00.35.45:360][277]LogGameplayEvents: Display: Objective 4 owned by team 1 was 
//      destroyed for team 0 by YourPlayerName6[76560000000000006], YourPlayerName7[76560000000000007], 
//      YourPlayerName1[76560000000000001], YourPlayerName4[76560000000000004], 
//      YourPlayerName5[76560000000000005]. 
//
#define DESTROY1_STRING "owned by team 1 was destroyed for team 0 by "
#define DESTROY2_STRING "owned by team 0 was destroyed for team 1 by "
#define DESTROY_PREFIX  "[76"

void rosterParseCacheDestroyed( char *destroyString, int maxChars, char *playerName, char *playerGUID, char *playerIP )
{
    char localBuffer[ 4096 ];
    char *p, *q;

    strlcpy( localBuffer, destroyString, 4096 );

    p = strstr( localBuffer, DESTROY1_STRING );
    if ( p == NULL ) p = strstr( localBuffer, DESTROY2_STRING );

    if ( p != NULL ) {
        p += strlen( DESTROY1_STRING );
        q = strstr( p, DESTROY_PREFIX );
        if ( q != NULL ) if ( strlen( q ) >= 18 ) {
            strlcpy( playerGUID, q+1, 18 );
            strlcpy( playerIP,   rosterLookupIPFromGUID( playerGUID ), maxChars );
            strlcpy( playerName, rosterLookupNameFromGUID( playerGUID ), maxChars );
        }
    }
    return;
}


//  ==============================================================================================
//  rosterParsePlayerSynthConn
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the Synthetic client add event.
//
//  "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
void rosterParsePlayerSynthConn( char *connectString, int maxSize,  char *playerName, char *playerGUID, char *playerIP )
{
    strlcpy( playerName, &connectString[ 45 ], maxSize );
    strlcpy( playerGUID, getWord( connectString, 1, " " ), maxSize );
    strlcpy( playerIP,   getWord( connectString, 2, " " ), maxSize );
    return;
}

//  ==============================================================================================
//  rosterParsePlayerSynthDisConn
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the Synthetic client disconnect event.
//
//  "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
void rosterParsePlayerSynthDisConn( char *connectString, int maxSize,  char *playerName, char *playerGUID, char *playerIP )
{
    strlcpy( playerName, &connectString[ 45 ], maxSize );
    strlcpy( playerGUID, getWord( connectString, 1, " " ), maxSize );
    strlcpy( playerIP,   getWord( connectString, 2, " " ), maxSize );
    return;
}

//  ==============================================================================================
//  rosterParsePlayerConn
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the client add event.
//
void rosterParsePlayerConn( char *connectString, int maxSize,  char *playerName, char *playerGUID, char *playerIP )
{
    // Refresh the roster from RCON first, THEN call this routine
    //
    char *w;

    // parse the log string for connect - only has playerName
    // Example:  [2019.07.26-01.45.36:776][792]LogNet: Join succeeded: PlayerName
    //
    w = strstr( connectString, "Join succeeded: " );
    if ( w != NULL ) {
        strlcpy( playerName, &w[ 16 ], maxSize );
        playerName[ strlen( playerName ) - 1] = 0;    // get rid of trailing LF/CR

        // Do a lookup of playerGUID and playerID
        //
        strlcpy( playerGUID, rosterLookupSteamIDFromName( playerName ), maxSize );
        strlcpy( playerIP,   rosterLookupIPFromName( playerName ), maxSize );
    }
    return;
}

//  ==============================================================================================
//  rosterParsePlayerDisConn
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the client disconnect event.
//
void rosterParsePlayerDisConn( char *connectString, int maxSize, char *playerName, char *playerGUID, char *playerIP )
{
    // Call this first THEN refresh the roster from RCON
    //
    char *w, *v;

    // parse the log string for disconnect - only has IP:port available
    // Example: [2019.07.26-01.47.06:457][106]LogNet: UChannel::Close: Sending CloseBunch. ChIndex == 0. 
    //     Name: [UChannel] ChIndex: 0, Closing: 0 [UNetConnection] RemoteAddr: 12.123.123.12:12345, 
    //     Name: IpConnection_3, Driver: GameNetDriver IpNetDriver_0, IsServer: YES, PC: INSPlayerController_2, 
    //     Owner: INSPlayerController_2, UniqueId: SteamNWI:UNKNOWN [0x7F6B5A5BC340]

    w = strstr( connectString, "RemoteAddr: " );
    if ( w != NULL ) {
        // extract player IP# (no port#)
        if ( NULL != ( v = getWord( w, 1, " :," ))) 
            strlcpy( playerIP, v, maxSize);
        else 
            strlcpy( playerIP, "", maxSize );

        // Do a lookup of playerGUID and playerName
        strlcpy( playerName, rosterLookupNameFromIP( playerIP ), maxSize );
        strlcpy( playerGUID, rosterLookupSteamIDFromName( playerName ), maxSize );
    }
}


//  ==============================================================================================
//  rosterParsePlayerChat
//
//  Parse the log line 'say' chat log into originator GUID and the said text string
//  [2019.08.30-23.39.33:262][176]LogChat: Display: name(76561198000000001) Global Chat: !ver sissm
//  [2019.09.15-03.44.03:500][993]LogChat: Display: name(76561190000000002) Team 0 Chat: !v
//
int rosterParsePlayerChat( char *strIn, int maxChars, char *clientID, char *chatLine )
{
    int errCode = 1, i;
    char strIn2[256];
    char *steamID = NULL, *chatText = NULL;

    // check if this is a valid chat line
    //
    if ( NULL != strstr( strIn, "LogChat: Display:" )) {

        // make a copy and get rid of last carriage return
        //
        strlcpy( strIn2, strIn, 256 );
        for (i = 0; i< (int) strlen( strIn2 ); i++ )
            if ( strIn2[i] == '\015') strIn2[i] = 0;

        // Go find the first character of typed text.  Check 3 cases for a match.
        // if none found, chatText is NULL
        //
        if ( chatText == NULL )  chatText = strstr( strIn2, "Global Chat: " );
        if ( chatText == NULL )  chatText = strstr( strIn2, "Team 0 Chat: " );
        if ( chatText == NULL )  chatText = strstr( strIn2, "Team 1 Chat: " );
        if ( chatText != NULL )  {
            chatText += 13;
            strlcpy( chatLine, chatText, maxChars );
            errCode = 0;
        }

        // Find the steam ID of the originator
        //
        strclr( clientID );
        if ( NULL != (steamID = strstr( strIn2, "(76" ) )) {
            strlcpy( clientID, &steamID[1], 18 );
        }

    }
    return errCode;
}


//  ==============================================================================================
//  rosterParseSessionID
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the session recording ID change event.
//
void rosterParseSessionID( char *sessionLogString, int maxChars, char *sessionID )
{
    char *w;
    int i, j;

    strclr( sessionID );
    w = strstr( sessionLogString, "SessionName:" );
    if ( w != NULL ) {
        strlcpy( sessionID, &w[13], maxChars );
        i = (int) strlen( sessionID );
        if ( i >= 1 )  {
            for (j = 0; j<i; j++)  {
                if ( sessionID[j] == 0x0d ) {
                    sessionID[j] = 0;
                    break;
                }
            }
        }
    }
    // if ( w != NULL ) strlcpy( sessionID, &w[13], 37 );

    return;
}


//  ==============================================================================================
//  rosterparseMapname
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the map change event.
//
void rosterParseMapname( char *mapLogString, int maxChars, char *mapName )
{
    char *w;

    strclr( mapName );
    w = strstr( mapLogString, "SeamlessTravel to:" );
    if ( w != NULL ) strlcpy( mapName, &w[19], maxChars );
    return;
}


//  ==============================================================================================
//  rosterSyntheticChangeEvent
//
//  This routine is an event generator -- ordinarily events are triggered by logfile
//  poller.  In rare cases, events must be internally generated, hence the term
//  synthetic event.  
//
//  This loop finds missing client data going from previous sample to the current, parse it, and 
//  invoke callback for each of the missing element.  Calling this routine in forward
//  direction is 'disconnect'; calling it backwards finds 'new connections'
//

#define SIZEOFGUID  (17)            // e.g., 76561100000000000 
#define SIZEOFIP    (15)            // e.g., 123.035.001.024

int rosterSyntheticChangeEvent( char *prevRoster, char *currRoster, int (*callback)( char *, char *, char *))
{
    char *w;
    char playerGUID[256], playerIP[256], playerName[256];
    int i = 0;

    while (1 == 1) {
        w = getWord( prevRoster, i++, "\011" );
        if ( w == NULL )      break;
        if ( 0 == strlen(w) ) break;

        if ( NULL == strstr( currRoster, w ) ) {  // if missing

            // parse the w
            //
            strlcpy( playerGUID, w, SIZEOFGUID+1 );
            playerGUID[ SIZEOFGUID+1 ] = 0;    // safety (probably not needed)
            strlcpy( playerIP,   &w[SIZEOFGUID+1], SIZEOFGUID );
            playerIP[ SIZEOFIP ] = 0;          // safety (probably not needed)
            strlcpy( playerName,  &w[SIZEOFGUID+SIZEOFIP+2], 256 );

            (*callback)( playerName, playerIP, playerGUID );
        }
    }
    return 0;
}



//  ==============================================================================================
//  _rosterTest
//
//  Test/dev method that reads captured raw RCON data dump into 'buf' then parse.
//  This routine is not called in operational program.
//

#if 0
static int _rosterReadTest( char *binaryDumpFile, unsigned char *buf )
{
    int i, n;
    FILE *fpr;
    char *p = NULL;

    fpr = fopen( binaryDumpFile, "rb" );
    if ( fpr == NULL ) {
        printf("\nOpen error\n");
        abort();
    }
    n = fread( buf, 1, 8*4096, fpr );
    printf("\nN = %d\n", n);
    if (  p == NULL) printf("\nRead error\n");
    fclose( fpr );

    i = 0;
    while (1)  {
        printf("%c", buf[i++]);
        // if (buf[i] == 0) break;
        if (i == n) break;
    }
    printf("\n\n----------------------\n\n");
    return( n );
}

int roster_test_main()
{
    unsigned char buf[4*4096];  
    int n;

    // 275::Name::76560000000000001::111.22.33.44::470
    rosterInit();
    rosterReset();
    n = _rosterReadTest( "dump.bin", buf );
    tabDumpDebug( buf, n );
    printf("\n---Parsing -----\n");
    rosterParse( buf, n );
    printf("\n---Dumping -----\n");
    rosterDump( 0, 0 );
    printf("\n---Counting-----\n");
    printf("\nCount %d\n", rosterCount());
    printf( "%s::%s::%s\n", 
        rosterLookupNameFromIP( "100.36.84.54" ),
        rosterLookupSteamIDFromName( "TestFlight" ),
        rosterLookupIPFromName( "TestFlight" )
    );
    printf("Roster ::%s::\n", rosterPlayerList(2, " : "));
    // rosterWeb();
}

#endif
