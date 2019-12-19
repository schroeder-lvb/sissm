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

extern int   apiInit( void );
extern int   apiDestroy( void );
extern int   apiServerRestart( char *reasonForRestart );
extern int   apiGameModePropertySet( char *gameModeProperty, char *value );
extern char *apiGameModePropertyGet( char *gameModeProperty );
extern int   apiSay( const char * format, ... );
extern int   apiSaySys( const char * format, ... );
extern int   apiKickOrBan( int isBan, char *playerGUID, char *reason );
extern int   apiRcon( char *commandOut, char *statusIn );
extern int   apiPlayersGetCount( void );
extern char *apiPlayersRoster( int infoDepth, char *delimeter );
extern char *apiGetServerName( void );
extern char *apiGetServerNameRCON( int forceCacheRefresh );
extern char *apiGetMapName( void );
extern char *apiGetSessionID( void );
extern unsigned long apiTimeGet( void );
extern char  *apiTimeGetHuman( unsigned long timeMark );
extern unsigned long apiGetLastRosterTime( void );
extern int   apiBadNameCheck( char *nameIn );


#define IDLISTMAXELEM        (256)
#define IDLISTMAXSTRSZ        (40)
#define IDSTEAMID64LEN        (17)

#define WORDLISTMAXELEM     (1024)
#define WORDLISTMAXSTRSZ      (32)


typedef char idList_t[IDLISTMAXELEM][IDLISTMAXSTRSZ];
extern int   apiIdListRead( char *listFile, idList_t idList );
extern int   apiIdListCheck( char *connectID, idList_t idList );

extern int   apiIsAdmin( char *connectID );
extern int   apiAuthIsAllowedCommand( char *playerGUID, char *command );
extern int   apiAuthIsAllowedMacro( char *playerGUID, char *command );
extern int   apiAuthIsAttribute( char *playerGUID, char *attriute );
extern char *apiAuthGetRank( char *playerGUID );


typedef char wordList_t[WORDLISTMAXELEM][WORDLISTMAXSTRSZ];
extern int apiWordListRead( char *listFile, wordList_t wordList );
extern int apiWordListCheck( char *stringTested, wordList_t wordList );


// for recalling last reboot event - time and reason
//
extern unsigned long apiGetLastRebootTime( void );
extern char *apiGetLastRebootReason( void );

