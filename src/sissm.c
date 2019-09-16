//  ==============================================================================================
// 
//  Module: SISSM "Simple Insurgency Sandstorm Server Manager"
//
//  Description:  
//  Top level, init, main loop, event & polling dispatcher
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under the MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================
//

#define SISSM_RESTRICTED   (0)           // 1=build shell-restricted version 0=full shell access

#if SISSM_RESTRICTED
#define VERSION    "SISSM v0.0.28 Alpha 20190916 - Test & Eval Only [Restricted Edition]"
#else
#define VERSION    "SISSM v0.0.28 Alpha 20190916 - Test & Eval Only"
#endif

#define COPYRIGHT  "(C) 2019 JS Schroeder, released under the MIT License"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <signal.h>

#ifdef _WIN32
#include "winport.h"     // sleep/usleep functions
#else 
#include <unistd.h>
#endif

#include "bsd.h"
#include "util.h"
#include "log.h"
#include "cfs.h"
#include "ftrack.h"
#include "events.h"
#include "alarm.h"
#include "rdrv.h"
#include "roster.h"

// Plugins INTERNAL 
//
#include "api.h"

// Plugins EXTERNAL
//
#include "pit001.h"
#include "pirebooter.h"
#include "pigreetings.h"
#include "pigateway.h"
#include "piantirush.h"
#include "pisoloplayer.h"
#include "piwebgen.h"
#include "pioverride.h"
#include "picladmin.h"


//  ==============================================================================================
//  Data definition  
//
//  Master State Definitions
//
#define SM_STARTUP               (1001)    // internal initialization
#define SM_FILE_TRY_REOPEN       (2001)    // retries to open input logfile
#define SM_FILE_CHECK_ACTIVITY   (2002)    // checks to see if logfile (server) is alive
#define SM_SYS_RESTART           (3001)    // issues a server restart
#define SM_POLLING_INIT          (4001)    // init code for below
#define SM_POLLING_TRACKING      (4002)    // active loop monitoring server behavior changes
#define SM_INVALID               (-1)      // internal error

#define SISSM_POLLING_INTERVAL_MICROSEC (50000)        // 50ms file polling interval

// Configuration variables read from the .cfg file
// at start of this app.  Each plugin (including this one) should 
// have own configuration structure and a method to read it.
//
static struct {

    char logFile[CFS_FETCH_MAX];
    int  loglevel;
    int  logEchoToScreen;
    
    char serverName[CFS_FETCH_MAX];  

    int  rconPort;
    char rconPassword[CFS_FETCH_MAX];
    char rconIP[CFS_FETCH_MAX];

    char gameLogFile[CFS_FETCH_MAX];
    char configFile[CFS_FETCH_MAX];          // this one is set by argv[] not from the config file

    char restartScript[CFS_FETCH_MAX];                  // command to invoke to restart the server
    int  restartDelay;                          // number of seconds required for server to reboot

    int  gracefulExit;           // 1=install sig handler and generate SIGTERM event to the plugins
    
} sissmConfig;



//  ==============================================================================================
//  sissmSigHandler
//
//  SIGKILL and SIGTERM handler
//  ==============================================================================================

volatile sig_atomic_t gracefulKill = 0;

#ifdef _WIN32
BOOL WINAPI sissmSigHandler( DWORD signum )
{
    BOOL retValue;

    switch ( signum ) {

    // handle soft kills including control-c exits
    // 
    case CTRL_C_EVENT:  case CTRL_CLOSE_EVENT:
        gracefulKill = 1;
        retValue = TRUE;
        break;

    // do not handle hard kills
    // 
    case CTRL_BREAK_EVENT: case CTRL_LOGOFF_EVENT: case CTRL_SHUTDOWN_EVENT:
        retValue = FALSE;
        break;
    
    // unknown cases
    // 
    default:
        retValue = FALSE;
        break;
    }

    return( retValue ); 
}
#else
void sissmSigHandler( int signum )
{
    gracefulKill = 1;         // checked by master loop for graceful exit
}
#endif
 
//  ==============================================================================================
//  sissmSigInstall
//
//  SIGKILL and SIGTERM handler
//  ==============================================================================================

int sissmSigInstall( void )
{
    int errCode = 1;

#ifdef _WIN32
    // For Windows taskill/close vs taskkill/f  vs ctrl-C
    // All this is decided by the handler function
    //
    if (SetConsoleCtrlHandler(sissmSigHandler, TRUE)) { errCode = 0; }
#else 
    // For Linux kill is SIGTERM, kill -9 is SIGKILL, Ctrl-C is SIGINT
    // Don't handle SIGKILL!
    //
    signal(SIGINT,  sissmSigHandler );        // graceful Ctrl-C
    signal(SIGTERM, sissmSigHandler );        // graceful 'kill'
    errCode = 0;
#endif

    return errCode;
}

//  ==============================================================================================
//  sissmInitLogAndConfig
//
//  Reads "sissm" block of parameters from the configuration file
//
int sissmInitLogAndConfig( char *configPath )
{
    cfsPtr cP;

    strlcpy( sissmConfig.configFile, configPath, CFS_FETCH_MAX );      // store it for the plugins

    cP = cfsCreate( configPath );

    // read the log info first, so that log module can be used to report
    // status of remaining configuration reads.
    //
    strlcpy( sissmConfig.logFile, cfsFetchStr( cP, "sissm.logfile", "sissm.log" ), CFS_FETCH_MAX );
    strlcpy( sissmConfig.serverName, cfsFetchStr( cP, "sissm.serverName", "Unknown Server" ), CFS_FETCH_MAX );
    sissmConfig.loglevel = (int) cfsFetchNum( cP, "sissm.loglevel", 2.0 );
    sissmConfig.logEchoToScreen = (int) cfsFetchNum( cP, "sissm.logechotoscreen", LOG_LEVEL_DEBUG );

    // Initialize the log system based on parameters read above
    // Options for loglevel: _DEBUG _INFO _WARN _CRITICAL 
    // 
    logPrintfInit( sissmConfig.loglevel, sissmConfig.logFile, sissmConfig.logEchoToScreen ); 

    // read the RCON parameters
    //
    strlcpy( sissmConfig.rconIP, cfsFetchStr( cP, "sissm.rconip", "127.0.0.1" ), CFS_FETCH_MAX );
    strlcpy( sissmConfig.rconPassword, cfsFetchStr( cP, "sissm.rconpassword", "" ), CFS_FETCH_MAX );
    sissmConfig.rconPort = (int) cfsFetchNum( cP, "sissm.rconport", 27015 );

    // read the game log file path
    //
    strlcpy( sissmConfig.gameLogFile, cfsFetchStr( cP, "sissm.gamelogfile", "Insurgency.log" ), CFS_FETCH_MAX );

    // read the server restart script
    //
#if SISSM_RESTRICTED
    strcpy( sissmConfig.restartScript, "" );
#else
    strlcpy( sissmConfig.restartScript, cfsFetchStr( cP, "sissm.restartscript", "" ), CFS_FETCH_MAX );
#endif
    sissmConfig.restartDelay = (int) cfsFetchNum( cP, "sissm.restartdelay", 30.0 );

    // graceful exit - when enabled, SISSM will install a signal handler for terminating itself to 
    // generate a SIGTERM event to the plugins.
    //
    sissmConfig.gracefulExit = (int) cfsFetchNum( cP, "sissm.gracefulExit", 1.0 );

    cfsDestroy( cP );

    return 0;
}

//  ==============================================================================================
//  sissmVersion
//
//  Returns the version string.
//  
char *sissmVersion( void )
{
    static char versionString[ 255 ];

    strlcpy( versionString, VERSION, 255 );
    return( versionString );
}

//  ==============================================================================================
//  sissmGetConfigPath
//
//  Returns a configuration path.  This function is exported so that plugins can use and
//  read from the same configuratino file used by the main SISSM module.
//
char *sissmGetConfigPath( void )
{
    static char systemConfigFilePath[256];
    strlcpy( systemConfigFilePath, sissmConfig.configFile, 256 );
    return( systemConfigFilePath );
}

//  ==============================================================================================
//  sissmInitPlugins
//
//  Inoke plugin initialization - this includes both internal plugins (sissm core) as well as
//  external/third-party plugins (by convension prefixed by "pi" name).
//
int sissmInitPlugins( void )
{
    int errCode = 0;

    // "SISSM" core internal plugins - do not touch
    //
    apiInit();                                             // must be first one called!!!!

    // "Included" - for customizations
    //
    pirebooterInstallPlugin();                    // server side time-based  auto-rebooter
    pigreetingsInstallPlugin();                               // player greetings, notices
    pigatewayInstallPlugin();         // 'badname' and 'priority slots' connectino gateway
    piantirushInstallPlugin();         // anti-rush dual-rate throttler for capture points
    pisoloplayerInstallPlugin();                     // solo player counterattack disabler
    piwebgenInstallPlugin();                                       // web status generator
    pioverrideInstallPlugin();                   // gamemodeproperty override for rulesets
    picladminInstallPlugin();     // admin in-game command executioner from the chat input

    // "Third Party" Plugins - for customizations
    //
    pit001InstallPlugin();                       // example template for plugin develoeprs

    return errCode;
}

//  ==============================================================================================
//  sissmServerRestart
//
//  Call this method to request a server restart.  
//
static int _sissmRestartRequest = 0;

void sissmServerRestart( void ) { 
    _sissmRestartRequest = 1; 
}

//  ==============================================================================================
//  sissmServerRestartpending
//
//  Checks to see if server restart is pending, and clears the pending indicator
//
int sissmServerRestartPending( void ) {
    int retValue = 0;
    if (_sissmRestartRequest ) {
        _sissmRestartRequest = 0;
	retValue = 1;
    }
    return retValue;
}

//  ==============================================================================================
//  sissmInitInternal
//
//  Internal initialization - clear & setup 'core' event and alarm handlers
//
int sissmInitInternal( void )
{
    int errCode = 0;

    alarmInit();
    eventsInit();

    return errCode;
}


//  ==============================================================================================
//  sissmRestartServer
//
//  Server soft-restart via the RCON quit command
//
int sissmRestartServer( void )
{
    char cmdOut[256], statusIn[256];
    int errCode = 0;

    if (0==strlen( sissmConfig.restartScript )) {
        strcpy( cmdOut, "quit" );
        apiRcon( cmdOut, statusIn );
    }
    else {
        system( sissmConfig.restartScript );                       // hard restart from OS
    }
    sleep( sissmConfig.restartDelay );

    return errCode;
}

//  ==============================================================================================
//  sissmSplash
//
//  Splash Screen (intro)
//  
void sissmSplash( void ) 
{
    logPrintf(LOG_LEVEL_INFO, "sissm", "--------------------------------------" );
    logPrintf(LOG_LEVEL_INFO, "sissm", "%s", VERSION);
    logPrintf(LOG_LEVEL_INFO, "sissm", "%s", COPYRIGHT);
    return;
}

//  ==============================================================================================
//  sissmDiagnostics
//
//  Pre-run check for operator misconfigurations
// 
int sissmDiagnostics( void )
{
    int errCode = 0;

    // Check RCON configuration
    //
    if ( (sissmConfig.rconPort < 1024) || (sissmConfig.rconPort > 32767) ) {
        printf("\n** CONFIGURATION ERROR: Invalid Rcon Port is set in .cfg\n");
        printf("Please fix rconPort '%d'.\n", sissmConfig.rconPort );
        errCode = 1;
    }
    if (0 == strlen( sissmConfig.rconPassword) ) {
        printf("\n** CONFIGURATION ERROR: Rcon password is not set in .cfg\n");
        printf("Please fix rconPassword\n");
        errCode = 1;
    }
    if ( 0 == strncmp( "000.000.000.000", reformatIP( sissmConfig.rconIP ), 16 ))  {
        printf("\n** CONFIGURATION ERROR: Invalid server IP set in .cfg\n"); 
        printf("Please fix rconIP '%s'\n", sissmConfig.rconIP );
        errCode = 1;
    }

    // Check critical files
    //
    if ( !isReadable( sissmConfig.gameLogFile ) ) {
        printf("\n** CONFIGURATION ERROR: Game log file not found or unable to read in .cfg\n");
        printf("Please fix gameLogFile: '%s'\n", sissmConfig.gameLogFile );
        errCode = 1;
    }

    if ( errCode ) {
        printf("\nPress ^C now, or SISSM will proceed to run in 15 seconds\n");
        sleep( 15 );
    }

    return errCode;
}



//  ==============================================================================================
//  sissmMainLoop
//
//  Main master loop - this is a single-thread sequential processing loop 
//  
//
int sissmMainLoop( void )
{
    int errCode = 0;
    int masterState = SM_STARTUP;
    ftrackObj *fPtr = NULL;
    char strBuffer[4096];
    unsigned long timePrev;          // for tracking periodic 1.0 Hz call


    // Log some general info
    //
    logPrintf(LOG_LEVEL_INFO, "sissm", "Server ID: ::%s::", sissmConfig.serverName );
    if ( sissmConfig.gracefulExit ) 
        logPrintf(LOG_LEVEL_CRITICAL, "sissm", "** Warning: Console '^C' may take several seconds to process");
    logPrintf(LOG_LEVEL_INFO, "sissm", "State - SM_STARTUP" );

    timePrev = time( NULL );

    for ( ;; ) {

        //  0. Synchronized SIGTERM exit process
        //
        if ( gracefulKill ) {
            logPrintf( LOG_LEVEL_CRITICAL, "sissm", "######## SIGTERM Exit #######" );
            break;
        }

	//  1. Server restart supercedure
	//
        if (sissmServerRestartPending()) masterState = SM_SYS_RESTART;     // restart supercedure

        //  2. Logfile tracking state machine
        //   
        switch ( masterState ) {
        case SM_STARTUP:
            logPrintf(LOG_LEVEL_INFO, "sissm", "State - SM_STARTUP");
            if ( fPtr == NULL ) {
                sleep( 1 );
                fPtr = ftrackOpen( sissmConfig.gameLogFile );
            }
            if (fPtr != NULL)  {
                logPrintf( LOG_LEVEL_CRITICAL, "sissm", "Tracking game logfile ::%s::", 
                    sissmConfig.gameLogFile );
                ftrackTailOfFile( fPtr, strBuffer, sizeof( strBuffer ), 1 );       // seek to end
                masterState = SM_POLLING_INIT;
            }
            else {
                logPrintf( LOG_LEVEL_CRITICAL, "sissm", "Trying to open game logfile ::%s::", 
                    sissmConfig.gameLogFile );
            }
            break;
        case SM_POLLING_INIT:
            logPrintf(LOG_LEVEL_INFO, "sissm", "State - SM_POLLING_INIT->TRACKING");
            eventsDispatch( "~INIT~" );
            ftrackResync( fPtr );      //      check for log file rotate and follow
            masterState = SM_POLLING_TRACKING;
            break;
        case SM_POLLING_TRACKING:
            if ( 0 == ftrackTailOfFile( fPtr, strBuffer, sizeof( strBuffer ), 0 ) ) {
                logPrintf(LOG_LEVEL_RAWDUMP, "sissm", "::%s::", strBuffer);
                eventsDispatch( strBuffer );
            }
            else { 
                usleep( SISSM_POLLING_INTERVAL_MICROSEC );
            }
            break;
        case SM_SYS_RESTART:
            logPrintf(LOG_LEVEL_INFO, "sissm", "State - SM_SYS_RESTART");
            eventsDispatch( "~RESTART~" );
            ftrackClose( fPtr );
            fPtr = NULL;
            sissmRestartServer();
            masterState = SM_STARTUP;
            break;
        default:
            ;;
        }

        // 3. periodic callbacks and alarms processing
	// currently at 1.0Hz polling rate
        //   
        if ( timePrev != time( NULL ) ) {
	    alarmDispatch();
            eventsDispatch( "~PERIODIC~" );
            timePrev = time( NULL );
            ftrackResync( fPtr );                          // check for log file rotate and follow
        }
    }

    // 4. graceful exit
    // This only happens if "sissm.gracefulExit" flag is set in the .cfg file.
    // Before SISSM shuts down, generate a synthetic event to all plugins so that 
    // they can take actions to leave the server in a playable state without SISSM.
    //
    eventsDispatch( "~SIGTERM~" );

    return errCode;
}


//  ==============================================================================================
//  main
//
//  This is the application starting point.  SISSM requires one and only one parameter:  
//  configuration file. All configuration variables are defined in the configuration file.
//  The idea is that you have custom configuration file per the game server, and multiple instances
//  of SISSM can be run on a same server.
//

#define SISSM_DEFAULT_CONFIG_NAME               "sissm_default.cfg"

int main( int argc, char *argv[] )
{
    int errCode = 0;
    char configFileName[ 256 ];

    // Check the arguments
    //
    switch ( argc ) {
    case 1:              // no parameter, check if default exists, else show help  
       if ( isReadable( SISSM_DEFAULT_CONFIG_NAME ) )  {
           strlcpy( configFileName, SISSM_DEFAULT_CONFIG_NAME, 256 );
           printf("\n%s. Using %s\n\n", VERSION, SISSM_DEFAULT_CONFIG_NAME);
       }
       else
           errCode = 1;
       break;
    case 2:              // single parameter, assume operator specified  sissm.cfg
        strlcpy( configFileName, argv[1], 256 );
        break;
    default:             // all other cases show help and do not run
        errCode = 1;
    }
    if (  errCode ) printf("\n%s\nSyntax: sissm config-file\n\n", VERSION);

    // Initialize the Config reader, ^C handler, Log Systems, then the Plugins
    //
    if ( !errCode ) errCode = sissmInitLogAndConfig( configFileName );  
    if ( !errCode ) sissmSplash();
    if ( !errCode ) errCode = sissmInitInternal();
    if ( !errCode ) errCode = sissmInitPlugins(); 
 
    // Run quick diagnostics for minimal parameters required to run
    //
    if ( !errCode ) sissmDiagnostics();

    // Optinoally install a SIGTERM handler to intercept ^C user
    // input - this allows individual plugins to take action to leave the server in
    // a playable state in the absense of SISSM.
    //
    if ( sissmConfig.gracefulExit ) sissmSigInstall();

    // Initialize the system, plugins, then spin the main loop
    //
    if ( !errCode ) errCode = sissmMainLoop();

    return( errCode );
}


