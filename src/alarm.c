//  ==============================================================================================
//
//  Module: ALARM
//
//  Description:
//  Alarm event handling with callback feature
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under the MIT License
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

#include "alarm.h"

//  ==============================================================================================
//  Data definition 
//

//  Master array of active alarm Objects.  The alarm object is inserted here by the
//  alarmCreate() routine, and associated callbacks are made by walking through this
//  table from the dispatcher.
//
static alarmPtr alarmTable[ SISSM_MAXALARMS ];

//  ==============================================================================================
//  alarmInit
//
//  Module initialization to be called once only.
//  
//
void alarmInit( void )
{
    int i;
    for (i=0; i<SISSM_MAXALARMS; i++) {
        alarmTable[ i ] = NULL;
    }
    return;
}


//  ==============================================================================================
//  alarmCreate
//
//  Create an alarm object, optionally install a callback.  Callback may be NULL if not used.
//  
//
alarmObj *alarmCreate( int (*alarmCBfunc)( char * ) )
{
    alarmObj *aPtr = NULL;
    int i;

    aPtr = calloc( 1, sizeof( alarmObj ));
    if ( aPtr != NULL ) {
        aPtr->alarmTime = 0L;
        aPtr->alarmCallbackFunction = alarmCBfunc;

        for (i = 0; i<SISSM_MAXALARMS; i++) {
            if ( alarmTable[ i ] == NULL ) {
                alarmTable[ i ] = aPtr;
                aPtr->usedIndex = i;
                break;
            }
        }
    }
    return aPtr;
}


//  ==============================================================================================
//  alarmDestroy
//
//  Destroy an alarm object.
//
//
int alarmDestroy( alarmObj *aPtr )
{
    int errCode = 1;
    if ( NULL != aPtr ) {
        if ( aPtr->usedIndex != -1 ) {
            alarmTable[ aPtr->usedIndex ] = NULL;
            free( aPtr );
            errCode = 0;
        }
    }
    return errCode;
}


//  ==============================================================================================
//  alarmReset
//
//  Set or reset alarm for future time.  For example, if you wish to set alarm 5 seconds from 
//  present, then set timeSec 5.  If alarmReset() is called on an actively counting alarm,
//  the new value timeSec supercedes the previous count.
//
int alarmReset( alarmObj *aPtr, unsigned long timeSec)
{
    aPtr->alarmTime = timeSec + time(NULL); 
    return 0; 
}

//  ==============================================================================================
//  alarmCancel
//
//  Cancels a current alarm count.  Since the alarm instance is not destroyed it can be 
//  reactivated by alarmReset() call.
//  
//
int alarmCancel( alarmObj *aPtr )
{
    aPtr->alarmTime = 0L;
    return 0; 
}


//  ==============================================================================================
//  alarmStatus
//
//  Take a peek into currently active alarm, and returns time left before alarm is activated.
//  If the alarm is cannceled, expired, not started, or not set, then value zero is returned.
//  
//
long int alarmStatus( alarmObj *aPtr )
{
    unsigned long retValue = 0L;
    unsigned long timeNow;

    timeNow = time(NULL);
    if ( aPtr->alarmTime > timeNow ) retValue = aPtr->alarmTime - timeNow;
    return retValue; 
}


//  ==============================================================================================
//  alarmDispatch
//
//  This polling routine is called from the master application loop, or from a
//  periodic processing handler.   It loops through all active alarm objects, and 
//  fires off a callback routines of the expired objects.
//  ...
//
void alarmDispatch( void )
{
    alarmObj *aPtr;
    unsigned long timeNow;
    int i;
 
    timeNow = time(NULL);

    for (i = 0; i<SISSM_MAXALARMS; i++) {
        aPtr = alarmTable[ i ];
        if ( aPtr != NULL ) {
            if ( aPtr->alarmTime != 0L ) {
                if ( aPtr->alarmTime <= timeNow ) {
                    aPtr->alarmTime = 0L;
                    if ( aPtr->alarmCallbackFunction != NULL ) {
                        (aPtr->alarmCallbackFunction)("alarm");
                    }
                }
            }
        }
    }
    return;
}


