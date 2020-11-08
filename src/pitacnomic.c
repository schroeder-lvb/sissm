//  ==============================================================================================
//
//  Module: PITACNOMIC
//
//  Description:
//  Tactical Communication for No-Microphone Players.  This plugin rebroadcasts
//  common tactical phrases when a player types a matching double-letter commands
//  on in-game chat, such as 'aa', 'bb', etc.
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.11.22
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#define _GNU_SOURCE

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
#include "p2p.h"
#include "winport.h"

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "pitacnomic.h"

#define PITACNOMIC_MAXSHORTS    100

//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char shorts[PITACNOMIC_MAXSHORTS][CFS_FETCH_MAX];
    char livePlayersOnly[CFS_FETCH_MAX];

} pitacnomicConfig;


//  ==============================================================================================
//  pitacnomicInitConfig
//
//  Read parameters from the .cfg file 
//
int pitacnomicInitConfig( void )
{
    cfsPtr cP;
    int i;
    char varArray[CFS_FETCH_MAX];

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pitacnommic.pluginstate" variable from the .cfg file
    pitacnomicConfig.pluginState = (int) cfsFetchNum( cP, "pitacnomic.pluginState", 0.0 );  // disabled by default

    // Read the operator-specified "macro" (alias) sequence
    //
    for ( i=0; i<PITACNOMIC_MAXSHORTS; i++ ) {
        snprintf( varArray, 256, "pitacnomic.shorts[%d]", i );
        strlcpy( pitacnomicConfig.shorts[i],     cfsFetchStr( cP, varArray,  "" ),   CFS_FETCH_MAX);
        replaceDoubleColonWithBell( pitacnomicConfig.shorts[i] );
    }

    // Read the list string of two-letter-codes that only 'live' players can issue
    //
    strlcpy( pitacnomicConfig.livePlayersOnly, cfsFetchStr( cP, "pitacnomic.livePlayersOnly",  "" ),   CFS_FETCH_MAX);

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  pitacnomicClientAddCB
//
//  Add client event handler 
//  This only works reliably when EasyAC is on.  For slower but reliable event use
//  the Syntehtic-Add Callback.
//
int pitacnomicClientAddCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicClientDelCB
//
//  Del client event handler 
//  This only works reliably when EasyAC is on.  For slower but reliable event use
//  the Syntehtic-Del Callback.
//
int pitacnomicClientDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicInitCB
//
//  This callback is invoked whenever SISSM is started or reset.
//
int pitacnomicInitCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicRestartCB
//
//  This callback is invoked just before restart/reboot is issued to the game server.
//  Expect comms blackout for 10-30 seconds after this while the game server reboots.
//
int pitacnomicRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicMapChangeCB
//
//  This callback is invoked whenever a map change is detected.
//
int pitacnomicMapChangeCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicGameStartCB
//
//  This callback is invoked whenever a Game-Start event is detected.
//
int pitacnomicGameStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicGameEndCB
//
//  This callback is invoked whenever a Game-End event is detected.
//
int pitacnomicGameEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicRoundStartCB
//
//  This callback is invoked whenver a Round-Start event is detected.
//
int pitacnomicRoundStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicRoundEndCB
//
//  This callback is invoked whenever a End-of-round event is detected.
//
int pitacnomicRoundEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicCaptureCB
//
//  This callback is invoked whenever an objective is captured.  Note multiple call of this
//  may occur for each objective.  To generate a single event, following example filters
//  all but the first event within N seconds.
//
int pitacnomicCapturedCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicPeriodicCB
//
//  This callback is invoked at 1.0Hz rate (once a second).  Due to serial processing,
//  exact timing cannot be guaranteed.
//
int pitacnomicPeriodicCB( char *strIn )
{
    return 0;
}


//  ==============================================================================================
//  pitacnomicShutdownCB
//
//  This callback is invoked whenever a game shutdown is detected, typically as a result of 
//  restart request, or operator terminating the server.   This event is not a SISSM shutdown.
//
int pitacnomicShutdownCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicClientSynthDelCB
//
//  This is a slower but more reliable version of Player deletion event.  Unlike ClientDEL event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pitacnomicClientSynthDelCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicClientSynthAddCB
//
//  This is a slower but more reliable version of Player add event.  Unlike ClientADD event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pitacnomicClientSynthAddCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomicChatCB           
//
//  This callback is invoked whenever any player enters a text chat message.                  
//
//
//  Parse the log line 'say' chat log into originator GUID and the said text string
//  [2019.08.30-23.39.33:262][176]LogChat: Display: name(76561198000000001) Global Chat: !ver sissm
//  [2019.09.15-03.44.03:500][993]LogChat: Display: name(76561190000000002) Team 0 Chat: !v
//
int pitacnomicChatCB( char *strIn )
{
    int errCode = 0, i;
    char *typeObj, clientID[256], chatText[256];
    char *w, *v;

    logPrintf( LOG_LEVEL_INFO, "pitacnomic", "Client chat ::%s::", strIn );

    // check if this is a valid chat line 
    //
    if ( 0 == (errCode = rosterParsePlayerChat( strIn, 256, clientID, chatText ))) {
        if (( 2 == strlen( chatText ))) {
            chatText[2] = 0;

            // check if the command issuer is 'dead' -- if so, consult list of prohibited 
            // codes that can only be issued by players that are alive.
            //
            if (0 == apiIsPlayerAliveByGUID( clientID )) {  // if the issuer is "dead"

                 // check if this 2-letter-code is in the "live player only" list
                 // 
                 if ( NULL != strcasestr( pitacnomicConfig.livePlayersOnly, chatText ) ) {
                     // if so, invalidate the entered text (ignored by this plugin
                     //
                     chatText[0] = ' ';  chatText[1] = ' ';
                 }
            }

            // Resume regular processing of two-letter-code.  Do a table lookup
            // and send translated string to the player chat
            //
            for (i = 0; i<PITACNOMIC_MAXSHORTS; i++) {
                if ( 0 == strncmp( pitacnomicConfig.shorts[i], chatText, 2 )) {
                    // match is found on the table.  Parse for phrase to be
                    // sent to the game.
                    //
                    if (NULL != (w = getWord( pitacnomicConfig.shorts[i], 1, "\007" ) )) {
                        typeObj = rosterGetObjectiveType();
                        if ( NULL != strcasestr( typeObj, "capturable"  ) ) {
                            ;
                        }
                        if ( NULL != strcasestr( typeObj, "weaponcache" ) ) {
                            if (NULL != (v = getWord( pitacnomicConfig.shorts[i], 2, "\007" ) )) {
                                w = v;
                             }
                        }
                        apiSaySys( w );
                    }
                    break;
                }
            }       
        }
    }

    return errCode;
}

//  ==============================================================================================
//  pitacnomicSigtermCB           
//
//  This callback is invoked when SISSM is terminated abnormally (either by OS or operator
//  pressing ^C.  Since the game server is assumed running, plugins should take necessary action
//  to leave the server in 'playable state' in absense of SISSM.
//
int pitacnomicSigtermCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomic
//
//
int pitacnomicWinLose( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomic
//
//
int pitacnomicTravel( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  pitacnomic
//
//
int pitacnomicSessionLog( char *strIn )
{

    return 0;
}

//  ==============================================================================================
//  pitacnomic
//
//
int pitacnomicObjectSynth( char *strIn )
{
    return 0;
}


//  ==============================================================================================
//  ...
//
//  ...
//  This method is exported and is called from the main sissm module.
//
int pitacnomicInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pitacnomicInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pitacnomicConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pitacnomicClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pitacnomicClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pitacnomicInitCB );
    eventsRegister( SISSM_EV_RESTART,              pitacnomicRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pitacnomicMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pitacnomicGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pitacnomicGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pitacnomicRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pitacnomicRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pitacnomicCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pitacnomicPeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             pitacnomicShutdownCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pitacnomicClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pitacnomicClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 pitacnomicChatCB     );
    eventsRegister( SISSM_EV_SIGTERM,              pitacnomicSigtermCB  );
    eventsRegister( SISSM_EV_WINLOSE,              pitacnomicWinLose );
    eventsRegister( SISSM_EV_TRAVEL,               pitacnomicTravel );
    eventsRegister( SISSM_EV_SESSIONLOG,           pitacnomicSessionLog );
    eventsRegister( SISSM_EV_OBJECT_SYNTH,         pitacnomicObjectSynth );

    return 0;
}


