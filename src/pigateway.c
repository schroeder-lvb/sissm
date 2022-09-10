//  ==============================================================================================
//
//  Module: PIGATEWAY
//
//  Description:
//  Reserved slots control & bad-name player auto-kick
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#define _GNU_SOURCE                                                            // for strcasestr()

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
#include "winport.h"                                                           // for strcasestr()

#include "roster.h"
#include "api.h"
#include "p2p.h"
#include "sissm.h"

#include "pigateway.h"

//  ==============================================================================================
//  Data definition 
//
//
#define PRILISTMAX (100)
#define PIGATEWAY_RESTART_LOCKOUT_SEC  (60)      // #secs to exempt full-server kick after restart

static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int  firstAdminSlotNo;            // first slot# to start checking if player is on access list
    char adminPortKickMsgLock[CFS_FETCH_MAX];                // kick message if not on access list
    char adminPortKickMsgFull[CFS_FETCH_MAX];                    // kick message if server is full
    // char adminListFilePath[CFS_FETCH_MAX];
    int  gameChangeLockoutSec;
    int  enableBadNameFilter;
    int  minPlayerNameLength;               // minimum char length of player name required to join
    char minPlayerNameMsg[CFS_FETCH_MAX];                 // minimum char length message to player

    char chatTrollWords[CFS_FETCH_MAX];      // auto-kick extreme profanity or trolls in chat text

    unsigned int  allowInWindowTimeSec;                 // picladmin !allow window time in seconds

    int  adminPortDisable;                   // for disable admin port blocking during game change
    alarmObj *aPtr;                                 // create a 'lockout' alarm druing game change

} pigatewayConfig;

// static char prioritySteamID[PRILISTMAX][80];

static unsigned long timeRestarted = 0L;


//  ==============================================================================================
//  _isPriority
//
//  Check if a client GUID is an admin 
//
static int _isPriority( char *connectID )
{
    int isMatch = 0;

    // check if player has "priport" attribute
    //
    // isMatch = apiIsAdmin( connectID );
    isMatch = apiAuthIsAttribute( connectID, "priport" );

    return( isMatch );
}


//  ==============================================================================================
//  _isBadName
//
//  Check if a player name contains a 'bad' profanity word:  returns 1.
//  Check if a player GUID is under suspention:  return 2.
//
static int _isBadName( char *playerName, char *playerGUID )
{
    int isBad = 0;

    if ( apiBadNameCheck( playerName, 0 ) )          // check if substr contains offensive words
        isBad = 1;
    else if ( apiBadNameCheck( playerGUID, 1 ) )      // check if exact GUID is under access suspention 
        isBad = 2; 
    
    return( isBad );
}


//  ==============================================================================================
//  pigatewayCGameChangeAlarmCB
//
//  Legacy:  Alarm function that exempt admin-kick during map change.  This is now obsolete
//  but code is still active due to unpredictability of the game server development.
//
int pigatewayGameChangeAlarmCB( char *str ) 
{
    pigatewayConfig.adminPortDisable = 0;
    logPrintf( LOG_LEVEL_DEBUG, "pigateway", "Gateway Lockout is CLEAR" );
    return 0;
}


//  ==============================================================================================
//  pigatewayInitConfig
//
//  Read from config file, init this plugin
//
int pigatewayInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pigateway.pluginstate" variable from the .cfg file
    //
    pigatewayConfig.pluginState = (int) cfsFetchNum( cP, "pigateway.pluginState", 0.0 );  // disabled by default

    pigatewayConfig.firstAdminSlotNo = (int) cfsFetchNum( cP, "pigateway.firstAdminSlotNo", 11 );
    strlcpy( pigatewayConfig.adminPortKickMsgFull, 
        cfsFetchStr( cP, "pigateway.adminPortKickMsgFull", "Server full." ), CFS_FETCH_MAX);
    strlcpy( pigatewayConfig.adminPortKickMsgLock, 
        cfsFetchStr( cP, "pigateway.adminPortKickMsgLock", "Server hosting a special event." ), CFS_FETCH_MAX);

    pigatewayConfig.enableBadNameFilter = (int) cfsFetchNum( cP, "pigateway.enableBadNameFilter", 1 );
    pigatewayConfig.gameChangeLockoutSec = (int) cfsFetchNum( cP, "pigateway.gameChangeLockoutSec", 120 );
    // strlcpy( pigatewayConfig.adminListFilePath,  
        // cfsFetchStr( cP, "pigateway.adminListFilePath",  "admins.txt" ), CFS_FETCH_MAX);
    pigatewayConfig.allowInWindowTimeSec = (unsigned long int) cfsFetchNum( cP, "pigateway.allowInWindowTimeSec", 180 );

    pigatewayConfig.minPlayerNameLength = (int) cfsFetchNum( cP, "pigateway.minPlayerNameLength", 0 );
    strlcpy( pigatewayConfig.minPlayerNameMsg,  
        cfsFetchStr( cP, "pigateway.minPlayerNameMsg", "Player name too short" ), CFS_FETCH_MAX);
    strlcpy( pigatewayConfig.chatTrollWords,  
        cfsFetchStr( cP, "pigateway.chatTrollWords", "admin: admins: admin;" ), CFS_FETCH_MAX);

    cfsDestroy( cP );

    // _priSlots_init( pigatewayConfig.adminListFilePath, 0 );

    pigatewayConfig.adminPortDisable = 0;
    pigatewayConfig.aPtr = alarmCreate( pigatewayGameChangeAlarmCB );
    logPrintf( LOG_LEVEL_DEBUG, "pigateway", "Created lockout alarm" );

    return 0;
}

//  ==============================================================================================
//  (internal) _allowInExemption
//
//  Returns true if playerName is under special full-server join grant by admin-command (picladmin) 
//  
static int _allowInExemption( char *playerName )
{
    unsigned long timeMarked;
    int allowIn = 0;
    char *allowSubstring;

    timeMarked = p2pGetL( "picladmin.p2p.allowTimeStart", 0L );
    allowSubstring = p2pGetS( "picladmin.p2p.allowInPattern", "" );

    if (( apiTimeGet() - timeMarked ) < pigatewayConfig.allowInWindowTimeSec ) {
        if (0 == strcmp( allowSubstring, "*" )) 
            allowIn = 1;
        else if ( NULL != strcasestr( playerName, allowSubstring )) 
            allowIn = 1;
    }

    return( allowIn );
}


//  ==============================================================================================
//  pigatewayClientSynthAddCB
//
//  Proces incoming client connection
//  
int pigatewayClientSynthAddCB( char *strIn )
{
    static char playerName[256], playerGUID[256], playerIP[256];
    int alreadyKicked = 0;
    int kickReason, firstAdminSlot;

    rosterParsePlayerSynthConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_INFO, "pigateway", "Synthetic ADD Callback Name ::%s:: IP ::%s:: GUID ::%s::", 
        playerName, playerIP, playerGUID );

    // copy to local, first admin slot from sissm.cfg
    //
    firstAdminSlot = pigatewayConfig.firstAdminSlotNo;
  
    // server lockout via picladmin "lock on|off" command
    // if "locked" then make all slots "admin" or via the allowin exemption
    //
    if ( 0L != p2pGetL( "pigateway.p2p.lockOut", 0L ))   // default is 0L = unlock
        firstAdminSlot = 1;

    if (apiPlayersGetCount() >= firstAdminSlot ) {         // check if these are admin-only slots
        if ( _allowInExemption( playerName ) ) {
            logPrintf( LOG_LEVEL_CRITICAL, "pigateway", "Special full server join granted by admin ::%s::%s::%s::", 
                    playerName, playerGUID, playerIP );
        }
        else if ( !_isPriority( playerGUID ) && (pigatewayConfig.adminPortDisable == 0) ) {     // check if this is an admin

            if ( (apiTimeGet() - timeRestarted) > PIGATEWAY_RESTART_LOCKOUT_SEC ) {  // check if we are restarting
                // apiKickOrBan( 0, playerGUID, "Server_full, under maintenance, or hosting a private session." );

                if ( 0L == p2pGetL( "pigateway.p2p.lockOut", 0L ) ) {
                    apiKickOrBan( 0, playerGUID, pigatewayConfig.adminPortKickMsgFull );    // server full
                    apiSay ( "Player %s kicked Server Full", playerName );
                }
                else {
                    apiKickOrBan( 0, playerGUID, pigatewayConfig.adminPortKickMsgLock );    // special events !lock
                    apiSay ( "Player %s kicked Server Reserved", playerName );
                }

                logPrintf( LOG_LEVEL_CRITICAL, "pigateway", "Full Server Kick ::%s::%s::%s::", 
                    playerName, playerGUID, playerIP );
                alreadyKicked = 1;
            }
            else {
                logPrintf( LOG_LEVEL_INFO, "pigateway", 
                    "Exempt Full Server Kick due to Restart ::%s::%s::%s::", playerName, playerGUID, playerIP );
            }
        }
    }

    // process badname auto-kicking
    //
    if ( (0 == alreadyKicked) && (pigatewayConfig.enableBadNameFilter) ) {
        if ( 0 != (kickReason = _isBadName( playerName, playerGUID )) ) {
            apiKickOrBan( 0, playerGUID, "" );
            if ( kickReason == 1 ) {
                apiSay ( "Player %s auto-kicked by server", playerName );
                logPrintf( LOG_LEVEL_INFO, "pigateway", "Bad Name Auto-kick ::%s::%s::%s::", 
                    playerName, playerGUID, playerIP );
            }
            else {
                logPrintf( LOG_LEVEL_INFO, "pigateway", "Suspended User Auto-kick ::%s::%s::%s::", 
                    playerName, playerGUID, playerIP );
            }
            alreadyKicked = 1;
        }
    }

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
int pigatewayClientSynthDelCB( char *strIn )
{
    // if there are no players, unlock the server
    //
    if ( 0 == apiPlayersGetCount() ) {
        // Unlcok the server - restore to normal operation
        // Do not unlock if the variable state is "perm" (=2L) 
        //
        p2pSetL( "pigateway.p2p.lockOut", 0L );
        apiSaySys( "Unlocking the server due to zero players" );
        logPrintf( LOG_LEVEL_INFO, "pigateway", "Unlocking the server due to zero players" );
    }
    return 0;
}

//  ==============================================================================================
//  pigatewayClientAddCB
//
//  (Legacy - moved to SynthAdd counter part)
//
int pigatewayClientAddCB( char *strIn )
{

    // legacy:  do not use this function as of game revision 1.3.  Use SynthAddCB instead.
    //

    // char playerName[256], playerGUID[256], playerIP[256];
    // int alreadyKicked = 0;
    // 
    // rosterParsePlayerConn( strIn, 256, playerName, playerGUID, playerIP );

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayClientDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayInitCB( char *strIn )
{

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayMapChangeCB( char *strIn )
{
    unsigned long lockState;

    // Unlcok the server - restore to normal operation
    // Do not unlock if the variable state is "perm" (=2L) 
    //
    lockState = p2pGetL( "pigateway.p2p.lockOut", 0L );
    if ( lockState != 2L )  {
        p2pSetL( "pigateway.p2p.lockOut", 0L );
    }

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayGameStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pigatewayGameEndCB
//
//  Legacy:  Alarm function that exempt admin-kick during map change.  This is now obsolete
//  but code is still active due to unpredictability of the game server development.
//
int pigatewayGameEndCB( char *strIn )
{
    // setup a lockout for admin-only blocking (disable this function)
    // during map change so we don't accidentally kick existing 
    // players due random re-joining order
    //
    logPrintf( LOG_LEVEL_DEBUG, "pigateway", "Gateway Lockout is SET" );
    alarmReset( pigatewayConfig.aPtr, pigatewayConfig.gameChangeLockoutSec );

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayRoundStartCB( char *strIn )
{
    if ( 1L ==  p2pGetL( "pigateway.p2p.lockOut", 0L )) {
        apiSay( "REMINDER: Server is locked by Access Control." );
    }
    else if ( 2L ==  p2pGetL( "pigateway.p2p.lockOut", 0L )) {
        apiSay( "*Server PERM-LOCKED by Access Control!" );
    }

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayRoundEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pigatewayCapturedCB
//
//  The optional "minimum player name length" is checked whenever an objective is captured.
//  The reason this is done here, instead of during intiial player connection, is that the
//  "rcon kick" does not register well on many clients during the server-player negotiation 
//  phase, causing players to "timeout" instead of seeing the kick message "name too short."
//  By deferring the kick until end of capture, will increase likelihood the message is properly
//  displayed.
//  
//
#define PIGATEWAY_MAXROSTER          (16*1024)
#define PIGATEWAY_STEAMIDLEN         (17)    // length of steamID i.e., strlen("76560000000000005")

int pigatewayCapturedCB( char *strIn )
{
    static char rosterBuf[ PIGATEWAY_MAXROSTER ];
    char   *w;
    int    i, myLen;
    char   name[256], guid[256];

    if ( pigatewayConfig.minPlayerNameLength > 0 ) {

        strlcpy( rosterBuf, apiPlayersRoster( 1, "\011" ), PIGATEWAY_MAXROSTER );
 
        for ( i=0 ; ; i++ ){
            w = getWord( rosterBuf, i, "\011" );  // Fetch one player: "abcd[76560000000000005]"
            if ( w == NULL ) break;

            myLen = (int) strlen( w );
            if ( myLen == 0 ) break;

            if ( myLen < (PIGATEWAY_STEAMIDLEN + 2 + pigatewayConfig.minPlayerNameLength) ) {

                strlcpy( name, w, myLen-( PIGATEWAY_STEAMIDLEN+2)+1 );
                strlcpy( guid, & w[ strlen( name ) + 1], PIGATEWAY_STEAMIDLEN+1 );
            
                apiKickOrBan( 0, guid,  pigatewayConfig.minPlayerNameMsg ); 
                logPrintf( LOG_LEVEL_INFO, "pigateway", "Short Name Auto-kick ::%s::%s", name, guid );

            }
        }
    }
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigatewayPeriodicCB( char *strIn )
{
    static int tenSecondPolling = 0;

    if ( ++tenSecondPolling > 10 ) {
        // This is a redundant, safety code duplicated from DelCB callback.
        // if there are no players, unlock the server
        //
        tenSecondPolling = 0;

        if ( 0 == apiPlayersGetCount() ) {
            // Unlcok the server - restore to normal operation
            // Do not unlock if the variable state is "perm" (=2L) 
            //
            p2pSetL( "pigateway.p2p.lockOut", 0L );
            apiSaySys( "Unlocking the server due to zero players" );
            logPrintf( LOG_LEVEL_INFO, "pigateway", "Unlocking the server due to zero players at PeriodicCB " );
        }
    }

    return 0;
}


//  ==============================================================================================
//  pigatewayChatCB
//
//  This plugin is now watching for a short list of "troll" words typed into chat.  For now the
//  offending player is auto-kicked.  The list of words can be entered in sissm.cfg.  Empty
//  string "" will disable this feature.  Good candidate for this is racial slurs and social 
//  engineering hacking attempt.
//
int pigatewayChatCB( char *strIn )
{
    char clientID[512], chatLine[512], *w;
    int errCode, i = 0;

    if ( 0 != strlen( pigatewayConfig.chatTrollWords ) ) {
        if ( 0 == (errCode = rosterParsePlayerChat( strIn, 512, clientID, chatLine ))) {
            while (1 == 1 ) {
               w = getWord( pigatewayConfig.chatTrollWords, i++, " " );
               if ( w == NULL ) break;
               if ( 0 == strlen( w ) ) break;
               if (NULL != strcasestr( chatLine, w )) {
                    apiKickOrBan( 0, clientID, "Troll Detect: Generating Report" );
                    logPrintf( LOG_LEVEL_INFO, "pigateway", "Troll Detect Auto-kick ::%s::'%s'", clientID, chatLine );
               }
            }
        }
    }
    return 0;
}

//  ==============================================================================================
//  pigatewayInstallPlugin
//
//  This method is exported and is called from the main sissm module.
//
int pigatewayInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pigatewayInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pigatewayConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pigatewayClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pigatewayClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pigatewayInitCB );
    eventsRegister( SISSM_EV_RESTART,              pigatewayRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pigatewayMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pigatewayGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pigatewayGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pigatewayRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pigatewayRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pigatewayCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pigatewayPeriodicCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pigatewayClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pigatewayClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 pigatewayChatCB     );

    timeRestarted = apiTimeGet();
    return 0;
}


