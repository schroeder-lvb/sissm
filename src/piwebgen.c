//  ==============================================================================================
//
//  Module: PIWEBGEN
//
//  Description:
//  Generates server status html file - periodic and/or change event driven
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

#include "piwebgen.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    char webFileName[CFS_FETCH_MAX];       // name of HTML output
    unsigned int  updateIntervalSec;       // forced periodic update rate (even if same data)
    int  updateOnChange;                   // update only when changed
    char webminURL[128];                   // webmin instruction e.g., www.mysite.com/webmin
    char directConnect[CFS_FETCH_MAX];     // direct connect instructions e.g., 123.123.123.123:12345
    char line2[CFS_FETCH_MAX];             // 2nd line title (comes after the server name
    int  hyperlinkFormat;                  // 0=simple output, 1=player names as hyperlinks
    int  autoRefreshHeader;                // 1=create html page autorefresh header, 0=none
    int  commTimeoutSec;                   // # sec of RCON failure to indicate "fail" on web

} piwebgenConfig;


//  ==============================================================================================
//  piwebgenInitConfig
//
//  Read parameters from the configuration file.
// 
//
int piwebgenInitConfig( void )
{
    cfsPtr cP;

    cP = cfsCreate( sissmGetConfigPath() );

    // read "piwebgen.pluginstate" variable from the .cfg file
    piwebgenConfig.pluginState = (int) cfsFetchNum( cP, "piwebgen.pluginState", 0.0 );  // disabled by default

    // read "piwebgen.StringParameterExample" variable from the .cfg file
    strlcpy( piwebgenConfig.webFileName,  cfsFetchStr( cP, "piwebgen.webFileName",  "sissm.html" ), CFS_FETCH_MAX );
    piwebgenConfig.updateIntervalSec = (int) cfsFetchNum( cP, "piwebgen.updateIntervalSec", 60 );
    piwebgenConfig.updateOnChange = (int) cfsFetchNum( cP, "piwebgen.updateOnChange", 1 );
    piwebgenConfig.autoRefreshHeader = (int) cfsFetchNum( cP, "piwebgen.autoRefreshHeader", 0 );
    strlcpy( piwebgenConfig.webminURL, cfsFetchStr( cP, "piwebgen.webminURL", "" ), 128 );
    strlcpy( piwebgenConfig.directConnect, cfsFetchStr( cP, "piwebgen.directConnect", ""), CFS_FETCH_MAX );
    strlcpy( piwebgenConfig.line2, cfsFetchStr( cP, "piwebgen.line2", ""), CFS_FETCH_MAX );
    piwebgenConfig.hyperlinkFormat = (int) cfsFetchNum( cP, "piwebgen.hyperlinkFormat", 1 );
    piwebgenConfig.commTimeoutSec = (int) cfsFetchNum( cP, "piwebgen.commTimeoutSec", 120 );

    cfsDestroy( cP );
    return 0;
}


//  ==============================================================================================
//  _convertNameToHyperlink
//
//  This routine converrts: "76560000000000001 111.001.034.025 Random Person "
//  To: <a href="http://steamcommunity.com/profiles/76560000000000001/">Random Person</a>
//  To: <a href="http://steamcommunity.com/profiles/76560000000000001/" target="_blank">Random Person</a>
// <a href="xyz.html" target="_blank"> Link </a>
//
#define STEAM_PROFILE  "<a href=\"http://steamcommunity.com/profiles/76560000000000001/\" target=\"_blank\">"

static char *_convertNameToHyperlink( char *identString )
{
    static char retStr[256];

    strlcpy( retStr, STEAM_PROFILE, 256 );                                    // copy the template
    strncpy( &retStr[44], identString, 17 );                        // insert/substitute SteamID64
    strlcat( retStr, &identString[34 ], 256 );                          // append name (text part)
    strlcat( retStr, "</a>", 256 );                                             // close with </a>
    return( retStr );
}


//  ==============================================================================================
//  _genWebFile
//
//  Generates a HTML status file using current server state
//  
//
#define PIWEBGEN_MAXROSTER   (16*1024)

static int _genWebFile( void )
{
    FILE *fpw;
    int   errCode = 1;
    int   i;
    char  *printWordOut, *rosterElem;
    char  rosterWork[PIWEBGEN_MAXROSTER];
    char  hyperLinkCode[256];
    char  timeoutStatus[256];

    if (NULL != (fpw = fopen( piwebgenConfig.webFileName, "wt" ))) {

        if ( apiTimeGet() < (piwebgenConfig.commTimeoutSec + apiGetLastRosterTime()) )  
            strlcpy( timeoutStatus, "[OK]", 256 );
        else
            strlcpy( timeoutStatus, "<font color=\"red\">[SERVER DOWN]</font> ", 256 );

	if ( 0 != piwebgenConfig.autoRefreshHeader ) {
            fprintf( fpw, "<meta http-equiv=\"refresh\" content=\"14\" /> \n" );
        }

        fprintf( fpw, "<b>%s</b>\n", apiGetServerName() );
        if ( 0 != strlen( piwebgenConfig.line2 ) )
            fprintf( fpw, "<br>%s\n", piwebgenConfig.line2 );
        fprintf( fpw, "<br>Map: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp %s\n", apiGetMapName() );
        fprintf( fpw, "<br>Players: &nbsp; %d\n", apiPlayersGetCount());
        fprintf( fpw, "<br>Time: &nbsp;&nbsp;&nbsp;&nbsp;   %s\n", apiTimeGetHuman() );
        fprintf( fpw, "<br>Status: &nbsp;&nbsp;   %s\n", timeoutStatus );
        if ( 0 != strlen( piwebgenConfig.directConnect ) )
            fprintf( fpw, "<br>Connect: %s\n", piwebgenConfig.directConnect );
        if ( 0 != strlen( piwebgenConfig.webminURL ) )  {
            // <a href="url">link text</a>
            // old - snprintf( hyperLinkCode, 256, "<a href=\"http://%s\" target=\"_blank\">link</a>", piwebgenConfig.webminURL );
            snprintf( hyperLinkCode, 256, "<a href=\"%s\" target=\"_blank\">link</a>", piwebgenConfig.webminURL );
            fprintf( fpw, "<br>Admin: &nbsp;&nbsp; %s\n", hyperLinkCode );
        }
        fprintf( fpw, "<br>Names: &nbsp;&nbsp; ");

	if ( 0 == piwebgenConfig.hyperlinkFormat ) {
            fprintf( fpw, "<font color=\"blue\">%s</font> ", apiPlayersRoster( 0, " : " ) );
	}
	else {
	    strlcpy( rosterWork, apiPlayersRoster( 4, "\011" ), PIWEBGEN_MAXROSTER);  // get roster with tab for delimiter
	    if (  0 != strlen( rosterWork )) {                   // if not an empty list then
 	        for ( i=0 ;; i++ ) {

		    // walk through each player
		    //
		    rosterElem = getWord( rosterWork, i, "\011" );  
		    if (NULL == rosterElem)        break;    // end of list?
		    if (strlen( rosterElem) < 35 ) break;    // element is invalid if <35 chars

		    // convert each player record to hyperlink html format
		    //
                    printWordOut = _convertNameToHyperlink( rosterElem );

		    // write out this player info  to the html file
		    //
                    fprintf( fpw, "%s ", printWordOut );
                }
	    }
	}
        fprintf( fpw, "\n<br><br>\n");
        fclose( fpw );
        errCode = 0;
    }
    else {
        logPrintf( LOG_LEVEL_CRITICAL,
            "piwebstat", "Unable to create the html output file ::%s::",
            piwebgenConfig.webFileName );
    }
    return errCode;
}

//  ==============================================================================================
//  piwebgenClientAddCB
//
//  Update the web when a new client connects (if so configured)
//  
//
int piwebgenClientAddCB( char *strIn )
{
    if ( piwebgenConfig.updateOnChange ) _genWebFile();
    return 0;
}

//  ==============================================================================================
//  piwebenClientDelCB
//
//  Update the web when a client disconnects (if so configured)
//
//
int piwebgenClientDelCB( char *strIn )
{
    if ( piwebgenConfig.updateOnChange ) _genWebFile();
    return 0;
}

//  ==============================================================================================
//  piwebgenInitCB
//
//  Update the web at server start
//  
//
int piwebgenInitCB( char *strIn )
{
    if ( piwebgenConfig.updateOnChange ) _genWebFile();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piwebgenRestartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piwebgenMapChangeCB
//
//  Update the web on map change (if so configured)
// 
//
int piwebgenMapChangeCB( char *strIn )
{
    if ( piwebgenConfig.updateOnChange ) _genWebFile();
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piwebgenGameStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piwebgenGameEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piwebgenRoundStartCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piwebgenRoundEndCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  ...
//
//  ...
//  ...
//
int piwebgenCapturedCB( char *strIn )
{
    return 0;
}

//  ==============================================================================================
//  piwebgenPeriodicCB
//
//  This option updates the web page by time.  The update rate can be modified from the
//  configuration file.
//  
//
int piwebgenPeriodicCB( char *strIn )
{
    static int intervalCount = 0;

    if ( 0 != piwebgenConfig.updateIntervalSec ) {

        if ( (++intervalCount) > piwebgenConfig.updateIntervalSec ) {
            _genWebFile();
           intervalCount = 0;
        }
    }
    return 0;
}

//  ==============================================================================================
//  piwebgenSigtermCB
//
//  SISSM shutdown - leave a 'down' webfile so that the server is not mistaken as
//  up and running.
//
int piwebgenSigtermCB( char *strIn )
{
    FILE *fpw;

    if (NULL != (fpw = fopen( piwebgenConfig.webFileName, "wt" ))) {

	if ( 0 != piwebgenConfig.autoRefreshHeader ) 
            fprintf( fpw, "<meta http-equiv=\"refresh\" content=\"14\" /> \n" );

        fprintf( fpw, "<br><br>SISSM shut down at: &nbsp; %s\n", apiTimeGetHuman() );
        fclose( fpw );
    }

    return 0;
}


//  ==============================================================================================
//  piwebgenInstallPlugin
//
//  
//  This method is exported and is called from the main sissm module.
//
int piwebgenInstallPlugin( void )
{
    // Read the plugin-specific variables from the .cfg file 
    // 
    piwebgenInitConfig();

    // if plugin is disabled in the .cfg file then do not activate
    //
    if ( piwebgenConfig.pluginState == 0 ) return 0;

    // Install Event-driven CallBack hooks so the plugin gets
    // notified for various happenings.  A complete list is here,
    // but comment out what is not needed for your plug-in.
    // 
    eventsRegister( SISSM_EV_CLIENT_ADD,           piwebgenClientAddCB );
    eventsRegister( SISSM_EV_CLIENT_DEL,           piwebgenClientDelCB );
    eventsRegister( SISSM_EV_INIT,                 piwebgenInitCB );
    eventsRegister( SISSM_EV_RESTART,              piwebgenRestartCB );
    eventsRegister( SISSM_EV_MAPCHANGE,            piwebgenMapChangeCB );
    eventsRegister( SISSM_EV_GAME_START,           piwebgenGameStartCB );
    eventsRegister( SISSM_EV_GAME_END,             piwebgenGameEndCB );
    eventsRegister( SISSM_EV_ROUND_START,          piwebgenRoundStartCB );
    eventsRegister( SISSM_EV_ROUND_END,            piwebgenRoundEndCB );
    eventsRegister( SISSM_EV_OBJECTIVE_CAPTURED,   piwebgenCapturedCB );
    eventsRegister( SISSM_EV_PERIODIC,             piwebgenPeriodicCB );
    eventsRegister( SISSM_EV_SIGTERM,              piwebgenSigtermCB );

    return 0;
}


