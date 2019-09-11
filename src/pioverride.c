//  ==============================================================================================
//
//  Module: PIOVERRIDE
//
//  Description:
//  Gameproperty Ruleset overrider e.g., controlling bot count for Frenzy
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

#include "pioverride.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled
    char cvar[10][CFS_FETCH_MAX];          // array of 10 [<gamemodeproperties> <value>]
    int  pokeEveryRound;

} pioverrideConfig;


//  ==============================================================================================
//  pioverrideInitConfig
//
//  Read parameters from the configuration file
// 
//
int pioverrideInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pioverride.pluginstate" variable from the .cfg file
    pioverrideConfig.pluginState = (int) cfsFetchNum( cP, "pioverride.pluginState", 0.0 );  // disabled by default

    // array of 10 [<gamemodeproperty> <value>] pairs
    //
    strlcpy( pioverrideConfig.cvar[0],  cfsFetchStr( cP, "pioverride.cvar[0]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[1],  cfsFetchStr( cP, "pioverride.cvar[1]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[2],  cfsFetchStr( cP, "pioverride.cvar[2]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[3],  cfsFetchStr( cP, "pioverride.cvar[3]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[4],  cfsFetchStr( cP, "pioverride.cvar[4]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[5],  cfsFetchStr( cP, "pioverride.cvar[5]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[6],  cfsFetchStr( cP, "pioverride.cvar[6]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[7],  cfsFetchStr( cP, "pioverride.cvar[7]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[8],  cfsFetchStr( cP, "pioverride.cvar[8]",  "" ), CFS_FETCH_MAX );
    strlcpy( pioverrideConfig.cvar[9],  cfsFetchStr( cP, "pioverride.cvar[9]",  "" ), CFS_FETCH_MAX );

    // if pokeEveryRound = 1, then this plugin will program the gamemodeproperty to 
    // the server at start of every round.  Otherwise, it is only programmed at start of 
    // every game.
    //
    pioverrideConfig.pokeEveryRound = (int) cfsFetchNum( cP, "pioverride.pokeEveryRound", 0 );

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  _pokeCvar
//
//  Walk through the list of gamemodeproperties in the configuration file, and  
//  send it via the RCON path.  This method is called at start of game, and optionally
//  at start of every round, depending on what is specified in the configuration file.
//  
//
static void _pokeCvar( void )
{
    int i;
    char cvarStr[256], cvarVal[256];
    char *w1, *w2;

    for (i=0; i<10; i++) {

        w1 = getWord( pioverrideConfig.cvar[i], 0, " " );
        w2 = getWord( pioverrideConfig.cvar[i], 1, " " );

	if (( w1 != NULL ) && ( w2 != NULL )) {
            strlcpy( cvarStr, w1, 256);
            strlcpy( cvarVal, w2, 256);

	    if ( (0 != strlen(cvarStr)) && (0 != strlen(cvarVal)) ) {
                apiGameModePropertySet( cvarStr, cvarVal );
	        logPrintf( LOG_LEVEL_INFO, "pioverride", "Setting ::%s:: to ::%s::", cvarStr, cvarVal );
	    }
        }
    }
    return;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideClientAddCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideClientDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideInitCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideMapChangeCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pioverrideGameStartCB
//
//  Update the override gamemodeproperties at start of game.
// 
//
int pioverrideGameStartCB( char *strIn )
{
     _pokeCvar();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideGameEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pioverrideRoundStartCB
//
//  Optionally update the override gamemodeproperties at start of game.
//
int pioverrideRoundStartCB( char *strIn )
{
    if ( pioverrideConfig.pokeEveryRound ) 
        _pokeCvar();

    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideRoundEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverrideCapturedCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int pioverridePeriodicCB( char *strIn )
{
    return 0;
}



//  ==============================================================================================
//  pioverrideInstallPlugins
//
//  This method is exported and is called from the main sissm module.
//
int pioverrideInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pioverrideInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pioverrideConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pioverrideClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pioverrideClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pioverrideInitCB );
    eventsRegister( SISSM_EV_RESTART,              pioverrideRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pioverrideMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pioverrideGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pioverrideGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pioverrideRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pioverrideRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pioverrideCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pioverridePeriodicCB );
    return 0;
}


