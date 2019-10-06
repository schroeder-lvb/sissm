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

#define ROSTER_MAX       (128)
#define ROSTER_FIELD_MAX (80)

typedef struct {

    char netID[ROSTER_FIELD_MAX];
    char playerName[ROSTER_FIELD_MAX];
    char steamID[ROSTER_FIELD_MAX];
    char IPaddress[ROSTER_FIELD_MAX];
    char score[ROSTER_FIELD_MAX];

} rconRoster_t;

int rosterIsValidGUID( char *testGUID );
extern void rosterSetMapName( char *mapName );
extern char *rosterGetMapName( void );

extern void rosterSetTravel( char *travelName );
extern int  rosterGetCoopSide( void );
extern char *rosterGetScenario( void );
extern char *rosterGetMutator( void );

extern void rosterSetServerName( char *serverName );
extern char *rosterGetServerName( void );

extern void rosterReset( void );
extern void rosterInit( void );
extern int  rosterParse( unsigned char *buf, int n );
extern int  rosterCount( void );
extern char *rosterLookupNameFromIP( char *playerIP );
extern char *rosterLookupSteamIDFromName( char *playerName );
extern char *rosterLookupSteamIDFromPartialName( char *partialName );
extern char *rosterLookupIPFromName( char *playerName );
extern char *rosterPlayerList( int infoDepth, char *delimeter );
extern void rosterDump( int humanFlag, int npcFlag );

extern void rosterParsePlayerConn( char *connectString, int maxChars, char *playerName, char *playerGUID, char *playerIP );
extern void rosterParsePlayerDisConn( char *connectString, int maxChars, char *playerName, char *playerGUID, char *playerIP );

extern void rosterParsePlayerSynthConn( char *connectString, int maxChars, char *playerName, char *playerGUID, char *playerIP );
extern void rosterParsePlayerSynthDisConn( char *connectString, int maxChars, char *playerName, char *playerGUID, char *playerIP );

extern void rosterParseMapname( char *mapLogString, int maxChars, char *mapName );

extern int rosterSyntheticChangeEvent( char *prevRoster, char *currRoster, int (*callback)( char *, char *, char *));



