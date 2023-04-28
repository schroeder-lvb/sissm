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

#define FINALCOUNTER_REDUCE  (0)     // experimental code, do not enable
#define MUTATOR_SELECT       (0)     // experimetnal code, do not enable

#define API_ROSTER_STRING_MAX        (256*64)      // max size of contatenated roster string +EPIC
#define API_LINE_STRING_MAX             (256)
#define API_CMDS_STRING_MAX             (512)


extern int   apiInit( void );
extern void  apiInitAuth( void );
extern void  apiInitBadWords( void );
extern int   apiDestroy( void );
extern int   apiServerRestart( char *reasonForRestart );
extern int   apiGameModePropertySet( char *gameModeProperty, char *value );
extern char *apiGameModePropertyGet( char *gameModeProperty );
extern char *apiGameModePropertyGetRaw( char *gameModeProperty );
extern int   apiSay( const char * format, ... );
extern int   apiSaySys( const char * format, ... );
extern int   apiKickOrBan( int isBan, char *playerGUID, char *reason );
extern int   apiKick( char *playerNameOrGUID, char *reason );
extern int   apiKickAll( char *reason );
extern int   apiRcon( char *commandOut, char *statusIn );
extern int   apiBotRespawn( int resetAlgo );   // experimental bot reset-respawn
extern int   apiPlayersGetCount( void );
extern char *apiPlayersRoster( int infoDepth, char *delimeter );
extern char *apiGetServerName( void );
extern char *apiGetServerNameRCON( int forceCacheRefresh );
extern char *apiGetServerMode( void );
extern char *apiGetMapName( void );
extern char *apiGetLighting( void );
extern char *apiGetSessionID( void );
extern unsigned long apiTimeGet( void );
extern char *apiTimeGetHuman( unsigned long timeMark );
extern unsigned long apiGetLastRosterTime( void );
extern int   apiBadNameCheck( char *nameIn, int exactMatch );


extern int   apiMapcycleRead( char *mapcycleFilePath );
extern int   apiMapChange( char *mapName, char *gameMode, int secIns, int dayNight, char *mutatorsList, int clearMutator );
extern char *apiMapList(void );
extern int   apiIsSupportedGameMode( char *candidateMode  );
extern int   apiIsCounterAttack( void );

extern int   apiMutLookup( char *partialString, char *fullString, int maxStringSize );
extern char *apiMutList( void );
extern char *apiMutActive( int fetchNow );


extern int   apiIsHotRestart( void );

#define IDSTEAMID64LEN        (17)
#define IDEPICIDLEN           (65)   // EPIC 

#define WORDLISTMAXELEM     (1024)
#define WORDLISTMAXSTRSZ     ( 32)  

#define API_MAXENTITIES       (64)

#define API_R_BUFSIZE                (4*1024)
#define API_T_BUFSIZE                (4*1024)
#define API_MAXSAY                       (80)       // Maximum string that can be printed by "say"

#define API_MUT_MAXLIST                  (64)
#define API_MUT_MAXCHAR                  (80)


// List manager & data structure
//
#define IDLISTMAXELEM        (256)
#define IDLISTMAXSTRSZ       (256)   // EPIC
typedef char idList_t[IDLISTMAXELEM][IDLISTMAXSTRSZ];
extern int   apiIdListRead( char *listFile, idList_t idList );     // list read from file
extern int   apiIdListCheck( char *connectID, idList_t idList );   // exact match
extern int   apiIdListMatch( char *connectID, idList_t idList );   // substr match
extern int   apiIdListAdd( char *connectID, idList_t idList );     // insert elem to list
extern int   apiIdListClear( idList_t idList );                    // clear the list
extern char *apiIdListGet( idList_t idList, int index );           // get elem from list

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

// for translating CharacterID to/from Player Name
//
extern int   apiBPIsActive( void );
extern char *apiNameToCharacter( char *playerName );
extern char *apiCharacterToName( char *characterID );
extern int   apiBPPlayerCount( void );
extern int   apiIsPlayerAliveByName( char *playerName );
extern int   apiIsPlayerAliveByGUID( char *playerGUID );

// for looking up objective letter 'A' 'B' 'C'... from cached objective
// name instead of parsing the names of the objective  (3rd party maps)
//
extern char apiLookupObjectiveLetterFromCache( char *objectiveName );
extern char apiLookupLastObjectiveLetterFromCache( void );

// clan list to work with picladmin and pigateway (!lock)
// 
extern int   apiIsClan( char *playerID );
extern int   apiClanAdd( char *playerID );
extern int   apiClanClear( void );
extern char *apiClanGet( int index );


