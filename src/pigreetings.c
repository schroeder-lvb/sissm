//  ==============================================================================================
//
//  Module: PIGREETINGS
//
//  Description:
//  Player connection/disconnection in-game notifications
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================


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

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "pigreetings.h"


//  ==============================================================================================
//  Data definition 
//

#define PIGREETINGS_RESTART_LOCKOUT_SEC   (30)     // nSecs do NOT display connection after reboot

static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int  showConnDisconn;
    int  showRules;

    char serverGreetings[2][CFS_FETCH_MAX];
    char serverRules[10][CFS_FETCH_MAX];

    char incognitoGUID[CFS_FETCH_MAX];

    char connected[CFS_FETCH_MAX];
    char connectedAsAdmin[CFS_FETCH_MAX];
    char disconnected[CFS_FETCH_MAX];

} pigreetingsConfig;

static unsigned long timeRestarted = 0L;

//  ==============================================================================================
//  _isIncognito
//
//  Check if the connecting client is on the incognito list.  This module does not 
//  announce incognito players on connection/disconnection.
// 
static int _isIncognito( char *playerGUID ) 
{
    int isIncognito = 0;

    if ( NULL != strstr( pigreetingsConfig.incognitoGUID, playerGUID )) isIncognito = 1;
    return isIncognito;
}



//  ==============================================================================================
//  pigreetingsInitConfig
//
//  Plugin initialization
//
int pigreetingsInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pigreetings.pluginstate" variable from the .cfg file
    //
    pigreetingsConfig.pluginState = (int) cfsFetchNum( cP, "pigreetings.pluginState", 0.0 );  // disabled by default

    // read the connection/disconnection display enabler
    //
    pigreetingsConfig.showConnDisconn = (int) cfsFetchNum( cP, "pigreetings.showconndisconn", 1.0 );

    // read the 2-line greeting messages
    // 
    strlcpy( pigreetingsConfig.serverGreetings[0], cfsFetchStr( cP, "pigreetings.servergreetings[0]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverGreetings[1], cfsFetchStr( cP, "pigreetings.servergreetings[1]", "" ), CFS_FETCH_MAX );

    // read the rules display enabler
    //
    pigreetingsConfig.showRules       = (int) cfsFetchNum( cP, "pigreetings.showrules", 1.0 );

    // read the 10 rule lines
    // 
    strlcpy( pigreetingsConfig.serverRules[0], cfsFetchStr( cP, "pigreetings.serverrules[0]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[1], cfsFetchStr( cP, "pigreetings.serverrules[1]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[2], cfsFetchStr( cP, "pigreetings.serverrules[2]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[3], cfsFetchStr( cP, "pigreetings.serverrules[3]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[4], cfsFetchStr( cP, "pigreetings.serverrules[4]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[5], cfsFetchStr( cP, "pigreetings.serverrules[5]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[6], cfsFetchStr( cP, "pigreetings.serverrules[6]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[7], cfsFetchStr( cP, "pigreetings.serverrules[7]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[8], cfsFetchStr( cP, "pigreetings.serverrules[8]", "" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.serverRules[9], cfsFetchStr( cP, "pigreetings.serverrules[9]", "" ), CFS_FETCH_MAX );

    // read the list of incognito GUIDs
    //
    strlcpy( pigreetingsConfig.incognitoGUID, cfsFetchStr( cP, "pigreetings.incognitoGUID", "" ), CFS_FETCH_MAX );

    // read 'connected' and 'disconnected' (as in 'ZZZZ connected' for localization)
    // 
    strlcpy( pigreetingsConfig.connected, cfsFetchStr( cP, "pigreetings.connected", "connected" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.disconnected, cfsFetchStr( cP, "pigreetings.disconnected", "disconnected" ), CFS_FETCH_MAX );
    strlcpy( pigreetingsConfig.connectedAsAdmin, cfsFetchStr( cP, "pigreetings.connectedasadmin", "[admin] connected" ), CFS_FETCH_MAX );

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  pigreetingsClientAddCB
//
//  Callback for add-client event based on logfile.  Game post 1.3, the player name is realiable
//  but GUID/IP are not -- but this plugin only needs player names for announcement.
// 
//
int pigreetingsClientAddCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerConn( strIn, 256, playerName, playerGUID, playerIP );
    logPrintf( LOG_LEVEL_CRITICAL, "pigreetings", "Add Client ::%s::%s::%s::", playerName, playerGUID, playerIP );

    // if (!_isIncognito( playerGUID )) {
    //     apiSay( "%s %s [%d]", playerName, pigreetingsConfig.connected, apiPlayersGetCount() );
    // }

    return 0;
}

//  ==============================================================================================
//  pigreetingsClientDelCB
//
//  Game post 1.3, disconnect client notification is only comprised of IP#, so it is no longer
//  useful for disconnect announcement.  This method is disabled.  Use SynthDel instead.
//
int pigreetingsClientDelCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerDisConn( strIn, 256, playerName, playerGUID, playerIP );
    // logPrintf( LOG_LEVEL_DEBUG, "pigreetings", "Del Client ::%s::%s::%s::", playerName, playerGUID, playerIP );
    // apiSay ( "%s %s [%d]", playerName, pigreetingsConfig.disconnected, apiPlayersGetCount() );

    return 0;
}

//  ==============================================================================================
//  pigreetingsClientSynthDelCB
//
//  Use SyntheticDel event to reliably announced disconnecting players.  SyntheticDel events
//  are derived from two consecutive samples of player roster from RCON, and 'core' 
//  synthetically creates change events for call-backs.
//  ...
//
//  strIn:  in the format: "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pigreetingsClientSynthDelCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthDisConn( strIn, 256, playerName, playerGUID, playerIP );
    // rosterParsePlayerDisConn( strIn, playerName, playerGUID, playerIP );
    // logPrintf( LOG_LEVEL_CRITICAL, "pigreetings", "SynDel Raw ::%s::", strIn );
    logPrintf( LOG_LEVEL_CRITICAL, "pigreetings", "SynDel Client ::%s:: GUID ::%s:: IP ::%s::", playerName, playerGUID, playerIP );
    // 
    if (!_isIncognito( playerGUID )) {
        if ( 0 != strlen( pigreetingsConfig.disconnected ) ) 
            apiSay( "%s %s [%d]", playerName, pigreetingsConfig.disconnected, apiPlayersGetCount() );
        logPrintf( LOG_LEVEL_CRITICAL, "pigreetings", "%s disconnected [%d]", playerName, apiPlayersGetCount() );
    }

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
//  strIn:  in the format: "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pigreetingsClientSynthAddCB( char *strIn )
{
    char playerName[256], playerGUID[256], playerIP[256];

    rosterParsePlayerSynthConn( strIn, 256, playerName, playerGUID, playerIP );
    if (!_isIncognito( playerGUID )) {
        if ( (apiTimeGet() - timeRestarted) > PIGREETINGS_RESTART_LOCKOUT_SEC ) {   // check if we are restarting
       
           if  ( apiIsAdmin( playerGUID ) && (0 != strlen( pigreetingsConfig.connectedAsAdmin)) ) 
               apiSay( "%s %s [%d]", playerName, pigreetingsConfig.connectedAsAdmin, apiPlayersGetCount() );
           else if ( 0 != strlen( pigreetingsConfig.connected ))
               apiSay( "%s %s [%d]", playerName, pigreetingsConfig.connected, apiPlayersGetCount() );

        }
        logPrintf( LOG_LEVEL_CRITICAL, "pigreetings", "SynAdd Client ::%s::%s::%s::", playerName, playerGUID, playerIP );
    }
    else {
        logPrintf( LOG_LEVEL_CRITICAL, "pigreetings", "SynAdd Client [incognito] ::%s::%s::%s::", playerName, playerGUID, playerIP );
    }
    return 0;
}


//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigreetingsInitCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigreetingsRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigreetingsMapChangeCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigreetingsGameStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigreetingsGameEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pigreetingsRoundStartCB
//
//  Start of round - announced two lines of "welcome" messages from sissm configuration file.
// 
//
int pigreetingsRoundStartCB( char *strIn )
{
    if ( 0 != strlen(pigreetingsConfig.serverGreetings[0] )) 
        apiSay( "%s", pigreetingsConfig.serverGreetings[0] );
    if ( 0 != strlen(pigreetingsConfig.serverGreetings[1] )) 
        apiSay( "%s", pigreetingsConfig.serverGreetings[1] );
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pigreetingsRoundEndCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pigreetings", "Round End Event" );
    return 0;
}

//  ==============================================================================================
//  pigreetingsCaptureCB
//
//  Start of new objective - display one line of "rules" in round-robin fashion.
//  Currently, multiple start of objective events in rapid succession so a 10 second
//  filtering is done in this block of code.
//
int pigreetingsCapturedCB( char *strIn )
{
    static long int lastTimeCaptured = 0L;
    static int lastIndex = 0;

    // only display rules when enabled (commonly disabled for private servers
    //
    if ( pigreetingsConfig.showRules ) {

        // system generates multiple 'captured' report, so it is necessary
        // to add a 10-second window filter to make sure only one gets fired off.
        //
        if ( lastTimeCaptured + 10L < apiTimeGet() ) {
            if ( 0 != strlen(pigreetingsConfig.serverRules[lastIndex] )) 
                apiSay( "%s", pigreetingsConfig.serverRules[lastIndex] );
	    lastIndex = ( lastIndex + 1 ) % 10;
            lastTimeCaptured = apiTimeGet();
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
int pigreetingsPeriodicCB( char *strIn )
{
    return 0;
}



//  ==============================================================================================
//  ...
//
//  This method is exported and is called from the main sissm module.
//
int pigreetingsInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pigreetingsInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pigreetingsConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pigreetingsClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pigreetingsClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pigreetingsInitCB );
    eventsRegister( SISSM_EV_RESTART,              pigreetingsRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pigreetingsMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pigreetingsGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pigreetingsGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pigreetingsRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pigreetingsRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pigreetingsCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pigreetingsPeriodicCB );

    // Synthetic Delete - this one is generated by RCON roster
    // poller, since player ident from logfile informatino is non-deterministic.
    //
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pigreetingsClientSynthDelCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pigreetingsClientSynthAddCB );

    // Remember restart time
    //
    timeRestarted = apiTimeGet();   
  
    return 0;
}


