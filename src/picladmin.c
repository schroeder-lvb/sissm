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

static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char msgOk[NUM_RESPONSES][CFS_FETCH_MAX];
    char msgErr[NUM_RESPONSES][CFS_FETCH_MAX];
    char msgInvalid[NUM_RESPONSES][CFS_FETCH_MAX];

    char cmdPrefix[CFS_FETCH_MAX];

    char macros[NUM_MACROS][CFS_FETCH_MAX];

    int botMaxAllowed, botMinDefault;

    char prepBlow[CFS_FETCH_MAX], prepCapture[CFS_FETCH_MAX];
    char askBlow[CFS_FETCH_MAX], askCapture[CFS_FETCH_MAX];
    char warnRusher[CFS_FETCH_MAX];

    char reason[NUM_REASONS][CFS_FETCH_MAX];

    char contactInfo[CFS_FETCH_MAX];
    char rulesInfo[NUM_RULES][CFS_FETCH_MAX];
    int  rulesIntervalSec;

} picladminConfig;


static int _rulesIndex = -1;
static int _rulesTickCount = 0;


//  ==============================================================================================
//  Jump Pointer Data Structure
//

int _cmdHelp(), _cmdVersion(), _cmdMacros(), _cmdMacrosList();
int _cmdBotFixed(), _cmdBotScaled(), _cmdBotDifficulty();
int _cmdKillFeed(), _cmdFriendlyFire();
int _cmdRestart(), _cmdEnd(), _cmdReboot();
int _cmdBan(), _cmdKick();
int _cmdBanId(), _cmdUnBanId(),_cmdKickId();
int _cmdGameModeProperty(), _cmdRcon();
int _cmdInfo(), _cmdAllowIn(), _cmdSpam(), _cmdFast(), _cmdAsk(), _cmdPrep(), _cmdWarn();
int _cmdRules(), _cmdContact();
int _cmdMap(), _cmdMapList();

struct {

    char cmdShort[20];
    char cmdLong[40];
    char cmdHelp[40];
    int (*cmdFunction)();

}  cmdTable[] =  {

    { "help",   "help",          "help [command], help list",   _cmdHelp  },
    { "v",      "version",       "version [sissm]",         _cmdVersion },   

    { "bf",     "botfixed",      "botfixed [2-60]",         _cmdBotFixed },
    { "bs",     "botscaled",     "botscaled [2-60]",        _cmdBotScaled },
    { "bd",     "botdifficulty", "botdifficulty [0-10]",    _cmdBotDifficulty },
    { "x",      "execute",       "execute [alias]",         _cmdMacros },
    { "ml",     "macroslist",    "macroslist",              _cmdMacrosList },

    { "kf",     "killfeed",      "killfeed on|off",         _cmdKillFeed },
    { "ff",     "friendlyfire",  "friendlyfire on|off",     _cmdFriendlyFire },

    { "gmp",    "gamemodeproperty",  "gmp [cvar] {value}",  _cmdGameModeProperty },
    { "rcon",   "rcon",          "rcon [passthru]",        _cmdRcon },

    { "rr",     "roundrestart",  "roundrestart [now]",      _cmdRestart },
//  { "re",     "roundend",      "roundend [now]",          _cmdEnd },
    { "reboot", "reboot",        "reboot [now]",            _cmdReboot },

    { "b",      "ban",           "ban [partial name] {reason}",  _cmdBan },
    { "k",      "kick",          "kick [partial name] {reason}", _cmdKick },

    { "bi",     "banid",         "banid [steamid64] {reason}", _cmdBanId },
    { "ub",     "unbanid",       "unbanid [steamid64]",     _cmdUnBanId },
    { "ki",     "kickid",        "kickid [steamid64] {reason}", _cmdKickId },

    { "info",   "info",          "info",                    _cmdInfo },
    { "al",     "allowin",       "allowin [partial name]",  _cmdAllowIn },
    { "sp",     "spam",          "spam on|off",             _cmdSpam },
    { "fast",   "fast",          "fast on|off",             _cmdFast },

    { "a",      "ask",           "ask",                     _cmdAsk  },
    { "p",      "prep",          "prep",                    _cmdPrep },
    { "w",      "warn",          "warn",                    _cmdWarn },

    { "map",    "map",           "map name [night][ins]",   _cmdMap       },
    { "maplist","maplist",       "maplist",                 _cmdMapList   },

    { "rules",  "rules",         "rules",                   _cmdRules },
    { "contact","contact",       "contact",                 _cmdContact },

    { "*",      "*",             "*",                       NULL }

};

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
    for (i=0; i<strlen( rawLine ); i++) {
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
    for ( i=0; i<strlen(outReason); i++ ) {
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
    char outStr[1024];

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
    if (notFound) {
        strlcpy( outStr, "help [cmd], avail: ", 1024);
        i = 0;           
        while ( 1 == 1 ) {
            strlcat( outStr, cmdTable[i].cmdLong, 1024 );
            strlcat( outStr, " ", 1024 );
            if ( strlen(outStr) > 40 ) {
                apiSaySys( outStr );
                strclr( outStr );
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

// ===== "botfixed [2-60]"
//
int _cmdBotFixed( char *arg, char *arg2, char *passThru ) 
{  
    int errCode = 1, botCount;

    if (1 == sscanf( arg, "%d", &botCount )) {
        if ( ( botCount <=  picladminConfig.botMaxAllowed ) && ( botCount >= 2 ) ) {
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

// ===== "botscaled [2-60]"
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

    if (0 == strcmp( "now", arg ))  
        apiServerRestart( "Admin Command" );
    else 
        errCode = 1;

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

    if ( 17==strlen( arg ) ) {
        if (NULL != strstr( arg, "765611" )) {
            snprintf( cmdOut, 256, "banid %s 0 \"%s\"", arg, _reason( passThru ) );
            apiRcon( cmdOut, statusIn );
            apiSay( statusIn );
            errCode = 0;
        }
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

    if ( 17==strlen( arg ) ) {
        if (NULL != strstr( arg, "765611" )) {
            snprintf( cmdOut, 256, "unban %s", arg );
            apiRcon( cmdOut, statusIn );
            apiSay( statusIn );
            errCode = 0;
        }
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

    if ( IDSTEAMID64LEN == strlen( arg ) ) {
        if (NULL != strstr( arg, "765611" )) {
            snprintf( cmdOut, 256, "kick %s \"%s\"", arg, _reason( passThru) );
            apiRcon( cmdOut, statusIn );
            errCode = 0;
        }
    }

    _stddResp( errCode );   // ok or error message to game

    return errCode;
}

// ===== "ban [partial-name]"
// target must be online
//
int _cmdBan( char *arg, char *arg2, char *passThru  ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256], steamID[256];
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg, 3 ), 256 );
    if ( 0 != strlen( steamID ) ) {
        snprintf( cmdOut, 256, "ban %s 0 \"%s\"", steamID, _reason( passThru) );
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
    int errCode = 1;
    char cmdOut[256], statusIn[256], steamID[256];
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg, 3 ), 256 );
    if ( 0 != strlen( steamID ) ) {
        snprintf( cmdOut, 256, "kick %s \"%s\"", steamID, _reason( passThru ) );
        apiRcon( cmdOut, statusIn );
        apiSay( statusIn );
        errCode = 0;
    }

    if ( errCode != 0) _stddResp( errCode );   // ok or error message to game

    return errCode; 
}


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
           snprintf( cmdOut, 256, "'%s' is set to '%s'", arg, statusIn );
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
    }
    else {
        _stddResp( 1 );   // ok or error message to game
    }

    return errCode;
}

// ===== "map"
// Change maps 
//
int _cmdMap( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 0;
    int isNight = 0, isIns = 0;
    char *w, gameMode[80];
    int j;


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
        if ( apiIsSupportedGameMode( w ) ) {
            strlcpy( gameMode, w, 80 );
            break;
        }
    } 

    // change map
    errCode = apiMapChange( arg, gameMode, isIns, isNight );

    if ( errCode ) {
        apiSaySys( "Map/mode/side not found" );
    }
    return errCode;
}


// ===== "maplist"
// list maps 
//
int _cmdMapList( char *arg, char *arg2, char *passThru ) 
{ 
    char *w, *v, outBuf[256];
    int errCode = 0;
    int j, i, exitF = 0;;

    w = apiMapList();
    j = 0;
    if ( w != NULL ) { 
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
                strncat( outBuf, v, 256 );
                strncat( outBuf, " ", 256 );
            }
            apiSaySys( "%s", outBuf );
        }
    } 

    // apiSaySys( "Maps: %s", w );
    return errCode;
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
    picladminConfig.botMaxAllowed = (int) cfsFetchNum( cP, "picladmin.botMaxAllowed", 60.0 );
    picladminConfig.botMinDefault = (int) cfsFetchNum( cP, "picladmin.botMinDefault",  6.0 );
    if ( picladminConfig.botMaxAllowed < 2.0 ) picladminConfig.botMaxAllowed = 4.0;
    if ( picladminConfig.botMinDefault < 2.0 ) picladminConfig.botMinDefault = 2.0;

    // text to show in-game for !ask, !prep, and !warn
    //
    strlcpy( picladminConfig.prepBlow, cfsFetchStr( cP, "picladmin.prepBlow",  
        "*Planting Explosives/Ready to Blow!" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.prepCapture, cfsFetchStr( cP, "picladmin.prepCapture",  
        "*Testing Capture Point - Stepping On then Off" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.askBlow, cfsFetchStr( cP, "picladmin.askBlow",  
        "*BLOWING CACHE in 5-sec, <NEGATIVE> to halt" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.askCapture, cfsFetchStr( cP, "picladmin.askCapture",  
        "*BREACHING in 5-sec, <NEGATIVE> to halt" ), CFS_FETCH_MAX);
    strlcpy( picladminConfig.warnRusher, cfsFetchStr( cP, "picladmin.warnRusher",  
        "*Do NOT ENTER CAP/BLOW CACHE without ASKING TEAM" ), CFS_FETCH_MAX);

    // Read the "reasons" preset strings for ban/banid/kick/kickid commmands
    //
    for ( i=0; i<NUM_REASONS; i++ ) {
        snprintf( varArray, 256, "picladmin.reason[%d]", i );
        strlcpy( picladminConfig.reason[i],     cfsFetchStr( cP, varArray,  "" ),   CFS_FETCH_MAX);
        replaceDoubleColonWithBell( picladminConfig.reason[i] );
    }

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

int picladminGameStartCB( char *strIn ) { _resetP2PVars(); return 0; }
int picladminMapChangeCB( char *strIn ) { _resetP2PVars(); return 0; }
int picladminGameEndCB( char *strIn )   { _resetP2PVars(); return 0; }
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
int picladminRoundEndCB( char *strIn ) { return 0; }
int picladminCapturedCB( char *strIn ) { return 0; }
int picladminShutdownCB( char *strIn ) { return 0; }
int picladminClientSynthDelCB( char *strIn ) { return 0; }
int picladminClientSynthAddCB( char *strIn ) { return 0; }


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

    return errCode; 
}


//  ==============================================================================================
//  _commandParse
//
//  Parse the log line 'say' chat log into originator GUID and the said text string
//  [2019.08.30-23.39.33:262][176]LogChat: Display: name(76561198000000001) Global Chat: !ver sissm
//  [2019.09.15-03.44.03:500][993]LogChat: Display: name(76561190000000002) Team 0 Chat: !v
//
#define LOGCHATPREFIX  "LogChat: Display: "
#define LOGCHATMIDDLE  "Chat: "
#define LOGSTEAMPREFIX "76561"
#define LOGSTEAMIDSIZE  (17)

int _commandParse( char *strIn, int maxStringSize, char *clientGUID, char *cmdString  )
{
    int parseError = 1;
    char *t, *u, *v, *w, *s, *r;
  
    t = u = v = w = s = r = NULL;
    strclr( clientGUID );
    strclr( cmdString );

    // Parse the string to produce clientGUID and cmdString
    // If not properly formatted then set parseError=1
    //   
    t = strstr( strIn, LOGCHATPREFIX );                         // T points to "LogChat: Display:"
    if ( t != NULL ) u = &t[ strlen( LOGCHATPREFIX ) ];          // U point to start of admin name
    if ( u != NULL ) v = strstr( u, LOGCHATMIDDLE );             // V now points start of "Chat: "
    if ( v != NULL ) w = &v[ strlen( LOGCHATMIDDLE ) ];            // W points to start of command
    if ( u != NULL ) s = strstr( u, LOGSTEAMPREFIX );              // S points to start of 7656...

    if ( w != NULL ) r = strstr( w, picladminConfig.cmdPrefix );                      
    if ( r != NULL ) r = &r[ strlen( picladminConfig.cmdPrefix ) ];   // R to command minus prefix

    if ( s != NULL ) strlcpy( clientGUID, s, 1+LOGSTEAMIDSIZE );            // extract Client GUID
    if ( r != NULL ) strlcpy( cmdString, r, maxStringSize );         // extract Cmd without prefix  

    if (( s != NULL ) && ( r != NULL )) { 
        if ( (w + strlen( picladminConfig.cmdPrefix)) ==  r ) {     // verify prefix is first char
            parseError = 0; 
            strTrimInPlace( cmdString );
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

    return 0;
}


