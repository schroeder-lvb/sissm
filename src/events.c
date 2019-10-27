//  ==============================================================================================
//
//  Module: EVENTS
//
//  Description:
//  Event-driven engine with callback features: init, register, dispatch
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
#include <time.h>
// #include <stdarg.h>
// #include <sys/stat.h>

// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netdb.h>
// #include <fcntl.h>

#include "events.h"


//  ==============================================================================================
//  Data definition 
//
//


//  Two-dimensional table of event callback functions and associated trigger strings.
//  They are populated as eventRegister() is called.
//
static int (*eventsCallbackFunctions[SISSM_MAXPLUGINS][SISSM_MAXEVENTS])( char * );


//  Master events table, to associate:
//  *  Event ID number - for API calls
//  *  Even ID to callback array index map eventsCallbackFunctions[plugin#][*]
//  *  String (game log file) that triggers the event 
//
struct {
    int        eventID;
    int        callBacktableIndex;
    const char *eventString;
} eventTable[SISSM_MAXEVENTS] = {

    { SISSM_EV_INIT,                 0, "~INIT~"              }, 
    { SISSM_EV_RESTART,              1, "~RESTART~"           },
    { SISSM_EV_CLIENT_ADD,           2, SS_SUBSTR_REGCLIENT   },
    { SISSM_EV_CLIENT_DEL,           3, SS_SUBSTR_UNREGCLIENT },
    { SISSM_EV_MAPCHANGE,            4, SS_SUBSTR_MAPCHANGE   },
    { SISSM_EV_GAME_START,           5, SS_SUBSTR_GAME_START  },
    { SISSM_EV_GAME_END,             6, SS_SUBSTR_GAME_END    },
    { SISSM_EV_ROUND_START,          7, SS_SUBSTR_ROUND_START },
    { SISSM_EV_ROUND_END,            8, SS_SUBSTR_ROUND_END   },
    { SISSM_EV_OBJECTIVE_CAPTURED,   9, SS_SUBSTR_CAPTURE     },
    { SISSM_EV_PERIODIC,            10, "~PERIODIC~"          },
    { SISSM_EV_CLIENT_ADD_SYNTH,    11, "~SYNTHADD~"          },
    { SISSM_EV_CLIENT_DEL_SYNTH,    12, "~SYNTHDEL~"          },
    { SISSM_EV_SHUTDOWN,            13, SS_SUBSTR_SHUTDOWN    },
    { SISSM_EV_CHAT,                14, SS_SUBSTR_CHAT        },
    { SISSM_EV_SIGTERM,             15, "~SIGTERM~"           },
    { SISSM_EV_WINLOSE,             16, SS_SUBSTR_WINLOSE     },
    { SISSM_EV_TRAVEL,              17, SS_SUBSTR_TRAVEL      },
    { SISSM_EV_SESSIONLOG,          18, SS_SUBSTR_SESSIONLOG  },
    { SISSM_EV_OBJECT_SYNTH,        19, "~SYNTHOBJ~"          },
    { -1,                           -1, "*"                   },

};


//  ==============================================================================================
//  eventsInit
//
//  One-time initialization of this module
//
int eventsInit( void )
{
    int errCode = 0;
    int i, j;

    // zero out the callback functions
    //
    for ( i=0; i<SISSM_MAXPLUGINS; i++ )
        for ( j=0; j<SISSM_MAXEVENTS; j++ )
            eventsCallbackFunctions[i][j] = NULL;

    return errCode;
}


//  ==============================================================================================
//  eventsRegister
//
//  Called from a Plugin, this method associates and registers the plugin-specific callback routine 
//  with the specific event.
//
int eventsRegister( int eventID, int (*callBack)( char * ))
{
    int i, j, callBackIndex = -1, errCode = 1;

    // Translate the eventID to callBackTable index
    // 
    for ( i=0; i<SISSM_MAXEVENTS; i++ ) {
        if ( 0 == strcmp( eventTable[ i ].eventString, "*" ))  {
            break;
        }
        if ( eventTable[ i ].eventID == eventID ) {
            callBackIndex = eventTable[ i ].callBacktableIndex;
            break;          
        }
    }

    // Insert callback function pointer to the first non-null slot
    // in the 'eventsCallbackFunctions' table
    //
    if ( callBackIndex != -1 ) {
        for ( j=0; j<SISSM_MAXPLUGINS ; j++ ) {
            if ( eventsCallbackFunctions[callBackIndex][j] == NULL) {
                eventsCallbackFunctions[callBackIndex][j] = callBack;
                errCode = 0;
                break;
            }
        }
    }
    return errCode;
}



//  ==============================================================================================
//  eventsDispatch
//
//  This routine is called from 1) a game log file poller (tail) to trigger a specific event
//  when the logged event matches the string patter, or 2) when a self-generated (synthetic)
//  event is generated to impact other events.
//
int eventsDispatch( char *strBuffer )
{
    int i, j;
    int activeCallBackIndex = -1;

    for (i=0; i<SISSM_MAXEVENTS; i++) { 
     
        if ( 0 == strlen( eventTable[i].eventString )) 
            continue; 
        if ( 0 == strcmp( eventTable[i].eventString, "*" )) 
            break;

        if ( NULL != strstr( strBuffer, eventTable[i].eventString ) ) {
            activeCallBackIndex = eventTable[i].callBacktableIndex;
            if ( activeCallBackIndex >= 0 ) {
                for (j=0; j<SISSM_MAXPLUGINS; j++) {
                    if ( NULL != eventsCallbackFunctions[activeCallBackIndex][j] ) {
                         (*eventsCallbackFunctions[activeCallBackIndex][j])( strBuffer );
                    }
                }
            }
            break;
        }
        if (0==strcmp( "*", eventTable[i].eventString )) break;
    }
    return activeCallBackIndex;
}


