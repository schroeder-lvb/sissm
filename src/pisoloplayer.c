//  ==============================================================================================
//
//  Module: PIREBOOTER
//
//  Description:
//  Sandstorm counterattack disabler for solo players
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

#include "pisoloplayer.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int  soloPlayerThreshold;              // # of players for 'solo', default 1.
    char multiWarning[CFS_FETCH_MAX];
    char soloWarning[CFS_FETCH_MAX];

} pisoloplayerConfig;


//  ==============================================================================================
//  pisoloplayerInitConfig
//
//  Read the configuration for this plugin
//
int pisoloplayerInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pisoloplayer.pluginstate" variable from the .cfg file
    pisoloplayerConfig.pluginState = (int) cfsFetchNum( cP, "pisoloplayer.pluginState", 0.0 );  // disabled by default

    // how players constitute "solo"?  Typically '1' but you may override to higher player count
    //
    pisoloplayerConfig.soloPlayerThreshold = (int) cfsFetchNum( cP, "pisoloplayer.soloPlayerThreshold", 1.0 ); 

    strlcpy( pisoloplayerConfig.soloWarning, 
        cfsFetchStr( cP, "pisoloplayer.soloWarning", "Counterattack disabled for a single player" ), CFS_FETCH_MAX);
    strlcpy( pisoloplayerConfig.multiWarning, 
        cfsFetchStr( cP, "pisoloplayer.multiWarning", "Counterattack enabled for multi-players" ), CFS_FETCH_MAX);

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  _singlePlayerNoCounter
//
//  Adjust the server if we are running single player or multi.  This routine is called
//  whenever a player connects or disconnects, as well as at start of game, round & objective.
// 
//
static int _singlePlayerNoCounter( void )
{
    if ( pisoloplayerConfig.soloPlayerThreshold >= apiPlayersGetCount() ) {
        apiGameModePropertySet( "DefendTimer", "1");
	apiSay( pisoloplayerConfig.soloWarning );
        logPrintf( LOG_LEVEL_INFO, "pisoloplayer", "Counterattack disabled for a single player" );
    }
    else {
        apiGameModePropertySet( "DefendTimer", "90");
	apiSay( pisoloplayerConfig.multiWarning );
        logPrintf( LOG_LEVEL_INFO, "pisoloplayer", "Counterattack enabled for multi-players" );
    }
    return 0;
}


//  ==============================================================================================
//  pisoloplayerClientAddCB
//
//  Check for solo/multi-player status when player is added.
//  
//
int pisoloplayerClientAddCB( char *strIn )
{
    _singlePlayerNoCounter();

    return 0;
}

//  ==============================================================================================
//  pisoloplayerClientDelCB
//
//  Check for solo/multi-player status when player disconnects
// 
//
int pisoloplayerClientDelCB( char *strIn )
{
    _singlePlayerNoCounter();

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pisoloplayerInitCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pisoloplayerRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pisoloplayerMapChangeCB
//
//  Check for solo/multi-player status at change of map.
//
//
int pisoloplayerMapChangeCB( char *strIn )
{
    _singlePlayerNoCounter();
    return 0;
}

//  ==============================================================================================
//  pisoloplayerGameStartCB
//
//  Check for solo/multi-player status at start of game.
//
//
int pisoloplayerGameStartCB( char *strIn )
{
    _singlePlayerNoCounter();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pisoloplayerGameEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pisoloplayerRoundStartCB
//
//  Check for solo/multi-player status at start of round.
//
//
int pisoloplayerRoundStartCB( char *strIn )
{
    _singlePlayerNoCounter();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pisoloplayerRoundEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pisoloplayerCapturedCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pisoloplayerPeriodicCB( char *strIn )
{
    return 0;
}



//  ==============================================================================================
//  ...
//
//  This method is exported and is called from the main sissm module.
//
int pisoloplayerInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pisoloplayerInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pisoloplayerConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pisoloplayerClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pisoloplayerClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pisoloplayerInitCB );
    eventsRegister( SISSM_EV_RESTART,              pisoloplayerRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pisoloplayerMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pisoloplayerGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pisoloplayerGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pisoloplayerRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pisoloplayerRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pisoloplayerCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pisoloplayerPeriodicCB );

    return 0;
}


