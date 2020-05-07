//  ==============================================================================================
//
//  Module: EVENTS
//
//  Description:
//  Event-driven engine with callback features: init, register, dispatch
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================


#define SISSM_MAXEVENTS                     (32)
#define SISSM_MAXPLUGINS                    (32)

#define SISSM_EV_CHAT                       ( 0)
#define SISSM_EV_INIT                       ( 1)    // order of the indeces must match
#define SISSM_EV_RESTART                    ( 2)    // the sequence of eventsTable[]
#define SISSM_EV_CLIENT_ADD                 ( 3)
#define SISSM_EV_CLIENT_DEL                 ( 4)
#define SISSM_EV_MAPCHANGE                  ( 5)
#define SISSM_EV_GAME_START                 ( 6)
#define SISSM_EV_GAME_END                   ( 7)
#define SISSM_EV_ROUND_START                ( 8)
#define SISSM_EV_ROUND_END                  ( 9)
#define SISSM_EV_OBJECTIVE_CAPTURED         (10)
#define SISSM_EV_PERIODIC                   (11)
#define SISSM_EV_CLIENT_ADD_SYNTH           (12)
#define SISSM_EV_CLIENT_DEL_SYNTH           (13)
#define SISSM_EV_SHUTDOWN                   (14)
#define SISSM_EV_SIGTERM                    (15)
#define SISSM_EV_WINLOSE                    (16)
#define SISSM_EV_TRAVEL                     (17)
#define SISSM_EV_SESSIONLOG                 (18)
#define SISSM_EV_OBJECT_SYNTH               (19)                       // active objective change
#define SISSM_EV_CACHE_DESTROY              (20)                       // Cache Destroyed event

#define SISSM_EV_BP_PLAYER_CONN             (21)
#define SISSM_EV_BP_PLAYER_DISCONN          (22)
#define SISSM_EV_BP_CHARNAME                (23)
#define SISSM_EV_BP_TOUCHED_OBJ             (24)
#define SISSM_EV_BP_UNTOUCHED_OBJ           (25)


// Following substring in log file triggers an event
//
#define SS_SUBSTR_ROUND_START  "Match State Changed from PreRound to RoundActive"
#define SS_SUBSTR_ROUND_END    "Match State Changed from RoundWon to PostRound"
#define SS_SUBSTR_GAME_START   "Match State Changed from GameStarting to PreRound"
#define SS_SUBSTR_GAME_END     "Match State Changed from GameOver to LeavingMap"
#define SS_SUBSTR_REGCLIENT    "LogNet: Join succeeded:"
#define SS_SUBSTR_UNREGCLIENT  "LogNet: UChannel::Close:"
#define SS_SUBSTR_OBJECTIVE    "LogGameMode: Display: Advancing spawns for faction"
#define SS_SUBSTR_MAPCHANGE    "SeamlessTravel to:"
#define SS_SUBSTR_CAPTURE      "LogSpawning: Spawnzone '"
#define SS_SUBSTR_SHUTDOWN     "LogExit: Game engine shut down"
#define SS_SUBSTR_CHAT         "LogChat: Display:"
#define SS_SUBSTR_WINLOSE      "LogGameMode: Display: Round Over: Team"
#define SS_SUBSTR_TRAVEL       "LogGameMode: ProcessServerTravel:"
#define SS_SUBSTR_SESSIONLOG   "HttpStartUploadingFinished. SessionName:"
#define SS_SUBSTR_DESTROY      "was destroyed for team"

#define SS_SUBSTR_BP_PLAYER_CONN        " RestartPlayerAtPlayerStart "
#define SS_SUBSTR_BP_PLAYER_DISCONN     " Unpossessed"
#define SS_SUBSTR_BP_CHARNAME           "' cached new pawn '"
// #define SS_SUBSTR_BP_TOUCHED_OBJ        " has been touched by sol"  
// #define SS_SUBSTR_BP_UNTOUCHED_OBJ      " has stopped being touched by sol" 
#define SS_SUBSTR_BP_TOUCHED_OBJ        " entered."
#define SS_SUBSTR_BP_UNTOUCHED_OBJ      " exited."


extern int eventsInit( void );
extern int eventsRegister( int eventID, int (*callBack)( char * ));
extern int eventsDispatch( char *strBuffer );


