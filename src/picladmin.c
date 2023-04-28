//  ==============================================================================================
//
//  Module: PICLADMIN
//
//  Description:
//  Admin command execution from in-game chat 
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.22
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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
#include "winport.h"   // strcasestr

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "picladmin.h"



//  ==============================================================================================
//  Data definition 
//

#define NUM_RESPONSES  (5)
#define NUM_MACROS     (128)
#define NUM_REASONS    (9)
#define NUM_RULES      (5)

#define REBOOT_CANCEL  (0)
#define REBOOT_EOR     (1)
#define REBOOT_EOG     (2)

static int _queueReboot = REBOOT_CANCEL;

static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char msgOk[NUM_RESPONSES][CFS_FETCH_MAX];
    char msgErr[NUM_RESPONSES][CFS_FETCH_MAX];
    char msgInvalid[NUM_RESPONSES][CFS_FETCH_MAX];

    char cmdPrefix[CFS_FETCH_MAX];

    char macros[NUM_MACROS][CFS_FETCH_MAX];

    int botMaxAllowed, botMinDefault;
    int botUpperMin;

    char prepBlow[CFS_FETCH_MAX], prepCapture[CFS_FETCH_MAX];
    char askBlow[CFS_FETCH_MAX], askCapture[CFS_FETCH_MAX];
    char warnRusher[CFS_FETCH_MAX];

    char reason[NUM_REASONS][CFS_FETCH_MAX];

    char contactInfo[CFS_FETCH_MAX];
    char rulesInfo[NUM_RULES][CFS_FETCH_MAX];
    int  rulesIntervalSec;

    int  tempBanMinutes;
    int  noKickProtectSec;

    char strictTutorial[CFS_FETCH_MAX];

} picladminConfig;



static int _rulesIndex = -1;
static int _rulesTickCount = 0;


//  ==============================================================================================
//  Jump Pointer Data Structure
//

int _cmdHelp(), _cmdVersion(), _cmdMacros(), _cmdMacrosList();
int _cmdDeveloper();
int _cmdBotFixed(), _cmdBotScaled(), _cmdBotDifficulty();
int _cmdKillFeed(), _cmdFriendlyFire();
int _cmdRestart(), _cmdEnd(), _cmdReboot();
int _cmdBan(), _cmdBant(), _cmdKick(), _cmdNoWait();
int _cmdBanId(), _cmdBanIdt(), _cmdUnBanId(),_cmdKickId();
int _cmdGameModeProperty(), _cmdRcon();
int _cmdInfo(), _cmdAllowIn(), _cmdSpam(), _cmdFast(), _cmdAsk(), _cmdPrep(), _cmdWarn();
int _cmdRules(), _cmdContact(), _cmdThirdWave(), _cmdLock(), _cmdNoKick();
int _cmdMap(), _cmdMapX(), _cmdMapList(), _cmdMutList(), _cmdMutActive(), _cmdReInit();
int _cmdBotRespawn(), _cmdBotReset(), _cmdEndGame(), _cmdWax(), _cmdStrict();
int _cmdClan();

struct {

    char cmdShort[20];
    char cmdLong[40];
    char cmdHelp[40];
    int (*cmdFunction)();

}  cmdTable[] =  {

    { "help",   "help",          "help [command], help list",     _cmdHelp  },
    { "v",      "version",       "version [sissm]",               _cmdVersion },   
    { "dev",    "developer",     "dev [var][value]",              _cmdDeveloper },   

    { "bf",     "botfixed",      "botfixed [nBots]",              _cmdBotFixed },
    { "bs",     "botscaled",     "botscaled [nBots]",             _cmdBotScaled },
    { "bd",     "botdifficulty", "botdifficulty [0-10]",          _cmdBotDifficulty },
    { "x",      "execute",       "execute [alias]",               _cmdMacros },
    { "ml",     "macroslist",    "macroslist",                    _cmdMacrosList },

    { "kf",     "killfeed",      "killfeed on|off",               _cmdKillFeed },
    { "ff",     "friendlyfire",  "friendlyfire on|off",           _cmdFriendlyFire },

    { "gmp",    "gamemodeproperty",  "gmp [cvar] {value}",        _cmdGameModeProperty },
    { "rcon",   "rcon",          "rcon [passthru]",               _cmdRcon },

    { "rr",     "roundrestart",  "roundrestart [now]",            _cmdRestart },
//  { "re",     "roundend",      "roundend [now]",                _cmdEnd },
    { "reboot", "reboot",        "reboot now|eor|eog|cancel",     _cmdReboot },
    { "eg",     "endgame",       "endgame now",                   _cmdEndGame }, 

    { "b",      "ban",           "ban  [partial name] {reason}",  _cmdBan },
    { "bt",     "bant",          "bant [partial name] {reason}",  _cmdBant },
    { "k",      "kick",          "kick [partial name] {reason}",  _cmdKick },

    { "bi",     "banid",         "banid [steamid64] {reason}",    _cmdBanId },
    { "bit",    "banidt",        "banidt [steamid64] {reason}",   _cmdBanIdt },
    { "ub",     "unbanid",       "unbanid [steamid64]",           _cmdUnBanId },
    { "ki",     "kickid",        "kickid [steamid64] {reason}",   _cmdKickId },

    { "info",   "info",          "info",                          _cmdInfo    },
    { "al",     "allowin",       "allowin [partial name]",        _cmdAllowIn },
    { "sp",     "spam",          "spam on|off",                   _cmdSpam    },
    { "fast",   "fast",          "fast on|off",                   _cmdFast    },
    { "nk",     "nokick",        "nokick",                        _cmdNoKick  },
    { "nw",     "nowait",        "nowait",                        _cmdNoWait  },

    { "a",      "ask",           "ask",                           _cmdAsk     },
    { "p",      "prep",          "prep",                          _cmdPrep    },
    { "w",      "warn",          "warn",                          _cmdWarn    },

    { "mapx",    "mapx",         "map name [night][ins]",         _cmdMapX    },
    { "map",     "map",          "mapx name [night][ins]{mut mut mut...}",  _cmdMap  },
    { "maplist", "maplist",      "maplist",                       _cmdMapList    },
    { "mutlist", "mutlist",      "mutlist",                       _cmdMutList    },
    { "mutactive", "mutactive",  "mutactive",                     _cmdMutActive  },

    { "rules",  "rules",         "rules",                         _cmdRules      },
    { "contact","contact",       "contact",                       _cmdContact    },

    { "reinit", "reinit",        "reinit",                        _cmdReInit     },
    { "bz",     "botrespawn",    "botrespawn [0..3]",             _cmdBotRespawn },
    { "br",     "botreset",      "botreset",                      _cmdBotReset   },
    // { "tw",     "thirdwave",     "thirdwave on|off",           _cmdThirdWave  },
    { "lk",     "lock",          "lock on|off|perm|clan",         _cmdLock       },
    { "wax",    "wax",           "wax on|off|perm",               _cmdWax        },
    { "sr",     "strict",        "strict",                        _cmdStrict     },
    { "cl",     "clan",          "clan clear|add|list",           _cmdClan       },

    { "*",      "*",             "*",                             NULL }

};


// ===== _setOrigin/_gerOrigin
// 
// Set command origin ID for authentication/priv controls
//

static char currentOriginID[ 256 ];   // supports SteamID and Others

static void _setOriginID( char *originID )
{
    strlcpy( currentOriginID, originID, 256 );
    return;
}

static char *_getOriginID( void )
{
    return( currentOriginID );
}

// 
// ===== write ok, error, or unauthorized status to in-game chat
//
static int _respSay( char msgArray[NUM_RESPONSES][CFS_FETCH_MAX] )
{
    int msgIndex ;

    msgIndex = rand() % NUM_RESPONSES;
    apiSaySys( msgArray[ msgIndex ] );       

    return 0;
}

//  ==============================================================================================
//  _stdResp
//
//  Standard "ok" or "fail" message
//
static void _stddResp( int errCode )
{
    if ( 0 == errCode ) 
        _respSay( picladminConfig.msgOk );              // ok message
    else                
        _respSay( picladminConfig.msgErr );          // error message
    return;
}

//  ==============================================================================================
//  _stdResp
//
//  Standard "ok" or "fail" message
//
static char *_int2str( int numOut )
{
    static char strOut[256];

    snprintf( strOut, 256, "%d", numOut );
    return( strOut );
}


//  ==============================================================================================
//  _reason
//
//  Extract the 'reason' field from kick/kickid/ban/banid commands.
//  The returned value is the pointer to substring 'rawLine.'  If the reason
//  is not given it turns to end of rawLine, which is an empty (but not null) string.
// 
//  Sample rawLine input:  "kick sch this is my reason"
//
static char *_reason( char *rawLine )
{
    char *p1 = NULL, *p3 = NULL;
    int   i, spaceCount = 0;
    static char outReason[ 256 ];

    // Parse and look for "reason" string from what was typed by 
    // admin at console.  
    //
    p3 = & rawLine[ strlen( rawLine ) ];
    for (i=0; i< (int) strlen( rawLine ); i++) {
        if ( rawLine[i] == ' ' ) spaceCount++;
        if ( spaceCount == 2 ) {
            p3 = & rawLine[ i+1 ];
            break;
        }
    }
    strlcpy( outReason, p3, 256 );

    // if the 'reason' was one of shortcut preset 'canned' reasons
    // e.g., 'rush', 'afk', 'tk', etc.  then replace it with the 
    // longer explanation.
    //
    for ( i=0; i<NUM_REASONS; i++ ) {
        p1 = getWord( picladminConfig.reason[i], 0, "\007" ) ;
        if ( p1 != NULL ) { 
            if ( 0 == strcmp( p1, p3 ) ) {  // replace if exact match to preset reasons
                strlcpy( outReason, getWord( picladminConfig.reason[i], 1, "\007" ), 256);
                break;
            }
        }
    }

    // replace double quotes with a single quote
    //
    for ( i=0; i< (int) strlen(outReason); i++ ) {
        if ( outReason[i] == 0x22 ) outReason[i] = 0x27;
    }

    return( outReason );
}

//  ==============================================================================================
//  Individual Micro-Command Executors
//
//
//

// ===== "macros [alias]"
//
int _cmdMacros( char *arg, char *arg2, char *passThru ) 
{
    int i, j = 1, errCode = 1;
    char *w;
    char statusIn[ 1024 ];

    for (i = 0; i<NUM_MACROS; i++) {
        w = getWord( picladminConfig.macros[i], 0, "\007");                // get the macro label
        if ( w != NULL ) {                                       // skip blank entries (possible)
            if ( 0 == strcmp( arg, w ) ) {                         // check if operator cmd match
                while ( NULL != ( w = getWord( picladminConfig.macros[i], j++, "\007" )) ) {
                    errCode = (0 == apiRcon( w, statusIn ));
                    if ( errCode ) break;
                } 
                break;
            }
        }
    }
    _stddResp( errCode );   // ok or error message to game

    return 0;
}

// ===== "macroslist"
//
int _cmdMacrosList( char *arg, char *arg2, char *passThru ) 
{
    int i, errCode = 1;
    char listOut[ 1024 ];

    strlcpy( listOut, "Macros: ", 1024 ); 

    for (i = 0; i<NUM_MACROS; i++) {
        if ( 0 != strlen( picladminConfig.macros[i] )) {    // skip blank entries (possible)
            strlcat( listOut, getWord( picladminConfig.macros[i], 0, "\007" ), 1024 );
            strlcat( listOut, " ", 1024 );
            if ( strlen( listOut ) > 40 ) {
                apiSaySys( listOut );
                strclr( listOut );
            }
            errCode = 0;
        }
    }
    if ( errCode ) {
        apiSaySys( "No Macros defined in config" );
    }
    else {
        if (0 != strlen( listOut )) apiSaySys( listOut );
    }
    return 0;
}

// ===== "help [command]"
//
int _cmdHelp( char *arg, char *arg2, char *passThru ) 
{ 
    int i = 0, notFound = 1;
    char outStr[1024], originID[256];

    // if has parameter then look for a specific help for that command
    //
    if ( 0 != strlen( arg ) ) {
        while ( 1 == 1 ) {
            if ( ( 0 == strcmp( arg, cmdTable[i].cmdShort )) || ( 0 == strcmp( arg, cmdTable[i].cmdLong )) ) {
                apiSaySys("'%s' or %s", cmdTable[i].cmdShort, cmdTable[i].cmdHelp);
                notFound = 0;
                break;
            }
            i++;
            if ( 0 == strcmp( "*", cmdTable[i].cmdShort )) break; 
        }
    }

    // if above search was invalid command, or no arg was specified, display list of commands
    // 
    if ( notFound ) {
        strlcpy( originID, _getOriginID(), 256 );   // get requester ID

        strlcpy( outStr, "help [cmd], avail: ", 1024);
        i = 0;           
        while ( 1 == 1 ) {
            if ( apiAuthIsAllowedCommand( originID, cmdTable[i].cmdLong ) ) {
                strlcat( outStr, cmdTable[i].cmdLong, 1024 );
                strlcat( outStr, " ", 1024 );
                if ( strlen(outStr) > 40 ) {
                    apiSaySys( outStr );
                    strclr( outStr );
                }
            }
            i++;
            if ( 0 == strcmp( "*", cmdTable[i].cmdShort )) break; 
        }
        if (0 != strlen( outStr ))  apiSaySys( outStr );
    }
   
    return 0; 
}

// ===== "version"
//
int _cmdVersion( char *arg, char *arg2, char *passThru ) 
{ 
    apiSaySys( sissmVersion() );
    return 0; 
}

// ===== "developer"
//
int _cmdDeveloper( char *arg, char *arg2, char *passThru ) 
{ 
    long int argLong;
    char p2pVarName[255];
    int errCode = 1;

    if ( 0 != strlen( arg )) {
        strlcpy( p2pVarName, "dev.p2p.", 80 );
        strlcat( p2pVarName, arg,        80 );
        errCode = 0;
    } 

    if (1 == sscanf( arg2, "%ld", &argLong )) 
        p2pSetL( p2pVarName, argLong );
    else 
        argLong = 0;

    argLong = p2pGetL( p2pVarName, argLong );
    apiSaySys( "Developer var = '%s' value = '%ld'", p2pVarName, argLong );

    return errCode; 
}



// ===== "lock on|off"
//
static void _sayLockStatus( int showOnlyIfLocked )
{
    int lockStatus;
    char strOut[40];

    lockStatus = (int) ( 0xffL & p2pGetL( "pigateway.p2p.lockOut", 0L ) );
    switch( lockStatus ) {
    case 1:   strlcpy( strOut, "'on'",   40 );  break;
    case 2:   strlcpy( strOut, "'perm'", 40 );  break;
    case 3:   strlcpy( strOut, "'clan'", 40 );  break;
    default:  strlcpy( strOut, "'off'",  40 );  break;
    }

    // check if always display, or only when locked
    //
    if ( !showOnlyIfLocked || !(lockStatus==0) ) 
        apiSaySys( "Server lock status is: %s", strOut );
}
int _cmdLock( char *arg, char *arg2, char *passThru ) 
{  
    int errCode = 0;

    if (0 == strcmp( "off", arg )) {
        p2pSetL( "pigateway.p2p.lockOut", 0L );
        apiSaySys( "Server public slots are UNLOCKED." );
    }
    else if (0 == strcmp( "on", arg )) {
        p2pSetL( "pigateway.p2p.lockOut", 1L );
        apiSaySys( "Server LOCKED to public until next map." );
    }
    else if (0 == strcmp( "perm", arg )) {
        p2pSetL( "pigateway.p2p.lockOut", 2L );
        apiSaySys( "Server LOCKED to public until unlocked.");
    }
    else if (0 == strcmp( "clan", arg )) {
        p2pSetL( "pigateway.p2p.lockOut", 3L );
        apiSaySys( "Server LOCKED for clan-only until unlocked.");
    }
    else if (0 == strlen( arg )) {
        _sayLockStatus( 0 );
    }
    else {
        apiSaySys( "Missing or incorrect option: off|on|perm|clan" );
    }

    // _stddResp( errCode );   // ok or error message to game
    
    return errCode; 
}

// ===== "wax on|off"
//
int _cmdWax( char *arg, char *arg2, char *passThru ) 
{  
    int errCode = 0;

    if (0 == strcmp( "off", arg )) {
        p2pSetL( "piantirush.p2p.training", 0L );
        apiSaySys( "Wax Off - starting next objective");
    }
    else if (0 == strcmp( "on", arg )) {
        p2pSetL( "piantirush.p2p.training", 1L );
        apiSaySys( "Wax On - starting next objective");
        p2pSetF("piantirush.p2p.fast", 0.0 );             // turn off FAST mode
    }
    else if (0 == strcmp( "perm", arg )) {
        p2pSetL( "piantirush.p2p.training", 2L );
        apiSaySys( "Wax On/Perm - starting next objective");
        p2pSetF("piantirush.p2p.fast", 0.0 );             // turn off FAST mode
    }
    else {
        switch ( p2pGetL( "piantirush.p2p.training", 0L ) ) {
            case 1L:  apiSaySys( "Wax is ON" );    break;
            case 2L:  apiSaySys( "Wax is PERM" );  break;
            default:  apiSaySys( "Wax is OFF" );  
        }
    }
    
    return errCode; 
}

// ===== "strict"
// Strict command is same as "!wax on" but should be enabled for
// regular players.  "!wax on" will auto-disable at next map change.
//
int _cmdStrict( char *arg, char *arg2, char *passThru ) 
{
    switch ( p2pGetL( "piantirush.p2p.training", 0L ) ) {
    case 0L:
        p2pSetL( "piantirush.p2p.training", 1L );
        apiSaySys( picladminConfig.strictTutorial );
        p2pSetF("piantirush.p2p.fast", 0.0 );             // turn off FAST mode
        break;
    default:
        apiSaySys( "Strict/Wax mode already enabled" );
    };
    return 0;
}


// ===== "ThirdWwave on|off"

// ===== "ThirdWwave on|off"
//
int _cmdThirdWave( char *arg, char *arg2, char *passThru ) 
{  
    int errCode = 0;

    if (0 == strcmp( "off", arg )) {
        p2pSetL( "pithirdwave.p2p.thirdWaveEnable", 0L );
        apiSay( "ThirdWave Algorithm DISABLED." );
    }
    else if (0 == strcmp( "on", arg )) {
        p2pSetL( "pithirdwave.p2p.thirdWaveEnable", 1L );
        apiSay( "ThirdWave Algorithm ENABLED." );
    }
    else
        errCode = 1;

    _stddResp( errCode );   // ok or error message to game
    
    return errCode; 
}


// ===== "botfixed [nBots]"
//
int _cmdBotFixed( char *arg, char *arg2, char *passThru ) 
{  
    int errCode = 1, botCount;

    if (1 == sscanf( arg, "%d", &botCount )) {
        if ( ( botCount <=  picladminConfig.botMaxAllowed ) && ( botCount >= 0 ) ) {
           apiGameModePropertySet( "minimumenemies", _int2str( botCount/2 ) ); 
           apiGameModePropertySet( "maximumenemies", _int2str( botCount/2 ) );  
           apiGameModePropertySet( "soloenemies", _int2str( botCount/2 ) );  
           p2pSetL( "picladmin.p2p.botAdminControl", 1L );  // flag as admin control
           errCode = 0;
        }
    }
    _stddResp( errCode );   // ok or error message to game
    
    return errCode; 
}

// ===== "botscaled [nBots]"
// polymorphic: !botscaled [max], or !botscaled [min max]
//
int _cmdBotScaled( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 1, botCount, botCountMin, botCountMax;

    if (1 == sscanf( arg, "%d", &botCountMax )) {    // assume single param case
        if ( (botCountMax <= picladminConfig.botMaxAllowed) && (botCountMax >= 2) ) {
            botCountMin = picladminConfig.botMinDefault;
            errCode = 0;
            if ( 1 == sscanf( arg2, "%d", &botCount )) {  // check for 2nd param
                botCountMin = botCountMax;
                botCountMax = botCount;
                if ( picladminConfig.botUpperMin != 0 ) {
                    if ( botCountMin > picladminConfig.botUpperMin )
                        botCountMin = picladminConfig.botUpperMin;   // limit min
                }
                if ( botCountMin > botCountMax )                   errCode = 1;
                if ( botCountMax > picladminConfig.botMaxAllowed ) errCode = 1;
                if ( botCountMax <  2 )                            errCode = 1;
            }
        }
    }
    if ( 0 == errCode ) {
        apiGameModePropertySet( "minimumenemies", _int2str( botCountMin / 2 ) );
        apiGameModePropertySet( "maximumenemies", _int2str( botCountMax / 2 ) );
        // apiGameModePropertySet( "soloenemies", _int2str( botCountMax ) );  
        p2pSetL( "picladmin.p2p.botAdminControl", 1L );  // flag as admin control
        p2pSetL( "pidynbots.p2p.sigBotScaled", 1L );  // signal to pidynbots (if running)
    }
    _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "botdifficulty [0-10]"
//
int _cmdBotDifficulty( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 1;
    char strOut[256];
    double botDifficultyF;

    if (1 == sscanf( arg, "%lf", &botDifficultyF )) {
         
        if ( botDifficultyF >= 9.9 ) botDifficultyF = 9.9;     // workaround for system bug (?)

        if ((botDifficultyF <= 10.0) && (botDifficultyF >= 0.0)) {
           botDifficultyF /= 10.0;
           snprintf( strOut, 256, "%6.3lf", botDifficultyF);
           apiGameModePropertySet( "aidifficulty", strOut );
           p2pSetL( "picladmin.p2p.botAdminControlDifficulty", 1L );   // flag as admin control
           errCode = 0;
        }
    }

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "killfeed on|off"
//
int _cmdKillFeed( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 0;

    if (0 == strcmp( "off", arg )) 
        apiGameModePropertySet( "bKillFeed", "false" );
    else if (0 == strcmp( "on", arg )) 
        apiGameModePropertySet( "bKillFeed", "true" );
    else 
        errCode = 1;

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "friendlyfire on|off"
//
int _cmdFriendlyFire( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;

    if (0 == strcmp( "off", arg )) 
        apiGameModePropertySet( "bAllowFriendlyFire", "false" );
    else if (0 == strcmp( "on", arg )) 
        apiGameModePropertySet( "bAllowFriendlyFire", "true" );
    else 
        errCode = 1;

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "roundrestart [now]"
//
int _cmdRestart( char *arg, char *arg2, char *passThru  ) 
{
    char cmdOut[256], statusIn[256];
    int errCode = 0;

    strlcpy( cmdOut, "restartround 0", 256 );
    if (0 == strcmp( "now", arg )) 
        apiRcon( cmdOut, statusIn );
    else 
        errCode = 1;

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "roundend [now]" 
//
int _cmdEnd( char *arg, char *arg2, char *passThru ) 
{ 
    char cmdOut[256], statusIn[256];
    int errCode = 0; 

    strlcpy( cmdOut, "restartround 2", 256 );
    if (0 == strcmp( "now", arg )) 
        apiRcon( cmdOut, statusIn );
    else 
        errCode = 1;

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "reboot [now]" 
//
int _cmdReboot( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 0;

    // Cancel any pending reboot request
    //
    if (0 == strcmp( "cancel", arg ))   {
        if (_queueReboot != REBOOT_CANCEL) apiSaySys( "Commanded server reboot is cancelled." );
        _queueReboot = REBOOT_CANCEL;
    }

    // special 0-player case - is a reboot now/eog/eor issued from RCON while the
    // server is empty -- do an immediately reboot.
    //
    else if (0 == strcmp( "now", arg ) || (0 == apiPlayersGetCount() ))   {
        apiSaySys( "*Server is REBOOTING - please reconnect" );
        apiServerRestart( "Server Admin Reboot" );
        _queueReboot = REBOOT_CANCEL;
    }

    // End of Game reboot - queue it and wait for EOG event
    //
    else if (0 == strcmp( "eog", arg ))  {
       _queueReboot = REBOOT_EOG;
       apiSaySys( "This server will reboot at end of this game" );
    }

    // End of Round reboot - queue it and wait for EOR event
    //
    else if (0 == strcmp( "eor", arg ))  {
        _queueReboot = REBOOT_EOR;
        apiSaySys( "This server will reboot at end of this round" );
    }
    else 
        errCode = 1;

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "endgame [now]" 
//
//  End game modifies the time limit parameters and issues are round-restart RCON
//  command.  In most configuration this will immediately end the current game
//  and bring up the map voting.
//
//  Note there is no need to restore the time limit parameters that are modified here.
//  This is because when the game server starts a new map, it is reset to the 
//  game.ini value automatically.
//
int _cmdEndGame( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 0;
    char cmdOut[256], statusIn[256];

    if (0 == strcmp( "now", arg ))  {
        apiSaySys( "*Forcing Game End*" );
        apiGameModePropertySet( "PreRoundTime", "1" );
        apiGameModePropertySet( "RoundLimit",   "1" );
        apiGameModePropertySet( "RoundTIme",    "1" );
        strlcpy( cmdOut, "restartround 0", 256 );
        apiRcon( cmdOut, statusIn );
    }
    else {
        apiSaySys( "Specify 'now' to end the game" );
        errCode = 1;
    }

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}





// ===== "banid [steamid]"
// target may be offline (ISS ver 1.4+ banid rcon command)
//
int _cmdBanId( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256];

    if ( rosterIsValidGUID( arg ) ) {
        snprintf( cmdOut, 256, "banid \"%s\" 0 \"%s\"", arg, _reason( passThru ) );
        apiRcon( cmdOut, statusIn );
        apiSay( statusIn );
        errCode = 0;
    }

    if ( errCode != 0 ) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "banidt [steamid]"
// target may be offline (ISS ver 1.4+ banid rcon command)
//
int _cmdBanIdt( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256];

    if ( rosterIsValidGUID( arg ) ) {
        snprintf( cmdOut, 256, "banid \"%s\" %d \"[Tempban] %s\"", arg, picladminConfig.tempBanMinutes, _reason( passThru ) );
        apiRcon( cmdOut, statusIn );
        apiSay( statusIn );
        errCode = 0;
    }

    if ( errCode != 0 ) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "unbanid [steamid]"
// target may be offline (ISS ver 1.4+ unbanid rcon command)
//
int _cmdUnBanId( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256];

    if ( rosterIsValidGUID( arg ) ) {
        snprintf( cmdOut, 256, "unban \"%s\"", arg );
        apiRcon( cmdOut, statusIn );
        apiSay( statusIn );
        errCode = 0;
    }

    if ( errCode != 0 ) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "kickid [steamid]"
// target must be online
//
int _cmdKickId( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256];

    if ( rosterIsValidGUID( arg )) {  // EPIC
        snprintf( cmdOut, 256, "kick \"%s\" \"%s\"", arg, _reason( passThru) );
        apiRcon( cmdOut, statusIn );
        errCode = 0;
    }

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "ban [partial-name]"
// target must be online - perm ban by partial name
//
int _cmdBan( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256], steamID[256];
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg, 3 ), 256 );
    if ( 0 != strlen( steamID ) ) {
        if ( (snprintf( cmdOut, 255, "banid \"%s\" 0 \"%s\"", steamID, _reason( passThru) )) < 0 )
            logPrintf( LOG_LEVEL_WARN, "picladmin", "Overflow warning at _cmdBan" );
        apiRcon( cmdOut, statusIn );
        apiSay( statusIn );
        errCode = 0;
    }

    if ( errCode != 0 ) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "bant [partial-name]"
// target must be online - temp ban by partial name 
//
int _cmdBant( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256], steamID[256];
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg, 3 ), 256 );
    if ( 0 != strlen( steamID ) ) {
        if ( (snprintf( cmdOut, 255, "ban %s %d \"[Tempban] %s\"", steamID, picladminConfig.tempBanMinutes, _reason( passThru) )) < 0 )
            logPrintf( LOG_LEVEL_WARN, "picladmin", "Overflow warning at _cmdBan" );
        apiRcon( cmdOut, statusIn );
        apiSay( statusIn );
        errCode = 0;
    }

    if ( errCode != 0 ) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "kick [partial-name]"
// target must be online
//
int _cmdKick( char *arg, char *arg2, char *passThru ) 
{ 
    int vendorType;
    int errCode = 0;
    char steamID[256];

    // Lookup Steam or EPIC ID from player name 'arg'
    //
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg, 3 ), 256 );

    if ( 0 != strlen( steamID ) ) {

        vendorType = rosterIsValidGUID( steamID );

        switch( vendorType ) {
        case 1:   // steam:  kick by ID for definitive target
            errCode = apiKick( steamID, _reason( passThru ) );
            logPrintf( LOG_LEVEL_WARN, "picladmin", "Steam Admin kick by ID  ::%s::", steamID );
            break;
        case 2:   // epic:  kick by name due to NWI bug (SS 1.13 and prior)
            errCode |= apiKick( arg, _reason( passThru ) );
            logPrintf( LOG_LEVEL_WARN, "picladmin", "EPIC Admin kick by Name ::%s::", arg );
            break;
        default:  // fault
            logPrintf( LOG_LEVEL_WARN, "picladmin", "Failed to kick ::%s::", passThru );
            errCode = 1;
            break;
        }
    }

    if ( errCode != 0) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

#if 0
    char cmdOut[256], statusIn[256];
    if ( snprintf( cmdOut, 255, "kick %s \"%s\"", steamID, _reason( passThru ) ) < 0) :
        logPrintf( LOG_LEVEL_WARN, "picladmin", "Overflow warning at _cmdKick" );
    }
    apiRcon( cmdOut, statusIn );
    apiSay( statusIn );
#endif


// ===== "gamemodeproperty [cvar] [value]"
// Change or read gamemodeproperty
//

int _cmdGameModeProperty( char *arg, char *arg2, char *passThru ) 
{ 
    int  errCode = 1;
    char statusIn[256], cmdOut[256];

    // snprintf( cmdOut, 256, "kick %s", steamID );
    if ( 0 != strlen( arg ) ) {
        if ( 0 != strlen( arg2 ) ) {
            apiGameModePropertySet( arg, arg2 );
        }
        strlcpy( statusIn, apiGameModePropertyGet( arg ), 256 );
        if ( 0 == strlen( statusIn ) ) 
           apiSaySys( "GMP Readback Error");
        else {
           if ( snprintf( cmdOut, 255, "'%s' is set to '%s'", arg, statusIn ) < 0 )
               logPrintf( LOG_LEVEL_WARN, "picladmin", "Overflow warning at _cmdGameModeProperty" );
           apiSaySys( cmdOut );
           errCode = 0;
        }
    }
    else {
        _stddResp( 1 );   // format error message to game
    }
    return errCode;
}


// ===== "rcon [passthru string]"
// RCON Passthru - very dangerous but necessary for prototyping and test
//
int _cmdRcon( char *arg, char *arg2, char *passThru ) 
{ 
    int bytesRead, errCode = 1;
    char statusIn[4096];
    char *rconArgs;
    int  supportedRconCommand = 1;
    char *unsupportedRcon[] =  { "help", "listplayers", "listbans", "maps", "scenarios", "listgamemodeproperties", "*" };

    supportedRconCommand = !foundMatch( arg, unsupportedRcon, 1 );

    if ( supportedRconCommand ) {
        rconArgs = strstr( passThru, " " );
        if ( rconArgs != NULL ) {  
            bytesRead = apiRcon( rconArgs, statusIn );
            if ( bytesRead != 0 )  {
                errCode = 0;
                apiSaySys( statusIn );
            }
            else {
                apiSaySys( "RCON Interface Error" );
            }
        }
    }
    else {
        apiSaySys( "Unsupported RCON command" );
    }
    return errCode;
}

// ===== "info"
// Provide general info 
//
int _cmdInfo( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    char *s, strOut[256];
    double botDiff;
    int botMin, botMax;

    strclr( strOut );

    // get bot counts and difficulty 
    //
    s = apiGameModePropertyGet( "minimumenemies" ); 
    strlcat( strOut, s, 256 );
    s = apiGameModePropertyGet( "maximumenemies" ); 
    strlcat( strOut, ":", 256 );  strlcat( strOut, s, 256 );
    s = apiGameModePropertyGet( "aidifficulty" ); 
    strlcat( strOut, ":", 256 );  strlcat( strOut, s, 256 );

    if ( 3 == sscanf( strOut, "%d:%d:%lf", &botMin, &botMax, &botDiff )) {
        if ( botMin != botMax ) 
            apiSaySys("AI scaled count %d:%d Difficulty %6.3lf", botMin*2, botMax*2, botDiff*10.0 );
        else
            apiSaySys("AI fixed count %d Difficulty %6.3lf", botMax*2, botDiff*10.0 );
    }
    else {
        apiSaySys( "Retrieval error ::%s::", strOut );
    }
    if (apiBPIsActive()) { 
        apiSaySys( "%d of %d players alive", apiBPPlayerCount(), apiPlayersGetCount());
    }


    return errCode; 
}

// ===== "allowin"
// Send "allowin" info to pigateway plugin
//
int _cmdAllowIn( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;

    if ( 0 != strlen( arg ) ) {
        p2pSetS( "picladmin.p2p.allowInPattern", arg );
        p2pSetL( "picladmin.p2p.allowTimeStart", apiTimeGet() );
        apiSaySys("Allow-in '%s' for a brief moment", p2pGetS( "picladmin.p2p.allowInPattern","" ));
    }
    else {
        p2pSetS( "picladmin.p2p.allowInPattern", "" );
        p2pSetL( "picladmin.p2p.allowTimeStart", 0L );
        apiSaySys("Allow-in closed");
    }

    // _stddResp( errCode );   // ok or error message to game
    return errCode; 
}

// ===== "spam"
// Send "spam" info to API module (apiSaySys)
//
int _cmdSpam( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;

    if (0 == strcmp( "off", arg )) {
        p2pSetF("api.p2p.sayDisable", 1.0 );
        _stddResp( 0 );   // ok message to game
    }
    else if (0 == strcmp( "on", arg )) {
        p2pSetF("api.p2p.sayDisable", 0.0 );
        _stddResp( 0 );   // ok message to game
    }
    else {
        _stddResp( 1 );   // error message to game
    }
    return errCode;
}


// ===== "prep"
// Send tactical team query for use before taking objective 
//
int _cmdPrep( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    char *typeObj, outStr[256];

    typeObj = rosterGetObjectiveType();
    strclr( outStr );
    if ( NULL != strcasestr( typeObj, "weaponcache" ) )
        strlcpy( outStr, picladminConfig.prepBlow, 256 );
    if ( NULL != strcasestr( typeObj, "capturable"  ) )
        strlcpy( outStr, picladminConfig.prepCapture, 256 );

    apiSaySys( outStr );
    return errCode;
}
// ===== "ask"
// Send tactical team query for use before taking objective 
//
int _cmdAsk( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    char *typeObj, outStr[256];

    typeObj = rosterGetObjectiveType();
    strclr( outStr );
    if ( NULL != strcasestr( typeObj, "weaponcache" ) ) 
        strlcpy( outStr, picladminConfig.askBlow, 256 );
    if ( NULL != strcasestr( typeObj, "capturable"  ) ) 
        strlcpy( outStr, picladminConfig.askCapture, 256 );

    apiSaySys( outStr );
    return errCode;
}

// ===== "warn"
// Send warning to cap rushers
//
int _cmdWarn( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    char outStr[256];

    strlcpy( outStr, picladminConfig.warnRusher, 256 );
    apiSaySys( outStr );

    return errCode;
}

// ===== "contact"
// Send warning to cap rushers
//
int _cmdContact( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    char outStr[256];

    strlcpy( outStr, picladminConfig.contactInfo, 256 );
    apiSaySys( outStr );

    return errCode;
}

// ===== "rules"
// 
//
int _cmdRules( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;

    if (_rulesIndex == -1 ) {   // handled by periodic callback
        _rulesIndex = 0;
    }

    return errCode;
}

// ===== "fast"
// Send "fast" info to piantirush plugin
//
int _cmdFast( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;

    if (0 == strcmp( "off", arg )) {
        p2pSetF("piantirush.p2p.fast", 0.0 );
        apiSaySys("Anti-Rushing algorithm will be enabled after the next objective");
    }
    else if (0 == strcmp( "on", arg )) {
        p2pSetF("piantirush.p2p.fast", 1.0 );
        apiSaySys("Anti-Rushing algorithm is disabled by admin");
        p2pSetL( "piantirush.p2p.training", 0L );    // turn off WAX
    }
    else {
        if (0.0 == p2pGetF( "piantirush.p2p.fast", 0.0 ) )
            apiSaySys( "Fast is OFF" );
        else
            apiSaySys( "Fast is ON" );
    }

    return errCode;
}

// ===== "nokick"
// Exempt autokick by piantirush for window of seconds (training)
//
int _cmdNoKick( char *arg, char *arg2, char *passThru ) 
{ 
    // Run this discretely - so no prompt.
    //
    p2pSetL( "piantirush.p2p.nokickprotect", (long) picladminConfig.noKickProtectSec );
    _stddResp( 0 );

    return 0;
}

// ===== "nowait"
// Exempt autokick by piantirush for window of seconds (training)
//
int _cmdNoWait( char *arg, char *arg2, char *passThru ) 
{ 
    // Run this discretely - so no prompt.
    // Set semaphore (binary), will be picked up by Periodic Processing
    // in piantirush plugin
    //
    p2pSetL( "piantirush.p2p.SigNoWait", 1L );
    _stddResp( 0 );

    return 0;
}

// ===== "mapX" OBSOLETE
// Change maps OBSOLETE
//
int _cmdMapX( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    int isNight = 0, isIns = 0;
    char *w, gameMode[80];
    char mutatorsList[256];
    int i, j;

    strclr( mutatorsList );
    if ( 0 == strlen( arg ) ) {
        apiSaySys( "You must specify a map.  Type !maplist");
        errCode = 1;
    }
    else {
        // Look for "ins" (Insurgent) and "night" (Night lighting) modes.
        //
        if (NULL != strcasestr( passThru, " night" )) isNight = 1;
        if (NULL != strcasestr( passThru, " ins"   )) isIns   = 1;

        // Look for optional gamemode:  checkpoint, checkpointhardcore, etc.
        // Because new game mode may be added as a mod, this table must be 
        // maintained in the config file.
        //
        j = 2;
        strclr( gameMode );
        while ( 1 == 1 ) {
            w = getWord( passThru, j++, " " );
            if ( w == NULL )       break;
            if ( 0 == strlen( w )) break;
#if 0 
            if ( w[0] == '%' ) {
                strlcat( mutatorsList, &w[1], 256 );
                strlcat( mutatorsList, ",",  256 );
            };
#endif

            if ( apiIsSupportedGameMode( w ) ) {
                strlcpy( gameMode, w, 80 );
            }
        } 

        // clean up the mutatorsList (remove trailing comma)
        //
        if ( 0 != ( i = (int) strlen( mutatorsList ) )) {
            mutatorsList[ i-1 ] = 0;
        }

        // change map
        errCode = apiMapChange( arg, gameMode, isIns, isNight, mutatorsList, 0 );

        if ( errCode ) {
            apiSaySys( "Map/mode/side not found" );
        }
    }
    return errCode;
}

// ===== "map"
// Change Maps Extended
//
int _cmdMap( char *arg, char *arg2, char *passThru )
{
    int errCode = 0;
    int isNight = 0, isIns = 0, clearMutators = 0;
    char *w, gameMode[80], wordBuf[80], mutName[80];
    char mutatorsList[256];
    int i, j, c;

    strclr( mutatorsList );
    strclr( gameMode );

    if ( 0 == strlen( arg ) ) {
        apiSaySys( "You must specify a map.  Type !maplist");
        errCode = 1;
    }
    else {
        //
        // Walk through each word to determine if it is "ins", "sec", 
        // "day", "night" or any of the approved mutators or game modes.
        // The check order may be important in the future.
        //
        j = 2;
        while ( errCode == 0 ) {

            w = getWord( passThru, j++, " " );
            if ( w == NULL )       break;
            if ( 0 == strlen( w )) break;
         
            strlcpy( wordBuf, w, 80 );

            // alias 
            //
            if ( 0 == strcasecmp( wordBuf, "hardcore" )) strlcpy( wordBuf, "checkpointhardcore", 80 );

            // process each word
            //
            if ( 0 == strcmp( wordBuf, "@" ) ) 
                clearMutators = 1;
            else if (NULL != strcasestr( wordBuf, "night" )) 
                isNight = 1;
            else if (NULL != strcasestr( wordBuf, "ins"   )) 
                isIns   = 1;
            else if ( apiIsSupportedGameMode( wordBuf ) ) {
                strlcpy( gameMode, wordBuf, 80 );
            }
            else {
                c = apiMutLookup( wordBuf, mutName, 80 );
                switch (c) {
                    case 0:
                        apiSaySys("Unknown game mode or mutator '%s'", wordBuf );
                        logPrintf(  LOG_LEVEL_INFO, "picladmin", "Unknown game mode or mutator '%s'", wordBuf );
                        errCode = 1;
                        break;
                    case 1:
                        strlcat( mutatorsList, mutName, 256 );
                        strlcat( mutatorsList, ",",     256 );
                        break;
                    default:
                        apiSaySys("Ambiguious mutator name '%s'", wordBuf );
                        logPrintf(  LOG_LEVEL_INFO, "picladmin", "Ambiguious mutator name '%s'", wordBuf );
                        errCode = 1;
                        break;
                }
            }
        }

        // clean up the mutatorsList (remove trailing comma)
        //
        if ( 0 != ( i = (int) strlen( mutatorsList ) )) {
            mutatorsList[ i-1 ] = 0;
        }

        if ( 0 == errCode ) {
            // change map
            logPrintf(  LOG_LEVEL_INFO, "picladmin", "----------------------------------------------------" );
            logPrintf(  LOG_LEVEL_INFO, "picladmin", "Map change to '%s' Mode '%s'  Sec/Ins '%d' Day/Night '%d'", arg, gameMode, isIns, isNight );
            logPrintf(  LOG_LEVEL_INFO, "picladmin", "Mutators: '%s'  clear: %d", mutatorsList, clearMutators );
            errCode = apiMapChange( arg, gameMode, isIns, isNight, mutatorsList, clearMutators );
        }

        if ( errCode ) {
            apiSaySys( "Map/mode/side/mutator not found" );
        }
    }
    return errCode;
}


// ===== _showList 
// list maps and mutators to console
//
static int _showList( char *list )
{
    char *w, *v, outBuf[256];
    int errCode = 1;
    int j, i, exitF = 0;;

    w = list;
    j = 0;
    if ( w != NULL ) { 
        errCode = 0;
        while ( 0 == exitF ) {
            strclr( outBuf );
            for (i=0; i<4; i++) {
                v = getWord( w, j++, " " );
                if ( NULL == v ) {
                    exitF = 1;
                    break;
                }
                if ( 0==strlen( v ) ) {
                    exitF = 1;
                    break;
                }
                strncat( outBuf, v, 255 );
                strncat( outBuf, " ", 255 );
            }
            apiSaySys( "%s", outBuf );
        }
    } 
    return errCode;
}

// ===== "maplist"
// list maps 
//
int _cmdMapList( char *arg, char *arg2, char *passThru ) 
{ 
    char *w, listBuf[1024];
    int errCode = 0;

    if (NULL != (w = apiMapList() )) {
        strlcpy( listBuf, w, 1024 );
        _showList( listBuf );
    }

    return errCode;
}


// ===== "mutList"
// list mutators
//
int _cmdMutList( char *arg, char *arg2, char *passThru )
{
    char *w, listBuf[1024];
    int errCode = 0;

    if (NULL != (w = apiMutList() )) {
        strlcpy( listBuf, w, 1024 );
        _showList( listBuf );
    }

    return errCode;
}

// ===== "mutActive"
// list mutators
//
int _cmdMutActive( char *arg, char *arg2, char *passThru )
{
    char *w, listBuf[1024];
    int errCode = 0;

    if (NULL != (w = apiMutActive( 1 ) )) {
        strlcpy( listBuf, w, 1024 );
        _showList( listBuf );
    }

    return errCode;
}


// ===== "reinit"
//  Superadmin command to re-read the auth and other lists from disk
//
int _cmdReInit( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    apiInitAuth();
    apiInitBadWords();

    apiSaySys( "SISSM ReInit Complete." );
    return errCode;
}

// ===== "botspawn"
//  experimental command to re-spawn the bots
//
int _cmdBotRespawn( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    int resetType = 0;

    if ( 1 != sscanf( arg, "%d", &resetType ) ) {
        resetType = 0;
    }
    errCode = apiBotRespawn( resetType );

    apiSaySys( "Bots Respawned" );
    return errCode;
}

// ===== "botreset"
//  Restore bots back to SISSM control (counteracts !bs, !bf, !bd)
//
int _cmdBotReset( char *arg, char *arg2, char *passThru ) 
{ 
    // signal pidynbots that picladmin (admins) reliquishes 
    // bot count and difficulty overrides.
    //
    p2pSetL( "picladmin.p2p.botAdminControlDifficulty", 0L );
    p2pSetL( "picladmin.p2p.botAdminControl", 0L );

    // signal pidynbots to force refresh bot configurations
    // if pidynbots plugin is not enabled, then ignore.
    //
    p2pSetL( "pidynbots.p2p.sigBotScaled", 1L );

    apiSaySys( "Bot Reset" );
    return 0;
}

// ===== "clan"
//  Manage 'clan' list for restricted server access
//
int _cmdClan( char *arg, char *arg2, char *passThru ) 
{ 
    int j, errCode = 0, found = 0, count;
    char *w, clanID[127];

    // === Case: "Clear"
    //
    if (0 == strcmp( "clear", arg )) {
        apiClanClear();
        apiSaySys( "Clan list cleared" );
        logPrintf( LOG_LEVEL_INFO, "picladmin", "cmdClan cleared list" );
    } 

    // === Case: "List"
    //
    else if (0 == strcmp( "list", arg )) {
        for (j = 0;; j++ ) {
            if ( NULL != (w = apiClanGet( j )) )  {
                strlcpy( clanID, w, 127 );
                if ( 0 == strlen( clanID ) ) break;
                found = 1;
                apiSaySys( "%03d '%s' ", j, clanID );
                logPrintf( LOG_LEVEL_INFO, "picladmin", "cmdClan list ::%d::%s::", j, clanID );
            }
            else {
                break;
            }
        }        
        if (found == 0) {
            apiSaySys( "Clan list is empty" );
        } 
    } 

    // === Case: "Add"
    //
    else if (0 == strcmp( "add", arg )) {
        if (0 != strlen( arg2 )) {
            for (count = 0, j = 2;; j++ ) {
                if (NULL != (w = getWord( passThru, j, " " ))) {
                    strlcpy( clanID, w, 127 );
                    apiClanAdd( clanID );
                    count++;
                }
                else {
                    break;
                }
            }
            // apiClanAdd( arg2 );
            logPrintf( LOG_LEVEL_INFO, "picladmin", "cmdClan add ::%s::", passThru );
            p2pSetL( "pigateway.p2p.lockOut", 3L );   // clan-lock the server
            apiSaySys( "Added %d elements, Server is Clan-locked", count );
        }
        else {
            apiSaySys( "You must specify partial name or ID" );
        }
    } 
    else {
        apiSaySys( "Missing or incorrect option: clear|add|list" );
    }
    return( errCode );
}



#if 0
//  ==============================================================================================
// 
//  (Internal)  _replaceDoubleColonWithBell
//
//
static void  _replaceDoubleColonWithBell( char *strInOut )
{
    int i;

    if ( strlen( strInOut ) > 1  ) {
        for (i=0; i<strlen( strInOut )-1; i++) {
            if (( strInOut[i] == ':' ) && ( strInOut[i+1] == ':'))  {
                strInOut[i] = '\007';
                strInOut[i+1] = '\007';
            }
        }
    }
    return;
}
#endif


//  ==============================================================================================
//  picladminInitConfig
//
//  Initialize configuration variables for this plugin
//
int picladminInitConfig( void )
{
    cfsPtr cP;
    int i;
    char varArray[256];

    _queueReboot = REBOOT_CANCEL;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "picladmin.pluginstate" variable from the .cfg file
    picladminConfig.pluginState = (int) cfsFetchNum( cP, "picladmin.pluginState", 0.0 );  // disabled by default

    // Read the humanized responses for when a command is issued can was successfully executed
    //
    strlcpy( picladminConfig.msgOk[0],       cfsFetchStr( cP, "picladmin.msgOk[0]",      "Ok!" ),             CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgOk[1],       cfsFetchStr( cP, "picladmin.msgOk[1]",      "Ok!" ),             CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgOk[2],       cfsFetchStr( cP, "picladmin.msgOk[2]",      "Ok!" ),             CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgOk[3],       cfsFetchStr( cP, "picladmin.msgOk[3]",      "Ok!" ),             CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgOk[4],       cfsFetchStr( cP, "picladmin.msgOk[4]",      "Ok!" ),             CFS_FETCH_MAX);

    // Read the humanized responses for when a command has an error
    //
    strlcpy( picladminConfig.msgErr[0],      cfsFetchStr( cP, "picladmin.msgErr[0]",     "Invalid Syntax!" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgErr[1],      cfsFetchStr( cP, "picladmin.msgErr[1]",     "Invalid Syntax!" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgErr[2],      cfsFetchStr( cP, "picladmin.msgErr[2]",     "Invalid Syntax!" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgErr[3],      cfsFetchStr( cP, "picladmin.msgErr[3]",     "Invalid Syntax!" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgErr[4],      cfsFetchStr( cP, "picladmin.msgErr[4]",     "Invalid Syntax!" ), CFS_FETCH_MAX);

    // Read the humanized responses for when a non-admin tries to use a command
    //
    strlcpy( picladminConfig.msgInvalid[0], cfsFetchStr( cP, "picladmin.msgInvalid[0]",  "Unauthorized!" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgInvalid[1], cfsFetchStr( cP, "picladmin.msgInvalid[1]",  "Unauthroized!" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgInvalid[2], cfsFetchStr( cP, "picladmin.msgInvalid[2]",  "Unauthroized!" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgInvalid[3], cfsFetchStr( cP, "picladmin.msgInvalid[3]",  "Unauthroized!" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.msgInvalid[4], cfsFetchStr( cP, "picladmin.msgInvalid[4]",  "Unauthorized!" ),   CFS_FETCH_MAX);


    // Read the 5-element rules list
    //
    strlcpy( picladminConfig.rulesInfo[0], cfsFetchStr( cP, "picladmin.rulesInfo[0]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.rulesInfo[1], cfsFetchStr( cP, "picladmin.rulesInfo[1]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.rulesInfo[2], cfsFetchStr( cP, "picladmin.rulesInfo[2]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.rulesInfo[3], cfsFetchStr( cP, "picladmin.rulesInfo[3]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.rulesInfo[4], cfsFetchStr( cP, "picladmin.rulesInfo[4]",  "" ),   CFS_FETCH_MAX);

    // Read the interval (sec) on how fast the rules are printed on player screen.  0 = no rules
    //
    picladminConfig.rulesIntervalSec = (int) cfsFetchNum( cP, "picladmin.rulesIntervalSec", 2.0 );  //  2sec apart

    // Read the server contacts info
    //
    strlcpy( picladminConfig.contactInfo, cfsFetchStr( cP, "picladmin.contactInfo",  "Contact info not set" ),   CFS_FETCH_MAX);

    // Read the operator-specified "macro" (alias) sequence
    //
    for ( i=0; i<NUM_MACROS; i++ ) {
        snprintf( varArray, 256, "picladmin.macros[%d]", i );
        strlcpy( picladminConfig.macros[i],     cfsFetchStr( cP, varArray,  "" ),   CFS_FETCH_MAX);
        replaceDoubleColonWithBell( picladminConfig.macros[i] );
    }

    strlcpy( picladminConfig.cmdPrefix,      cfsFetchStr( cP, "picladmin.cmdPrefix",      "!" ),              CFS_FETCH_MAX);

    // if nil cmdPrefix is in the .cfg file, this will cause every command to be admin command, so
    // we override that here.
    //
    if ( 0 == strlen( picladminConfig.cmdPrefix )) strlcpy( picladminConfig.cmdPrefix, "!", CFS_FETCH_MAX );

    // Set bot count max and default min values 
    // if Max value is set too high and admin issues a !bf or !bs, servers & clients may crash
    //
    picladminConfig.botMaxAllowed = (int) cfsFetchNum( cP, "picladmin.botMaxAllowed", 200.0 );
    picladminConfig.botMinDefault = (int) cfsFetchNum( cP, "picladmin.botMinDefault",  6.0 );
    if ( picladminConfig.botMaxAllowed < 2 ) picladminConfig.botMaxAllowed = 4;
    if ( picladminConfig.botMinDefault < 2 ) picladminConfig.botMinDefault = 2;

    // BotUpperMin limits the maximum number admin can specify in !botscaled !bs command.
    // Recommended value:  10.  If "0" is set (default) the limiter is disabled.
    //
    picladminConfig.botUpperMin = (int) cfsFetchNum( cP, "picladmin.botUpperMin", 0.0 );

    // text to show in-game for !ask, !prep, and !warn
    //
    strlcpy( picladminConfig.prepBlow, cfsFetchStr( cP, "picladmin.prepBlow",  
        "*Planting/Ready to detonate!" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.prepCapture, cfsFetchStr( cP, "picladmin.prepCapture",  
        "*Testing Capture Point - Stepping On then Off" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.askBlow, cfsFetchStr( cP, "picladmin.askBlow",  
        "*DETONATING in 5-sec, <NEGATIVE> to halt" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.askCapture, cfsFetchStr( cP, "picladmin.askCapture",  
        "*BREACHING in 5-sec, <NEGATIVE> to halt" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.warnRusher, cfsFetchStr( cP, "picladmin.warnRusher",  
        "*Do NOT BREACH/DETONATE CACHE without ASKING TEAM" ), CFS_FETCH_MAX);

    // Read the "reasons" preset strings for ban/banid/kick/kickid commmands
    //
    for ( i=0; i<NUM_REASONS; i++ ) {
        snprintf( varArray, 256, "picladmin.reason[%d]", i );
        strlcpy( picladminConfig.reason[i],     cfsFetchStr( cP, varArray,  "" ),   CFS_FETCH_MAX);
        replaceDoubleColonWithBell( picladminConfig.reason[i] );
    }

    // Read the temp ban duration in minutes
    //
    picladminConfig.tempBanMinutes = (int) cfsFetchNum( cP, "picladmin.tempBanMinutes", 1440.0 );

    // Read the nokick exemption (from antirush autokick) window time in seconds
    //
    picladminConfig.noKickProtectSec = (int) cfsFetchNum( cP, "picladmin.noKickProtectSec",  15.0 );
    if ( picladminConfig.noKickProtectSec < 0 ) picladminConfig.noKickProtectSec = 0;

    // Info text that is shown when a player types !strict
    //
    strlcpy( picladminConfig.strictTutorial,  cfsFetchStr( cP, "picladmin.strictTutorial",
        "*Auto-kick if no '11' before breach/detonate!"), 
        CFS_FETCH_MAX);

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  _resetP2PVars
//
//  Reset all P2P (Plugin-to-Plugin) variables - this happens at start/end of Game, and
//  map change.
//
static void _resetP2PVars( void )
{
    p2pSetF( "piantirush.p2p.fast", 0.0 );
    p2pSetF( "api.p2p.sayDisable", 0.0 );

    // Don't reset allowin because the player may join between map change
    //
    // p2pSetS( "picladmin.p2p.allowInPattern", "" );
    // p2pSetL( "picladmin.p2p.allowTimeStart", 0L );

    // Release the bot ownership - if this module (admin) change the bot
    // setting it is "owned" by this module until start of next map
    //
    p2pSetL( "picladmin.p2p.botAdminControl", 0L );
    p2pSetL( "picladmin.p2p.botAdminControlDifficulty", 0L );

    return ;
}



//  ==============================================================================================
//  picladminRoundEndCB
//
//  End of Game event processing
// 
int picladminRoundEndCB( char *strIn )  
{
    if ( _queueReboot == REBOOT_EOR ) {
        _queueReboot = REBOOT_CANCEL;
        apiSaySys( "*Server is REBOOTING - please reconnect" );
        apiServerRestart( "Server Admin Reboot" );
    }
    return 0; 
}

//  ==============================================================================================
//  picladminGameEndCB
//
//  End of Game event processing
// 
int picladminGameEndCB( char *strIn )  
{
    _resetP2PVars(); 
    if ( _queueReboot == REBOOT_EOG ) {
        _queueReboot = REBOOT_CANCEL;
        apiSaySys( "*Server is REBOOTING - please reconnect" );
        apiServerRestart( "Server Admin Reboot" );
    }
    return 0; 
}


int picladminRoundStartCB( char *strIn ) { _resetP2PVars(); return 0; }


//  ==============================================================================================
//  picladminPeriodicCB
//
//  Periodic processing at 1Hz.  
//
int picladminPeriodicCB( char *strIn ) 
{ 
    if (( _rulesIndex != -1 ) && ( picladminConfig.rulesIntervalSec != 0) ) {
        if ( (++_rulesTickCount) >= picladminConfig.rulesIntervalSec ) {
            _rulesTickCount = 0;
            apiSaySys( picladminConfig.rulesInfo[ _rulesIndex++ ]);
            if ( _rulesIndex >= NUM_RULES ) _rulesIndex = -1;
        }
    }

    // Special condition when EOG/EOR reboot is queued but everybody logs off
    // before end of game/round.
    //
    if (( _queueReboot == REBOOT_EOG ) || ( _queueReboot == REBOOT_EOR )) {
       if ( 0 == apiPlayersGetCount() ) {
           _queueReboot = REBOOT_CANCEL;
           apiSaySys( "*Server is REBOOTING - please reconnect" );
           apiServerRestart( "Server Admin Reboot" );
       }
    }

    return 0; 
}

//  ==============================================================================================
//  picladminCapturedCB
//
//  Callback for when objectie is captured
//
int picladminCapturedCB( char *strIn ) 
{ 
    // _sayLockStatus(  1 );
    return 0; 
}


//  ==============================================================================================
//  (Unused Callback methods)
//
//  Left here for future expansion
//
int picladminClientAddCB( char *strIn ) { return 0; }
int picladminClientDelCB( char *strIn ) { return 0; }
int picladminInitCB( char *strIn ) { return 0; }
int picladminRestartCB( char *strIn ) { return 0; }
int picladminClientSynthAddCB( char *strIn ) { return 0; }
int picladminShutdownCB( char *strIn ) { return 0; }
int picladminClientSynthDelCB( char *strIn ) { return 0; }
int picladminGameStartCB( char *strIn ) { _resetP2PVars(); return 0; }
int picladminMapChangeCB( char *strIn ) { _resetP2PVars(); return 0; }


//  ==============================================================================================
//  _commandExecute
//
//  Execute the admin command-line command by invoking the individual method based on 
//  sting command lookup.
//
int _commandExecute( char *cmdString, char *originID ) 
{   
    char *p, cmdOut[256], arg1Out[256], arg2Out[256];
    int i, errCode = 1;
    int isCommand = 0;

    // zero the physical stores
    //
    strclr( cmdOut );  strclr( arg1Out );  strclr( arg2Out );

    // The input string cmdString will be in the format, e.g., "botfixed 30"
    // parse command vs arguments and invoke the string-matched function 
    //
    if ( NULL != (p = getWord( cmdString, 0, " " ) )) strlcpy( cmdOut,  p, 256);
    if ( NULL != (p = getWord( cmdString, 1, " " ) )) strlcpy( arg1Out, p, 256);
    if ( NULL != (p = getWord( cmdString, 2, " " ) )) strlcpy( arg2Out, p, 256);

    // Handle special 'help' case for when no argument is specified
    // 
    if ( (0 == strcmp("help", cmdOut)) && (0 == strlen( arg1Out )) ) {
        strlcpy( arg1Out, cmdOut, 256 );
        strlcpy( cmdOut, "help",  256 );
    } 

    // remember the origin ID for some command that may need it (e.g., !help)
    //
    _setOriginID( originID );

    // loop through the command table
    //
    i = 0;
    while ( 1==1 ) {

        if ( (0 == strcmp( cmdTable[i].cmdShort, cmdOut )) || (0 == strcmp( cmdTable[i].cmdLong, cmdOut )) ) {

            isCommand = 1;

            // check if this admin has enough privilege to execute this command
            //
            if ( ! apiAuthIsAllowedCommand( originID, cmdTable[i].cmdLong ) ) {
                logPrintf( LOG_LEVEL_INFO, "picladmin", "Insufficient priv for [%s] to execute %s", originID, cmdString );
                _respSay( picladminConfig.msgInvalid );                       // unauthorized message
                break;
            }

            // execute the command
            //
            errCode = (*cmdTable[i].cmdFunction)( arg1Out, arg2Out, cmdString );
            if ( errCode == 0 ) {
                logPrintf( LOG_LEVEL_INFO, "picladmin", "Admin [%s] executed command: %s", originID, cmdString );
            }
            break;
        }
        i++;
        if ( 0==strcmp( cmdTable[i].cmdShort, "*") )
            break;
    }

    // check if the command was not in standard commands table -- if not
    // it may be a operator added macro.
    // 
    if ( !isCommand ) {

        // check if this admin has enough privilege to execute this command
        //
        if ( apiAuthIsAllowedMacro( originID, cmdOut ) ) {
        
            // execute the macro
            //
            _cmdMacros( cmdOut, "", "" );
            if ( errCode == 0 ) {
                logPrintf( LOG_LEVEL_INFO, "picladmin", "Admin [%s] executed macro: %s", originID, cmdOut );
            }
        }
    }

    // Command level security - set ident to invalid
    //
    _setOriginID( "INVALID" );

    return errCode; 
}


//  ==============================================================================================
//  _commandParse
//
//  Parse the log line 'say' chat log into originator GUID and the said text string
//  [2019.08.30-23.39.33:262][176]LogChat: Display: name(76561198000000001) Global Chat: !ver sissm
//  [2019.09.15-03.44.03:500][993]LogChat: Display: name(76561190000000002) Team 0 Chat: !v
//  [2022.03.23-14.49.24:208][545]LogChat: Display: name(00000000000000000000000000000000|00000000000000000000000000000000) Global Chat: !v
//

int _commandParse( char *strIn, int maxStringSize, char *clientGUID, char *cmdString  ) {
    char *w, *v;
    int guidType, parseError = 1;
    char prefixMatch[256];

    strclr( clientGUID );
    strclr( cmdString );

    // Look for start of chat text content 
    //
    strlcpy( prefixMatch, " Chat: ", 256 );       // build search pattern for start of msg
    strlcat( prefixMatch, picladminConfig.cmdPrefix, 256 );         // with command prefix
    w = strstr( strIn, prefixMatch );

    if ( w != NULL ) {
        if (0 != ( guidType = rosterFindFirstID( strIn, '(', ')', clientGUID, maxStringSize ))) {
            v = w + strlen( prefixMatch );
            strlcpy ( cmdString, v, maxStringSize );
            strTrimInPlace( cmdString );
            if (0 != strlen( cmdString )) parseError = 0;
        }
    }
    return parseError;
}

#define RCONSTEAMIDSIZE  (17)

//  ==============================================================================================
//  _commandParseRCON
//
//  Parse the log line 'LogRcon' 
//  [2020.09.18-19.01.18:450][779]LogRcon: 127.0.0.1:44960 << !testcommand
//
int _commandParseFromRCON( char *strIn, int maxStringSize, char *clientGUID, char *cmdString  )
{
    int parseError = 1;
    char *u, *v, *w;

    strclr( clientGUID );
    strclr( cmdString );

    w = strstr( strIn, "<< " );
    if ( w != NULL ) {
        v = w + 3;
        if ( v[0] != 0 ) {
            if ( NULL != (u = strstr( v, picladminConfig.cmdPrefix ))) {
                if ( u == v ) {       // cmdPrefix is the first character of say string...
                    strlcpy( cmdString, u+strlen( picladminConfig.cmdPrefix ), maxStringSize );
                    strTrimInPlace( cmdString );
                    strlcpy( clientGUID, "00000000000000000", 1+RCONSTEAMIDSIZE );   // "root" ID
                    parseError = 0;
                    logPrintf( LOG_LEVEL_INFO, "picladmin", "RCON initiated command ::%s::[%s]::", strIn, cmdString );
                }
            }
        }
    }

    return parseError;
}


//  ==============================================================================================
//  picladminChatCB
//
//  The main entry point callback routine to parse the log string and execute 
//  admin command.
//
//
int picladminChatCB( char *strIn )
{
    char clientGUID[1024], cmdString[1024];

    if ( 0 == _commandParse( strIn, 1024, clientGUID, cmdString )) {    // parse for valid format
        logPrintf( LOG_LEVEL_DEBUG, "picladmin-chatCB", "strin  ::%s::\n", strIn );
        logPrintf( LOG_LEVEL_DEBUG, "picladmin-chatCB", "parsed ::%s::%s::\n", clientGUID, cmdString );
        _commandExecute( cmdString, clientGUID ) ;                     // execute the command
    }

#if 0
    // pre-auth method using simple apiIsAdmin call - deprecated but left here for short term
    // just in case a revert is needed for test purposes.
    //
    if ( 0 == _commandParse( strIn, 1024, clientGUID, cmdString )) {    // parse for valid format
        if (apiIsAdmin( clientGUID )) {                                    // check if authorized
            _commandExecute( cmdString, clientGUID ) ;                     // execute the command
        }
        else  {
            _respSay( picladminConfig.msgInvalid );                       // unauthorized message
        }
    }
#endif

    return 0;
}

//  ==============================================================================================
//  picladminRconCB
//
//  The main entry point callback routine to parse the log string and execute 
//  admin command coming from RCON.
//
//
int picladminRconCB( char *strIn )
{
    char clientGUID[1024], cmdString[1024];

    if ( 0 == _commandParseFromRCON( strIn, 1024, clientGUID, cmdString )) {    // parse for valid format
        _commandExecute( cmdString, clientGUID ) ;                     // execute the command
    }

    return 0;

}

//  ==============================================================================================
//  picladminInstallPlugin
//
//  This method is exported and is called from the main sissm module.
//
int picladminInstallPlugin( void )
{

    // Read the plugin-specific variables from the .cfg file 
    // 
    picladminInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( picladminConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           picladminClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           picladminClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 picladminInitCB );
    eventsRegister( SISSM_EV_RESTART,              picladminRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            picladminMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           picladminGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             picladminGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          picladminRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            picladminRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   picladminCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             picladminPeriodicCB );
    eventsRegister( SISSM_EV_SHUTDOWN,             picladminShutdownCB );
    eventsRegister( SISSM_EV_CLIENT_ADD_SYNTH,     picladminClientSynthAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL_SYNTH,     picladminClientSynthDelCB );
    eventsRegister( SISSM_EV_CHAT,                 picladminChatCB );
    eventsRegister( SISSM_EV_RCON,                 picladminRconCB );


    // Clear the developer varibles for debug use only.
    //
    p2pSetL( "dev.p2p.nokick", 0L );
    p2pSetL( "dev.p2p.humans", 0L );

    // clear the origin ID
    // 
    _setOriginID( "INVALID");

    // clear the Clan list
    // 
    apiClanClear();

    return 0;
}



