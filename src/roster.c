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

#include "winport.h"   // strcasestr

static rconRoster_t masterRoster[ROSTER_MAX];
static char rosterServerName[256], rosterMapName[256];



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
#if 1
        else if (0 == strncmp( src, "| ",  2)) strlcpy( dst, &src[2], maxChars );
        else if (0 == strncmp( src, " |",  2)) strlcpy( dst, &src[2], maxChars );
#endif
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
        strcpy(masterRoster[i].netID,      "");
        strcpy(masterRoster[i].playerName, "");
        strcpy(masterRoster[i].steamID,    "");
        strcpy(masterRoster[i].IPaddress,  "");
        strcpy(masterRoster[i].score,      "");
    }
}

//  ==============================================================================================
//  rosterInit
//
//  Initialize the entire roster database 
//
void rosterInit( void )
{
    strcpy( rosterServerName, "" );
    strcpy( rosterMapName,    "" );
    rosterReset();
    return;
}


//  ==============================================================================================
//  _tabDumpDebug (internal, test only)
//
//  Development tool for parsing RCON roster list with tab as a delimiter.  It is left here
//  because it is handy for maintaining parser code, debugging.  THe argument 'buf' is the
//  exact image of what is read from RCON after listplayers command is issued.  The argument
//  'n' is the bytecount returned from the TCP/IP read procedure.
//
static int _tabDumpDebug( unsigned char *buf, int n )
{
    int i, j, validFlag;
    char *w, *headStr, *recdStr;

    headStr = strstr( &buf[32], "========================" );    // look for start of separator
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
int rosterParse( unsigned char *buf, int n )
{
    int i, j, validFlag;
    char *w, *headStr, *recdStr, *atLeastOne;

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
        strcpy( masterRoster[j].netID,       "" );
        strcpy( masterRoster[j].playerName,  "" );
        strcpy( masterRoster[j].steamID,     "" );
        strcpy( masterRoster[j].IPaddress,   "" );
        strcpy( masterRoster[j].score,       "" );

    }
    else {
        logPrintf( LOG_LEVEL_WARN, "roster", "Received non-divider listplayer rcon response size %d", n );
    }

    return( j ); 
}

//  ==============================================================================================
//  rosterCount
//
//  Counts and returns number of active human players in the current database snapsho.
//
int rosterCount( void )
{
    int i, count = 0;

    for (i=0; i<ROSTER_MAX; i++) {
        if ( strlen( masterRoster[i].netID ) ) {
            if (( rosterIsValidGUID(  masterRoster[i].steamID)) && ( 0 != strlen( masterRoster[i].IPaddress )) )  { 
                count++;
            }
        }
    }
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
    strcpy( playerName, "" );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( 0 == strcmp( masterRoster[i].IPaddress, playerIP )) {
            strlcpy( playerName, masterRoster[i].playerName, 256 ); 
            break;
        }
    }
    return( playerName );
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
    strcpy( steamID, "" );
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
char *rosterLookupSteamIDFromPartialName( char *partialName )
{
    static char steamID[256];
    int i, matchCount = 0;

    strcpy( steamID, "" );
    for (i=0; i<ROSTER_MAX; i++) {
        if ( NULL != strcasestr( masterRoster[i].playerName, partialName )) {
            strlcpy( steamID, masterRoster[i].steamID, 256 ); 
            matchCount++;
            break;
        }
    }
    if ( matchCount != 1 ) strcpy( steamID, "" );

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
    strcpy( playerIP, "" );
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
    strcpy( playerList, "" );
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

            printf("%s::%s::%s::%s::%s\n", 
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
//  rosterparseMapname
//
//  This is one of series of parser called by event callback routine.
//  This one is associated with the map change event.
//
void rosterParseMapname( char *mapLogString, int maxChars, char *mapName )
{
    char *w;

    strcpy( mapName, "" );
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
    char *p;

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


int main()
{
    unsigned char buf[4*4096];  
    int n;

    // 275::Name::76560000000000001::111.22.33.44::470

    rosterInit();
    rosterReset();
    n = _rosterReadTest( "dump.bin", buf );
    _tabDumpDebug( buf, n );

    printf("\n----------------\n");

    rosterParse( buf, n );

    rosterDump( 0, 0 );

    printf("\n----------------\n");

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



