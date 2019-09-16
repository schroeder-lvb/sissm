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

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "picladmin.h"



//  ==============================================================================================
//  Data definition 
//

#define NUM_RESPONSES  (5)
#define NUM_MACROS     (10)

static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char msgOk[NUM_RESPONSES][CFS_FETCH_MAX];
    char msgErr[NUM_RESPONSES][CFS_FETCH_MAX];
    char msgInvalid[NUM_RESPONSES][CFS_FETCH_MAX];

    char cmdPrefix[CFS_FETCH_MAX];

    char macros[NUM_MACROS][CFS_FETCH_MAX];

} picladminConfig;


//  ==============================================================================================
//  Jump Pointer Data Structure
//

int _cmdHelp(), _cmdVersion(), _cmdMacros(), _cmdMacrosList();
int _cmdBotFixed(), _cmdBotScaled(), _cmdBotDifficulty();
int _cmdKillFeed(), _cmdFriendlyFire();
int _cmdRestart(), _cmdEnd(), _cmdReboot();
int _cmdBan(), _cmdKick();
int _cmdBanId(), _cmdKickId();
int _cmdGameModeProperty(), _cmdRcon();

struct {

    char cmdShort[20];
    char cmdLong[40];
    char cmdHelp[40];
    int (*cmdFunction)();

}  cmdTable[] =  {

    { "help",   "help",          "help [command], help list",   _cmdHelp  },
    { "v",      "version",       "version [sissm]",        _cmdVersion },   

    { "bf",     "botfixed",      "botfixed [2-60]",        _cmdBotFixed },
    { "bs",     "botscaled",     "botscaled [2-60]",       _cmdBotScaled },
    { "bd",     "botdifficulty", "botdifficulty [0-10]",   _cmdBotDifficulty },
    { "m",      "macros",        "macros [alias]",         _cmdMacros },
    { "ml",     "macroslist",    "macroslist",             _cmdMacrosList },

    { "kf",     "killfeed",      "killfeed on|off",        _cmdKillFeed },
    { "ff",     "friendlyfire",  "friendlyfire on|off",    _cmdFriendlyFire },

    { "gmp",   "gamemodeproperty",  "gmp [cvar] {value}",  _cmdGameModeProperty },
    { "rcon",  "rcon",           "rcon [passthru]",        _cmdRcon },

    { "rr",     "roundrestart",  "roundrestart [now]",     _cmdRestart },
//  { "re",     "roundend",      "roundend [now]",         _cmdEnd },
    { "reboot", "reboot",        "reboot [now]",           _cmdReboot },

    { "b",      "ban",           "ban [partial name]",     _cmdBan },
    { "k",      "kick",          "kick [partial name]",    _cmdKick },

    { "bi",     "banid",         "banid [steamid64]",      _cmdBanId },
    { "ki",     "kickid",        "kickid [steamid64]",     _cmdKickId },

    { "*",      "*",             "*",                      NULL }

};

// ===== write ok, error, or unauthorized status to in-game chat
//
static int _respSay( char msgArray[NUM_RESPONSES][CFS_FETCH_MAX] )
{
    int msgIndex ;

    msgIndex = rand() % NUM_RESPONSES;
    apiSay( msgArray[ msgIndex ] );       

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
        w = getWord( picladminConfig.macros[i], 0, "::");   // get the macro label
        if ( w != NULL ) {                                  // skip blank entries (possible)
            if ( 0 == strcmp( arg, w ) ) {                  // check if operator cmd match
                while ( NULL != ( w = getWord( picladminConfig.macros[i], j++, "::" )) ) {
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
    char *w;
    char listOut[ 1024 ];

    strlcpy( listOut, "Macros: ", 1024 ); 

    for (i = 0; i<NUM_MACROS; i++) {
        if ( 0 != strlen( picladminConfig.macros[i] )) {    // skip blank entries (possible)
            strlcat( listOut, getWord( picladminConfig.macros[i], 0, "::" ), 1024 );
            strlcat( listOut, " ",                       1024 );
            errCode = 0;
        }
    }
    if ( errCode ) {
        apiSay( "No Macros defined in config" );
    }
    else {
        apiSay( listOut );
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
                apiSay("'%s' or %s", cmdTable[i].cmdShort, cmdTable[i].cmdHelp);
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
            strcat( outStr, cmdTable[i].cmdLong );
            strcat( outStr, " " );
            i++;
            if ( 0 == strcmp( "*", cmdTable[i].cmdShort )) break; 
        }
        apiSay( outStr );
    }
   
    return 0; 
}

// ===== "version"
//
int _cmdVersion( char *arg, char *arg2, char *passThru ) 
{ 
    apiSay( sissmVersion() );
    return 0; 
}

// ===== "botfixed [2-60]"
//
int _cmdBotFixed( char *arg, char *arg2, char *passThru ) 
{  
    int errCode = 1, botCount;

    if (1 == sscanf( arg, "%d", &botCount )) {
        if ((botCount <= 60) && (botCount >= 2)) {
           apiGameModePropertySet( "minimumenemies", _int2str( botCount/2 ) ); 
           apiGameModePropertySet( "maximumenemies", _int2str( botCount/2 ) );  
           errCode = 0;
        }
    }
    _stddResp( errCode );   // ok or error message to game
    
    return errCode; 
}

// ===== "botscaled [2-60]"
//
int _cmdBotScaled( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 1, botCount;

    if (1 == sscanf( arg, "%d", &botCount )) {
        if ((botCount <= 60) && (botCount >= 2)) {
           apiGameModePropertySet( "minimumenemies", "3" );
           apiGameModePropertySet( "maximumenemies", _int2str( botCount / 2 ) );
           errCode = 0;
        }
    }

    _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "botdifficulty [0-10]"
//
int _cmdBotDifficulty( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 1, botDifficulty;
    char strOut[256];
    double botDifficultyF;

    if (1 == sscanf( arg, "%d", &botDifficulty )) {
        if ((botDifficulty <= 10) && (botDifficulty >= 0)) {
           botDifficultyF = ((double) botDifficulty ) / 10.0;
           snprintf( strOut, 256, "%4.1lf", botDifficultyF);
           apiGameModePropertySet( "aidifficulty", strOut );
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
        apiServerRestart();
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
            snprintf( cmdOut, 256, "banid %s", arg );
            apiRcon( cmdOut, statusIn );
            errCode = 0;
        }
    }

    _stddResp( errCode );   // ok or error message to game

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
            snprintf( cmdOut, 256, "kick %s", arg );
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
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg ), 256 );
    if ( 0 != strlen( steamID ) ) {
        snprintf( cmdOut, 256, "ban %s", steamID );
        apiRcon( cmdOut, statusIn );
        errCode = 0;
    }

    _stddResp( errCode );   // ok or error message to game

    return errCode; 
}

// ===== "kick [partial-name]"
// target must be online
//
int _cmdKick( char *arg, char *arg2, char *passThru ) 
{ 
    int errCode = 1;
    char cmdOut[256], statusIn[256], steamID[256];
    strlcpy( steamID, rosterLookupSteamIDFromPartialName( arg ), 256 );
    if ( 0 != strlen( steamID ) ) {
        snprintf( cmdOut, 256, "kick %s", steamID );
        apiRcon( cmdOut, statusIn );
        errCode = 0;
    }

    _stddResp( errCode );   // ok or error message to game

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
           apiSay( "GMP Readback Error");
        else {
           snprintf( cmdOut, 256, "'%s' is set to '%s'", arg, statusIn );
           apiSay( cmdOut );
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
    char statusIn[4096], cmdOut[256];
    char *rconArgs;
    int  i, supportedRconCommand = 1;
    char *unsupportedRcon[] =  { "help", "listplayers", "listbans", "maps", "scenarios", "listgamemodeproperties", "*" };

    supportedRconCommand = !foundMatch( arg, unsupportedRcon, 1 );

    if ( supportedRconCommand ) {
        rconArgs = strstr( passThru, " " );
        if ( rconArgs != NULL ) {  
            bytesRead = apiRcon( rconArgs, statusIn );
            if ( bytesRead != 0 )  {
                errCode = 0;
                apiSay( statusIn );
            }
            else {
                apiSay( "RCON Interface Error" );
            }
        }
    }
    else {
        apiSay( "Unsupported RCON command" );
    }
    return errCode;
}


//  ==============================================================================================
//  picladminInitConfig
//
//  Initialize configuration variables for this plugin
//
int picladminInitConfig( void )
{
    cfsPtr cP;

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

    // Read the operator-specified "macro" (alias) sequence
    //
    strlcpy( picladminConfig.macros[0],     cfsFetchStr( cP, "picladmin.macros[0]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[1],     cfsFetchStr( cP, "picladmin.macros[1]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[2],     cfsFetchStr( cP, "picladmin.macros[2]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[3],     cfsFetchStr( cP, "picladmin.macros[3]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[4],     cfsFetchStr( cP, "picladmin.macros[4]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[5],     cfsFetchStr( cP, "picladmin.macros[5]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[6],     cfsFetchStr( cP, "picladmin.macros[6]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[7],     cfsFetchStr( cP, "picladmin.macros[7]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[8],     cfsFetchStr( cP, "picladmin.macros[8]",  "" ),   CFS_FETCH_MAX);
    strlcpy( picladminConfig.macros[9],     cfsFetchStr( cP, "picladmin.macros[9]",  "" ),   CFS_FETCH_MAX);

    strlcpy( picladminConfig.cmdPrefix,      cfsFetchStr( cP, "picladmin.cmdPrefix",      "!" ),              CFS_FETCH_MAX);

    // if nil cmdPrefix is in the .cfg file, this will cause every command to be admin command, so
    // we override that here.
    //
    if ( 0 == strlen( picladminConfig.cmdPrefix )) strcpy( picladminConfig.cmdPrefix, "!" );

    cfsDestroy( cP );
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
int picladminMapChangeCB( char *strIn ) { return 0; }
int picladminGameStartCB( char *strIn ) { return 0; }
int picladminGameEndCB( char *strIn ) { return 0; }
int picladminRoundStartCB( char *strIn ) { return 0; }
int picladminRoundEndCB( char *strIn ) { return 0; }
int picladminCapturedCB( char *strIn ) { return 0; }
int picladminPeriodicCB( char *strIn ) { return 0; }
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

    // zero the physical stores
    //
    strcpy( cmdOut, "" );  strcpy( arg1Out, "");  strcpy( arg2Out, "" );

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
            errCode = (*cmdTable[i].cmdFunction)( arg1Out, arg2Out, cmdString );
            if ( errCode == 0 ) {
                logPrintf( LOG_LEVEL_INFO, "picladmin", "Admin [%s] executed: %s", originID, cmdString );
            }
            break;
        }
        i++;
        if ( 0==strcmp( cmdTable[i].cmdShort, "*") )
            break;
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
    strcpy( clientGUID, "" );
    strcpy( cmdString, "" );

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
        if (apiIsAdmin( clientGUID )) {                                    // check if authorized
            _commandExecute( cmdString, clientGUID ) ;                     // execute the command
        }
        else  {
            _respSay( picladminConfig.msgInvalid );                       // unauthorized message
        }
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
    return 0;
}


