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
#include <ctype.h>
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

#include "roster.h"
#include "api.h"
#include "sissm.h"

#include "piwebgen.h"


//  ==============================================================================================
//  Data definition 
//
static struct {

    int  pluginState;                      // always have this in .cfg file:  0=disabled 1=enabled

    int utf8Encoding;                      // if true, generate utf-8 encoding

    char webFileName[CFS_FETCH_MAX];       // name of HTML output
    unsigned int  updateIntervalSec;       // forced periodic update rate (even if same data)
    int  updateOnChange;                   // update only when changed
    char webminURL[128];                   // webmin instruction e.g., www.mysite.com/webmin
    char directConnect[CFS_FETCH_MAX];     // direct connect instructions e.g., 123.123.123.123:12345
    char line2[CFS_FETCH_MAX];             // 2nd line title (comes after the server name
    int  hyperlinkFormat;                  // 0=simple output, 1=player names as hyperlinks
    int  autoRefreshHeader;                // 1=create html page autorefresh header, 0=none
    int  commTimeoutSec;                   // # sec of RCON failure to indicate "fail" on web
    int  showSessionID;                    // 1=show replay SessionID, 0=no
    int  reReadHostName;                   // 1=re-read hostname every iteration, 0=one time only

    int  showUpTime;                       // 1=show server "up time" and last rebooted reason
    int  showLastReboot;                   // 1=show last reboot time and last rebooted reason 
    int  showCurrentTime;                  // 1=show current time
    int  showGameMode;                     // 1=show current game mode

} piwebgenConfig;

static char gameMode[ API_ROSTER_STRING_MAX ];

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

    piwebgenConfig.utf8Encoding = (int) cfsFetchNum( cP, "piwebgen.utf8Encoding", 1.0 );  // enabled by default

    // read "piwebgen.StringParameterExample" variable from the .cfg file
    strlcpy( piwebgenConfig.webFileName,  cfsFetchStr( cP, "piwebgen.webFileName",  "sissm.html" ), CFS_FETCH_MAX );
    piwebgenConfig.updateIntervalSec = (int) cfsFetchNum( cP, "piwebgen.updateIntervalSec", 60 );
    piwebgenConfig.updateOnChange    = (int) cfsFetchNum( cP, "piwebgen.updateOnChange", 1 );
    piwebgenConfig.autoRefreshHeader = (int) cfsFetchNum( cP, "piwebgen.autoRefreshHeader", 0 );
    strlcpy( piwebgenConfig.webminURL, cfsFetchStr( cP, "piwebgen.webminURL", "" ), 128 );
    strlcpy( piwebgenConfig.directConnect, cfsFetchStr( cP, "piwebgen.directConnect", ""), CFS_FETCH_MAX );
    strlcpy( piwebgenConfig.line2, cfsFetchStr( cP, "piwebgen.line2", ""), CFS_FETCH_MAX );
    piwebgenConfig.hyperlinkFormat   = (int) cfsFetchNum( cP, "piwebgen.hyperlinkFormat", 1 );
    piwebgenConfig.commTimeoutSec    = (int) cfsFetchNum( cP, "piwebgen.commTimeoutSec", 120 );
    piwebgenConfig.showSessionID     = (int) cfsFetchNum( cP, "piwebgen.showSessionID", 0 );
    piwebgenConfig.showUpTime        = (int) cfsFetchNum( cP, "piwebgen.showUpTime", 1.0 );
    piwebgenConfig.showLastReboot    = (int) cfsFetchNum( cP, "piwebgen.showLastReboot", 0 );
    piwebgenConfig.showCurrentTime   = (int) cfsFetchNum( cP, "piwebgen.showCurrentTime", 0 );
    piwebgenConfig.showGameMode      = (int) cfsFetchNum( cP, "piwebgen.showGameMode", 0 );
    piwebgenConfig.reReadHostName    = (int) cfsFetchNum( cP, "piwebgen.reReadHostName", 0 );


    cfsDestroy( cP );

    strlcpy( gameMode, "Unknown", API_ROSTER_STRING_MAX );

    return 0;
}


//  ==============================================================================================
//  _convertNameToHyperlink
//
//  This routine converrts: 
//      "76560000000000001 111.001.034.025 Random Person "
//  To: 
//      <a href="http://steamcommunity.com/profiles/76560000000000001/" target="_blank">Random Person</a>
//  And this:
//      "12312312312312312312312312312312|12312312312312312312312312312312 111.001.034.025 Random Person"
//  To:
//      <a href="javascript:alert('12312312312312312312312312312312|12312312312312312312312312312312');" style="color: #8ebf42">Random Person</a> 
//
#define STEAM_PROFILE "<a href=\"http://steamcommunity.com/profiles/76560000000000001/\" target=\"_blank\">"
#define EPIC_PROFILE  "<a href=\"javascript:alert(\'01234567890123456789012345678901|01234567890123456789012345678901\');\" style=\"color: #8ebf42\">"

char *_convertNameToHyperlink( char *identString )
{
    static char retStr[256];
    char testGUID[256], *q;
    int formatOfID = 0;

    // Get the first word of identString and check if Steam or EPIC
    //
    if ( NULL != (q = getWord( identString, 0, " " ))) {
        strlcpy( testGUID, q, 256 );
        formatOfID = rosterIsValidGUID( testGUID );
    }

    switch( formatOfID ) {
    case 1: // Steam
        strlcpy( retStr, STEAM_PROFILE, 256 );                                    // copy the template
        strncpy( &retStr[44], identString, 17 );                        // insert/substitute SteamID64
        strlcat( retStr, &identString[34], 256 );                           // append name (text part)
        strlcat( retStr, "</a>", 256 );                                             // close with </a>
        break;
    case 2: // EPIC
        strlcpy( retStr, EPIC_PROFILE, 256 );                                    // copy the template
        strncpy( &retStr[27], identString, 65 );                         // insert/substitute EPIC ID
        strlcat( retStr, &identString[82], 256 );                           // append name (text part)
        strlcat( retStr, "</a>", 256 );                                             // close with </a>
        break;
    default:
        strlcpy( retStr, "*InternalError*", 256 );
        break;
    }

    return( retStr );
}


#define PIWEBGEN_MAXROSTER   (16*1024)


//  ==============================================================================================
//  _htmlSafeFilter
//
//  Replaces < and > from input string, with [ and ], respectively
//
char *_htmlSafeFilter( char *strIn )
{
    int i;
    static char strOut[ PIWEBGEN_MAXROSTER ];

    strlcpy( strOut, strIn, PIWEBGEN_MAXROSTER );

    // replace metatag or escape-sequence characters
    // with something benign (antihack)
    //
    for (i=0; i< (int) strlen( strOut ); i++) {
        switch (strOut[i]) {
            case '<':   strOut[i] = '[';  break;
            case '>':   strOut[i] = ']';  break;
            case '/':   case '\\':  strOut[i] = '|';  break;
        }
    }

    // If utf-8 encoding is NOT declared, then assume ascii.
    // replace any non-printable encodings wtih asterisks.
    // 
    if ( 0 == piwebgenConfig.utf8Encoding ) {
        for (i=0; i< (int) strlen( strOut ); i++) {
            if ( strOut[i] != '\011' )  {
                if (!isprint( (int) strOut[i] )) strOut[i] = '*';
            }
        }
    }

    return( strOut );
}

//  ==============================================================================================
//  _genWebFile
//
//  Generates a HTML status file using current server state
//  
//

static int _genWebFile( void )
{
    FILE *fpw;
    char *w;
    int   errCode = 1;
    int   i;
    char  *printWordOut, *rosterElem, rosterElem2[1024];
    char  rosterWork[PIWEBGEN_MAXROSTER];
    char  hyperLinkCode[256];
    char  timeoutStatus[256];
    char  *srvName;

    if (NULL != (fpw = fopen( piwebgenConfig.webFileName, "wt" ))) {

        if ( apiTimeGet() < (piwebgenConfig.commTimeoutSec + apiGetLastRosterTime()) )   {
            strlcpy( timeoutStatus, "[OK]", 256 );
            w = rosterGetObjective();
            if ( 0 != strlen( w ) ) strlcpy( timeoutStatus, w, 256 );
        }
        else
            strlcpy( timeoutStatus, "<font color=\"red\">[SERVER DOWN]</font> ", 256 );

        // Option for creating auto-refresh header for html.  Turn this off if you are refreshing
        // from higher level (e.g., text file inclusion or html frames).
        //
	if ( 0 != piwebgenConfig.autoRefreshHeader ) {
            fprintf( fpw, "<meta http-equiv=\"refresh\" content=\"14\" /> \n" );
        }

        // Option for UTF-8 encoding extended character sets
        //
	if ( 0 != piwebgenConfig.utf8Encoding ) {
            fprintf( fpw, "<meta charset=\"utf-8\"> \n" );
        }

        // Displaying the server name - from sissm.cfg file, or RCON
        //
        srvName = apiGetServerName();
        if ( 0 == strncmp( srvName, "*", 2 ) ) 
            srvName = apiGetServerNameRCON( 0 );
        fprintf( fpw, "<b>%s</b>\n", srvName );

        // Option for displaying extra line description
        //
        if ( 0 != strlen( piwebgenConfig.line2 ) )
            fprintf( fpw, "<br>%s\n", piwebgenConfig.line2 );

        // Option for displaying current sessino ID for playbacks
        //
        if ( piwebgenConfig.showSessionID )
            fprintf( fpw, "<br>SID: %s\n", apiGetSessionID());
        fprintf( fpw, "<br>Map: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp %s\n", apiGetMapName() );

        // Display number of players and stauts of the server
        //
        fprintf( fpw, "<br>Players: &nbsp; %d  &nbsp;&nbsp;&nbsp; Status: &nbsp;&nbsp;  %s\n", 
            apiPlayersGetCount(), timeoutStatus);

        // Option for displaying "current time" 
        //
        if ( piwebgenConfig.showCurrentTime ) {
            fprintf( fpw, "<br>Time: &nbsp;&nbsp;&nbsp;&nbsp;   %s [Now]\n", apiTimeGetHuman(0L) );
        }
 
        // Option for displaying "last reboot event & last reboot time"
        //
        if ( piwebgenConfig.showLastReboot ) {
            fprintf( fpw, "<br>Last: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   %s [%s]\n", 
                apiTimeGetHuman( apiGetLastRebootTime()), apiGetLastRebootReason() );
        }

        // Option for displaying "last reboot event & elapsed time since reboot"
        //
        if ( piwebgenConfig.showUpTime ) {
            fprintf( fpw, "<br>Uptime: &nbsp; %s since [%s]\n", 
                computeElapsedTime( apiGetLastRebootTime(), apiTimeGet()), apiGetLastRebootReason() );
        }

        // Option for displaying direct IP connection method from game client
        //
        if ( 0 != strlen( piwebgenConfig.directConnect ) )
            fprintf( fpw, "<br>Connect: %s\n", piwebgenConfig.directConnect );

        // Option for displaying hyperlink to web-based admin page
        //
        if ( 0 != strlen( piwebgenConfig.webminURL ) )  {
            snprintf( hyperLinkCode, 256, "<a href=\"%s\" target=\"_blank\">link</a>", 
                piwebgenConfig.webminURL );
            fprintf( fpw, "<br>Admin: &nbsp;&nbsp; %s\n", hyperLinkCode );
        }

        if ( 0 != piwebgenConfig.showGameMode ) {
            fprintf( fpw, "<br>Game Mode: &nbsp;&nbsp;&nbsp;&nbsp;   %s\n", gameMode );
        }

        if ( 1L == p2pGetL( "pigateway.p2p.lockOut", 0L )) {
            fprintf( fpw, "<br>* Temporarily LOCKED for Private Session *\n" );
        }
        else if ( 2L == p2pGetL( "pigateway.p2p.lockOut", 0L )) {
            fprintf( fpw, "<br>*** Temporarily LOCKED for Private Session ***\n" );
        }

        // Display list of players
        //
        fprintf( fpw, "<br>Names: &nbsp;&nbsp; ");
	if ( 0 == piwebgenConfig.hyperlinkFormat ) {
            fprintf( fpw, "<font color=\"blue\">%s</font> ", apiPlayersRoster( 0, " : " ) );
	}
	else {
            // get roster with tab for delimiter
	    strlcpy( rosterWork,  _htmlSafeFilter( apiPlayersRoster( 4, "\011" )), PIWEBGEN_MAXROSTER);  
	    if (  0 != strlen( rosterWork )) {                   // if not an empty list then
 	        for ( i=0 ;; i++ ) {

		    // walk through each player
		    //
                    strclr( rosterElem2 );
		    rosterElem = getWord( rosterWork, i, "\011" );  
		    if (NULL == rosterElem)        break;    // end of list?
		    if (strlen( rosterElem) < 35 ) break;    // element is invalid if <35 chars

                    strlcpy( rosterElem2, rosterElem, 1024 );

		    // convert each player record to hyperlink html format
		    //
                    printWordOut = _convertNameToHyperlink( rosterElem2 );

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
    // re-read & cache server name
    //
    apiGetServerNameRCON( piwebgenConfig.reReadHostName );

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
    static unsigned int intervalCount = 0;
    static unsigned int modePollingGameMode   = 0;
    static unsigned int modePollingHostName   = 15;

    if ( piwebgenConfig.showGameMode ) {
        if ( modePollingGameMode++ > 60 ) {
            modePollingGameMode = 0;

            // Get current game mode: "checkpoint", "hardcorecheckpoint"....
            //
            strlcpy( gameMode, apiGameModePropertyGet( "gamemodetagname" ), API_ROSTER_STRING_MAX );
            if ( 0 != strlen( gameMode ) )
                logPrintf( LOG_LEVEL_INFO, "piwebgen", "GameMode is ::%s::", gameMode );
            else {
                strlcpy( gameMode, "Unknown", API_ROSTER_STRING_MAX );
                logPrintf( LOG_LEVEL_WARN, "piwebgen", "** Unable to read; current GameMode" );
            }
        }
    }

    if ( piwebgenConfig.reReadHostName ) {
        if ( modePollingHostName++ > 60 ) {
            modePollingHostName = 0;
            apiGetServerNameRCON( piwebgenConfig.reReadHostName );
        }
    }

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

        fprintf( fpw, "<br><br>SISSM shut down at: &nbsp; %s\n", apiTimeGetHuman(0L) );
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



