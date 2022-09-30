//  ==============================================================================================
//
//  Module: PIDYNBOTS
//
//  Description:
//      Dynamic AI (bot) Adjustments - automatically re-scales bot count range, reprograms
//      difficulty level according to side/map/objective.
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.11.13
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

#include "p2p.h"
#include "bsd.h"
#include "log.h"
#include "events.h"
#include "cfs.h"
#include "util.h"
#include "alarm.h"
#include "winport.h"   // strcasestr

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "pidynbots.h"

#define PIDYN_MAXADJUSTERS  (128)
#define PIDYN_MAXMODES      (16)

//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int MinimumEnemies; 
    int MaximumEnemies; 
    int MaxPlayersToScaleEnemyCount;
    int MaxBotsCrashProtect;
    double AIDifficulty;                                 
    char Adjuster[PIDYN_MAXADJUSTERS][CFS_FETCH_MAX];      // side/map/objective nbots adjustments 
    char modeAdjust[PIDYN_MAXMODES][CFS_FETCH_MAX];     // bot adjustments (offsets) per game mode
    char BotBias[CFS_FETCH_MAX];                         // nbots count vs humans adjustment table

    int enableObjAdj;                       // 1 to enable objective adj; turn off if using webmin
    int enableDisconnAdj;                         // 1 to enable AI adjust when player disconnects
    int enableConnAdj;                               // 1 to enable AI adjust when player connects

    char showInGame[CFS_FETCH_MAX];                        // set to "" to disable in-game display

} pidynbotsConfig;

// bot count offsets for this gamemode.
//
int pidynMinimumOffset = 0, pidynMmaximumOffset = 0;

//  ==============================================================================================
//  pidynbotsInitConfig
//
//  Read parameters from the .cfg file 
//
int pidynbotsInitConfig( void )
{
    cfsPtr cP;
    int i;
    char varArray[256];

    cP = cfsCreate( sissmGetConfigPath() );

    // read "pidynbots.pluginstate" variable from the .cfg file
    pidynbotsConfig.pluginState = (int) cfsFetchNum( cP, "pidynbots.pluginState", 0.0 );  // disabled by default

    pidynbotsConfig.MinimumEnemies  = (int) cfsFetchNum( cP, "pidynbots.MinimumEnemies",  3.0 );  // minimumenemies
    pidynbotsConfig.MaximumEnemies  = (int) cfsFetchNum( cP, "pidynbots.MaximumEnemies", 18.0 );  // maximumenemies
    pidynbotsConfig.MaxPlayersToScaleEnemyCount = (int) cfsFetchNum( cP, "pidynbots.MaxPlayerToScaleEnemyCount", 10.0);
    pidynbotsConfig.AIDifficulty = (double) cfsFetchNum( cP, "pidynbots.AIDifficulty",    0.5 );  // AIDifficulty

    pidynbotsConfig.enableObjAdj         = (int) cfsFetchNum( cP, "pidynbots.enableObjAdj", 0.0 );  
    pidynbotsConfig.enableDisconnAdj     = (int) cfsFetchNum( cP, "pidynbots.enableDisconnAdj", 0.0 );  
    pidynbotsConfig.enableConnAdj        = (int) cfsFetchNum( cP, "pidynbots.enableConnAdj", 0.0 );  
    pidynbotsConfig.MaxBotsCrashProtect  = (int) cfsFetchNum( cP, "pidynbots.MaxBotsCrashProtect", 200.0 ); 

    strlcpy( pidynbotsConfig.showInGame, cfsFetchStr( cP, "pidynbots.showInGame", "" ), CFS_FETCH_MAX );  

    // Read the adjustment table
    //
    for ( i=0; i<PIDYN_MAXADJUSTERS; i++ ) {
        snprintf( varArray, 256, "pidynbots.Adjuster[%d]", i );
        strlcpy( pidynbotsConfig.Adjuster[i], cfsFetchStr( cP, varArray,  "" ), CFS_FETCH_MAX);
    }

    // Read the game mode adjustment offset table
    //
    for ( i=0; i<PIDYN_MAXMODES; i++ ) {
        snprintf( varArray, 256, "pidynbots.modeAdjust[%d]", i );
        strlcpy( pidynbotsConfig.modeAdjust[i], cfsFetchStr( cP, varArray,  "" ), CFS_FETCH_MAX);
        logPrintf( LOG_LEVEL_INFO, "pidynbots", "Reading GameMode BotOffset Table: ::%d::%s::", i, pidynbotsConfig.modeAdjust[i] ); 
    }

    // Read the botbias table
    //
    strlcpy( pidynbotsConfig.BotBias, cfsFetchStr( cP, "pidynbots.botbias",  "" ), CFS_FETCH_MAX);

    cfsDestroy( cP );

    return 0;
}

//  ==============================================================================================
//  _modeBotCountOffset
//
//  Routine to be called after map change/start of game, to determine game mode,
//  and lookup bot count offset adjstment values for minimumenemies and maximumenemies
//
static int _modeBotCountOffset( int *minimumOffset, int *maximumOffset )
{
    static char gameModeCurrent[ API_ROSTER_STRING_MAX ];
    char wMin[16], wMax[16], *w;
    int  iMin,     iMax;
    int  i, errCode = 1;

    *minimumOffset = 0;
    *maximumOffset = 0;

    // Get current game mode: "checkpoint", "hardcorecheckpoint"....
    //
    strlcpy( gameModeCurrent, apiGameModePropertyGet( "gamemodetagname" ), API_ROSTER_STRING_MAX );
    if ( 0 != strlen( gameModeCurrent ) ) 
        logPrintf( LOG_LEVEL_INFO, "pidynbots", "GameMode is ::%s::", gameModeCurrent ); 
    else 
        logPrintf( LOG_LEVEL_WARN, "pidynbots", "** Unable to read; current GameMode" );

    // Look for a match in gamemode vs. the supported offsets table
    //
    if ( 0 != strlen( gameModeCurrent ) )  {
        for ( i = 0; i < PIDYN_MAXMODES; i++ ) {
            w = getWord( pidynbotsConfig.modeAdjust[i], 0, " " );
            if ( w == NULL )        break;
            if ( strlen( w ) == 0 ) break;

            strclr( wMin ); strclr( wMax );
           
            if ( 0 == strcasecmp( w, gameModeCurrent ) ) {  // gamemode match
                w = getWord( pidynbotsConfig.modeAdjust[i], 1, " " );
                if ( w != NULL ) strlcpy( wMin, w, 16 );
                w = getWord( pidynbotsConfig.modeAdjust[i], 2, " " );
                if ( w != NULL ) strlcpy( wMax, w, 16 );
                
                if ( 1 == sscanf( wMin, "%d", &iMin )) {
                    if ( 1 == sscanf( wMax, "%d", &iMax )) {
                        *minimumOffset = iMin;
                        *maximumOffset = iMax;
                        logPrintf( LOG_LEVEL_INFO, "pidynbots", "GameMode ::%s:: BotCount Offset for Min/Max is %d and %d", gameModeCurrent, iMin, iMax );
                        errCode = 0;
                        break;
                    }
                }
            }
        } 
    }
    if ( errCode ) {
        logPrintf( LOG_LEVEL_WARN, "pidynbots", "Unable to parse GameMode BotCount Offset in sissm.cfg" );
    }

    return errCode;   
}


//  ==============================================================================================
//  _botScale
// 
//  Compute number of bots (y) given # of humans (x)
//
static unsigned int _botScale( unsigned int currHuman, 
    unsigned int maxBots, unsigned int minBots,
    unsigned int maxHuman, unsigned int minHuman )
{
    int errCode = 0;
    unsigned int _currHuman = currHuman;
    double slope;
    unsigned int currBots = 1;

    // check for invalid entry by the operator
    //
    if (0 == errCode)
        if ( maxBots < minBots )    errCode = 1;    // check neg slope
    if (0 == errCode)
        if ( maxHuman <= minHuman ) errCode = 1;    // check neg slope & divide-by-zero

    if (0 == errCode) {

        // clip the human counts to given limits
        //
        if ( _currHuman < minHuman ) _currHuman = minHuman;
        if ( _currHuman > maxHuman ) _currHuman = maxHuman;

        // compute the 2-point linear formula
        // y = f(x) = ((y1-y0) / (x1-x0)) * (x-x0) + y1;
        //
        slope = (( (double) maxBots - (double) minBots ) / ( (double) maxHuman - (double) minHuman ));
        currBots = (unsigned int ) (slope * ( _currHuman - minHuman ) + minBots);
    }

    // on error, this routine returns 'safe' bot value of '1'
    //
    return( currBots );
}

//  ==============================================================================================
//  _inRepsawnCoopMode
// 
//  Returns non-zero value if the server is in respawn mode (bBots=True).  To reduce RCON
//  traffic, caching method is used.  Rcon is only used when refresh is non-zero.
// 
static int _inRespawnCoopMode( int refresh )
{
    int inRespawnMode = 0;
    char *s1;
    static int lastMode = -1;

    if ( (lastMode == -1) || refresh ) {
        s1 = apiGameModePropertyGet( "bbots" );
        if ( NULL != strstr( s1, "True" ) ) inRespawnMode = 1;
        lastMode = inRespawnMode;
    }
    inRespawnMode = lastMode;

    return( inRespawnMode );
}

//  ==============================================================================================
//  _computeBotParams  
//
//  Reconfigure the AI parameters (minimumenemies, maximumenemies, soloenemies, aidifficulties)
//  for both standard checkpoint (bBots=False) and instant respawn variant (bBots=True);
//
static int _computeBotParams( int refresh )
{
    int minBots, maxBots, i;
    unsigned int currHuman, currBots, minHuman, maxHuman;
    double aiDifficulty;
    char *objName, *mapName, objLetter;
    char strBuf[256], *w, *v;
    int adjust = 0, errCode = 0, botBias, offsetIndex;

    // default parameters
    // 
    minBots      = pidynbotsConfig.MinimumEnemies;
    maxBots      = pidynbotsConfig.MaximumEnemies;
    minHuman     = 1;
    maxHuman     = pidynbotsConfig.MaxPlayersToScaleEnemyCount;
    currHuman    = rosterCount();

    // a wedge code to disable this plugin if non-Checkpoint mode is running
    //
    v = apiGetServerMode();
    if (! ((0==strcmp( v, "checkpoint" )) || (0==strcmp( v, "hardcore"))) )  return 0;

#if 0 
    // DEBUG tap for simulating humans joining and leaving - take out this block later
    static int currHuman_static = 1;
    int currHuman_tmp;
    if ( 0 == debugPoke( "delme.txt", &currHuman_tmp ) ) {
        currHuman_static = currHuman_tmp;
    }
    currHuman = currHuman_static;
    printf("\nDEBUG pidynbots: override to %d\n", currHuman );
    apiSay("pidynbots: override to %d humans", currHuman );
#endif

    aiDifficulty = pidynbotsConfig.AIDifficulty;

    // get current side, map, and objective
    //
    objName   = rosterGetObjective();                    // e.g., "OCCheckpoint_A"
    mapName   = rosterGetMapName();                      // e.g., "Farmhouse_Checkpoint_Security"
    objLetter = rosterGetCoopObjectiveLetter();          // e.g., 'A', 'B', 'C', ... ' '

    logPrintf( LOG_LEVEL_INFO, "pidynbots", "ObjName ::%s:: MapName ::%s:: objLetter ::%c::", 
        objName, mapName, objLetter );

    // search the table if override is found (look for matching map name)
    //
    for (i=0; i<PIDYN_MAXADJUSTERS; i++) {

        if ( NULL != strcasestr( pidynbotsConfig.Adjuster[i], mapName )) {

            // get min/max/dif values 
            //
            if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], 1, " " ))) 
                errCode |= ( 1 != sscanf( w, "%d", &minBots ));
            if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], 2, " " ))) 
                errCode |= ( 1 != sscanf( w, "%d", &maxBots ));
            if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], 3, " " ))) 
                errCode |= ( 1 != sscanf( w, "%lf", &aiDifficulty ));

            // if override was found, look for per-objective adjustments
            //
            if (( !errCode  ) && ( (objLetter >= 'A') && (objLetter <= 'Z') )) {
                offsetIndex = objLetter - 'A';
                offsetIndex += 4;        
                if ( NULL != (w = getWord( pidynbotsConfig.Adjuster[i], offsetIndex, " " ))) {
                    errCode |= ( 1 != sscanf( w, "%d", &adjust ));
                    if ( !errCode ) maxBots += adjust;
                }
            }
            break;
        }
    }

    // poke the new values
    //
    if ( !errCode )  {

        // Update the bot difficulty if it was not overridden by the 
        // operator (picladmin)
        //
        if ( 0L == p2pGetL( "picladmin.p2p.botAdminControlDifficulty", 0L )) {  // check if in-game admin took control
            logPrintf( LOG_LEVEL_INFO, "pidynbots", "Adjusting bot difficulty %lf", aiDifficulty );
            snprintf( strBuf, 256, "%lf", aiDifficulty );
                 apiGameModePropertySet( "aidifficulty",   strBuf ) ;

            // if enabled in config, update the players by showing new params in-game
            //
            if ( 0 != strlen( pidynbotsConfig.showInGame ) ) {
                apiSay("%s Difficulty %6.3lf", pidynbotsConfig.showInGame, aiDifficulty*10.0 );
            }
        }
        
        // Update the bot count if it was not overridden by the 
        // operator (picladmin)
        //
        if ( 0L == p2pGetL( "picladmin.p2p.botAdminControl", 0L )) {  // check if in-game admin took control
            logPrintf( LOG_LEVEL_INFO, "pidynbots", "Adjusting bots %d:%d" , minBots, maxBots );

            // Apply BotBias - this changes min/maxenemies by a specified offset in function of 
            // number of human players are present
            //
            botBias = 0;
            if ( NULL != (w = getWord( pidynbotsConfig.BotBias, currHuman, " " ))) 
               if ( 1 != sscanf( w, "%d", &botBias )) botBias = 0;

            // validate to keep the server from crashing
            // 
            minBots += botBias;                           // offset from bias table
            maxBots += botBias;
            minBots += pidynMinimumOffset;                // offset from game mode table
            maxBots += pidynMmaximumOffset;

            if ( maxBots > pidynbotsConfig.MaxBotsCrashProtect )  
                maxBots = pidynbotsConfig.MaxBotsCrashProtect;
            if ( minBots < 1  )      minBots =  1;
            if ( minBots > maxBots ) maxBots = minBots; 

            snprintf( strBuf, 256, "%d", minBots );
                apiGameModePropertySet( "minimumenemies", strBuf );
            snprintf( strBuf, 256, "%d", maxBots );
                apiGameModePropertySet( "maximumenemies", strBuf );

            // for instant-respawn mode, it uses SoloPlayer set of parameters.  Compute
            // the scaled bots linearly here and poke in the value;
            //
            currBots =  _botScale( currHuman, maxBots, minBots, maxHuman, minHuman );
            snprintf( strBuf, 256, "%d", currBots );
                apiGameModePropertySet( "soloenemies", strBuf );

            // apiSay("Bias %d Bots %d:%d:%d Humans %d", botBias, minBots, maxBots, currBots, currHuman);
            // printf("\n****Bias %d Bots %d:%d:%d Humans %d***\n", botBias, minBots, maxBots, currBots, currHuman);

            // if enabled in config, update the players by showing new params in-game
            //
            if ( 0 != strlen( pidynbotsConfig.showInGame ) ) {
                apiSay("%s Count %d:%d [%d]", pidynbotsConfig.showInGame, minBots*2, maxBots*2, currBots );
            }

        }

        // Admin Bot override is "on", but still must process if instand respawn mode to emulate
        // dynamic bot changes
        //
        else {
           
            // If "respawnMode" AND "not fixed bot" then, recompute the dynamic scaling for this objective
            // Poke the resulting value to soloenemies
            //
            if ( _inRespawnCoopMode( refresh ) ) {

                 // read gamemodeproperty for minenemies and maximumenemies
                 // convert to integer for math
                 //
                 if ( 1 != sscanf( apiGameModePropertyGet( "minimumenemies" ), "%d", &minBots )) errCode = 1;
                 if ( 1 != sscanf( apiGameModePropertyGet( "maximumenemies" ), "%d", &maxBots )) errCode = 1;

                 if ( 0 == errCode ) {
                     // program currBots to soloenemies
                     // 
                     if ( minBots != maxBots )  {
                         currBots =  _botScale( currHuman, maxBots, minBots, maxHuman, minHuman );
                         snprintf( strBuf, 256, "%d", currBots );

                         apiGameModePropertySet( "soloenemies", strBuf );

                         logPrintf( LOG_LEVEL_INFO, "pidynbots", 
                             "Adjusting bots respawn mode with operator override params %d:%d [%d] difficulty %lf", 
                             minBots, maxBots, currBots, aiDifficulty );

                         // if enabled in config, update the players by showing new params in-game
                         //
                         if ( 0 != strlen( pidynbotsConfig.showInGame ) ) {
                             apiSay("%s Count %d:%d [%d]", pidynbotsConfig.showInGame, minBots*2, maxBots*2, currBots );
                         }
                     }
                 }
             }
             else {
                 logPrintf( LOG_LEVEL_INFO, "pidynbots", "Adjusting bots bypassed due to Admin override %d:%d difficulty %lf", 
                     minBots, maxBots, aiDifficulty );
             }
        }

    }
    return 0;  
}



//  ==============================================================================================
//  Unused Functions
//
int pidynbotsClientAddCB( char *strIn ) { return 0; }
int pidynbotsClientDelCB( char *strIn ) { return 0; }
int pidynbotsChatCB( char *strIn )      { return 0; }


//  ==============================================================================================
//  pidynbotsCapturedCB 
//
//  This callback is invoked at 1Hz periodic rate.  It is used to handle P2P signaling events
//
int pidynbotsCapturedCB( char *strIn )  
{   
    return 0; 
}



//  ==============================================================================================
//  pidynbotsPeriodic
//
//  This callback is invoked at 1Hz periodic rate.  It is used to handle P2P signaling events
//
int pidynbotsPeriodicCB( char *strIn )  
{
    int sigBotScaleChanged;

    // Signal Handler FROM picladmin
    // 
    //
    sigBotScaleChanged = p2pGetL( "pidynbots.p2p.sigBotScaled", 0L ); 
    if ( sigBotScaleChanged ) {

        // Clear the signal
        p2pSetL( "pidynbots.p2p.sigBotScaled", 0L ); 
        // Force an update
        _computeBotParams( 0 );
    }
    return 0;
}

//  ==============================================================================================
//  pidynbotsInitCB
//
//  This callback is invoked whenever SISSM is started or reset.
//
int pidynbotsInitCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Init Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsRestartCB
//
//  This callback is invoked just before restart/reboot is issued to the game server.
//  Expect comms blackout for 10-30 seconds after this while the game server reboots.
//
int pidynbotsRestartCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Restart Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsMapChangeCB
//
//  This callback is invoked whenever a map change is detected.
//
int pidynbotsMapChangeCB( char *strIn )
{
    // char mapName[256];
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Map Change Event ::%s::", strIn );
    // rosterParseMapname( strIn, 256, mapName );
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Map name ::%s::", mapName );
    return 0;
}

//  ==============================================================================================
//  pidynbotsGameStartCB
//
//  This callback is invoked whenever a Game-Start event is detected.
//
int pidynbotsGameStartCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Game Start Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsGameEndCB
//
//  This callback is invoked whenever a Game-End event is detected.
//
int pidynbotsGameEndCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Game End Event ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsRoundStartCB
//
//  This callback is invoked whenver a Round-Start event is detected.
//
int pidynbotsRoundStartCB( char *strIn )
{
    logPrintf( LOG_LEVEL_INFO, "pidynbots", "Round Start Event ::%s::", strIn );

    // Compute offset to botcounts per different game mode
    // if parse error is detected, run wtih '0' for now
    // 
    if ( _modeBotCountOffset( &pidynMinimumOffset, &pidynMmaximumOffset )  ) {
        pidynMinimumOffset = 0;
        pidynMmaximumOffset = 0;
    } 

    _computeBotParams( 1 );

    return 0;
}

//  ==============================================================================================
//  pidynbotsRoundEndCB
//
//  This callback is invoked whenever a End-of-round event is detected.
//
int pidynbotsRoundEndCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Round End Event ::%s::", strIn );
    return 0;
}


//  ==============================================================================================
//  pidynbotsShutdownCB
//
//  This callback is invoked whenever a game shutdown is detected, typically as a result of 
//  restart request, or operator terminating the server.   This event is not a SISSM shutdown.
//
int pidynbotsShutdownCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_DEBUG, "pidynbots", "Shutdown Callback ::%s::", strIn );
    return 0;
}

//  ==============================================================================================
//  pidynbotsClientSynthDelCB
//
//  This is a slower but more reliable version of Player deletion event.  Unlike ClientDEL event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHDEL~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pidynbotsClientSynthDelCB( char *strIn )
{
    // static char playerName[256], playerGUID[256], playerIP[256];
    // rosterParsePlayerSynthDisConn( strIn, 256, playerName, playerGUID, playerIP );
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Synthetic DEL Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );

    if ( pidynbotsConfig.enableDisconnAdj ) _computeBotParams( 0 );

    return 0;
}

//  ==============================================================================================
//  pidynbotsClientSynthAddCB
//
//  This is a slower but more reliable version of Player add event.  Unlike ClientADD event
//  this CB is generated by comparing the RCON roster listplayer dumps.  This CB does
//  not require easyAC server to be operational.
//
//  strIn:  in the format: "~SYNTHADD~ 76561000000000000 001.002.003.004 NameOfPlayer"
//
int pidynbotsClientSynthAddCB( char *strIn )
{
    // static char playerName[256], playerGUID[256], playerIP[256];
    // rosterParsePlayerSynthConn( strIn, 256, playerName, playerGUID, playerIP );
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Synthetic ADD Callback Name ::%s:: IP ::%s:: GUID ::%s::", playerName, playerIP, playerGUID );

    if ( pidynbotsConfig.enableConnAdj ) _computeBotParams( 0 );

    return 0;
}

//  ==============================================================================================
//  pidynbotsSigtermCB           
//
//  This callback is invoked when SISSM is terminated abnormally (either by OS or operator
//  pressing ^C.  Since the game server is assumed running, plugins should take necessary action
//  to leave the server in 'playable state' in absense of SISSM.
//
int pidynbotsSigtermCB( char *strIn )
{
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "SISSM Termination CB" );
    return 0;
}


//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsTravel( char *strIn )
{
    // char *mapName, *scenario, *mutator;
    // int humanSide;
    // mapName = rosterGetMapName();
    // scenario = rosterGetScenario();
    // mutator = rosterGetMutator();
    // humanSide = rosterGetCoopSide();
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Change map to ::%s:: Scenario ::%s:: Mutator ::%s:: Human ::%d::", mapName, scenario, mutator, humanSide );
    // apiSay( "pidynbots: Test Map-Scenario %s::%s::%s::%d::", mapName, scenario, mutator, humanSide );

    return 0;
}

//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsSessionLog( char *strIn )
{
    // char *sessionID;
    // sessionID = rosterGetSessionID();
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Session ID ::%s::", sessionID );
    // apiSay( "pidynbots: Session ID %s", sessionID );

    return 0;
}

//  ==============================================================================================
//  pidynbots
//
//
int pidynbotsObjectSynth( char *strIn )
{
    // char *obj, *typ; 
    // obj = rosterGetObjective();
    // typ = rosterGetObjectiveType();
    // logPrintf( LOG_LEVEL_INFO, "pidynbots", "Roster Objective is ::%s::, Type is ::%s::", obj, typ ); 

    if ( pidynbotsConfig.enableObjAdj ) _computeBotParams( 1 );

    return 0;
}


//  ==============================================================================================
//  pidynbotsInstallPlugin
//
//  This method is exported and is called from the main sissm module.
//
int pidynbotsInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    pidynbotsInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( pidynbotsConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           pidynbotsClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           pidynbotsClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 pidynbotsInitCB );
    eventsRegister( SISSM_EV_RESTART,              pidynbotsRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            pidynbotsMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           pidynbotsGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             pidynbotsGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          pidynbotsRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            pidynbotsRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   pidynbotsCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             pidynbotsPeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             pidynbotsShutdownCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     pidynbotsClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     pidynbotsClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 pidynbotsChatCB     );
    eventsRegister( SISSM_EV_SIGTERM,              pidynbotsSigtermCB  );
    eventsRegister( SISSM_EV_TRAVEL,               pidynbotsTravel );
    eventsRegister( SISSM_EV_SESSIONLOG,           pidynbotsSessionLog );
    eventsRegister( SISSM_EV_OBJECT_SYNTH,         pidynbotsObjectSynth );

    return 0;
}

