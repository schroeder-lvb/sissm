//  ==============================================================================================
//
//  Module: P2P
//
//  Description:
//  Plugin-to-Plugin communication (data and signaling)
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.10.02
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsd.h"
#include "log.h"
#include "util.h"

#define P2P_MAXSTR          (128)
#define P2P_MAXELEM         (256)

static char p2pVarNames[P2P_MAXELEM][P2P_MAXSTR];
static char p2pVarValues[P2P_MAXELEM][P2P_MAXSTR];

//  ==============================================================================================
//  (internal) _p2pSearch
//
//  Searches the p2pVarNames[*] for a match, returns the found index.  If not found,
//  -1 if found.  If "autoCreate" is set AND varname is not found, then it creates
//  a new entry and returns the index of the created table entry.  The autoCreate flag
//  is typically used for variable-set operation.
//  
//
static int _p2pSearch( char *varName, int autoCreate )
{
    int foundIndex = -1;
    int i;
    for (i=0; i<P2P_MAXELEM; i++) {
        if ( 0 == strcmp( varName, p2pVarNames[i] ) ) {
            foundIndex = i;
            break;
        }
    }

    if (( -1 == foundIndex ) && autoCreate ) {
        for (i=0; i<P2P_MAXELEM; i++) {
            if ( 0 == strlen( p2pVarNames[i] ) ) {
                strlcpy( p2pVarNames[i], varName, P2P_MAXSTR );
                strclr( p2pVarValues[i] );
                foundIndex = i;
                break;
            }
        }
        if ( -1 == foundIndex ) {
            logPrintf( LOG_LEVEL_CRITICAL, "p2p", "ERROR: Out of space");
        }
    }
    return( foundIndex );
}


//  ==============================================================================================
//  p2pSetF
//
//  Sets floating variables into the p2p communications namespace.
//
void p2pSetF( char *varName, double value )
{
    int i;

    if ( -1 != (i = _p2pSearch( varName, 1 ))) {
        strlcpy( p2pVarNames[i], varName, P2P_MAXSTR );
        snprintf( p2pVarValues[i], P2P_MAXSTR, "%lf", value );
    }
    return;
}

//  ==============================================================================================
//  p2pSetL
//
//  Sets unsigned long variables into the p2p communications namespace.
//
void p2pSetL( char *varName, unsigned long value )
{
    int i;

    if ( -1 != (i = _p2pSearch( varName, 1 ))) {
        strlcpy( p2pVarNames[i], varName, P2P_MAXSTR );
        snprintf( p2pVarValues[i], P2P_MAXSTR, "%lu", value );
    }
    return;
}

//  ==============================================================================================
//  p2pSetS
//
//  Sets string variables into the p2p communications namespace.
//
void p2pSetS( char *varName, char *value )
{
    int i;

    if ( -1 != (i = _p2pSearch( varName, 1 ))) {
        strlcpy( p2pVarNames[i], varName, P2P_MAXSTR );
        strlcpy( p2pVarValues[i], value, P2P_MAXSTR );
    }
    return;
}

//  ==============================================================================================
//  p2pGetF
//
//  Reads floating variables from the p2p communications namespace.  If the variable
//  is not found (not already set), the defaultValue is returned.
//
double p2pGetF( char *varName, double defaultValue )
{
    int i;
    double retValue = defaultValue;

    if ( -1 != (i = _p2pSearch( varName, 0 )) ) {
        sscanf( p2pVarValues[i], "%lf", &retValue ); 
    }
    return( retValue );
}

//  ==============================================================================================
//  p2pGetL
//
//  Reads unsigned long variables from the p2p communications namespace.  If the variable
//  is not found (not already set), the defaultValue is returned.
//
unsigned long p2pGetL( char *varName, unsigned long defaultValue )
{
    int i;
    unsigned long retValue = defaultValue;

    if ( -1 != (i = _p2pSearch( varName, 0 )) ) {
        sscanf( p2pVarValues[i], "%lu", &retValue ); 
    }
    return( retValue );
}

//  ==============================================================================================
//  p2pGetS
//
//  Reads string variables from the p2p communications namespace.  If the variable
//  is not found (not already set), the defaultValue is returned.
//
char  *p2pGetS( char *varName, char *defaultValue )
{
    int i;
    static char retValue[ P2P_MAXSTR ];

    if ( -1 != (i = _p2pSearch( varName, 0 )) ) 
        strlcpy( retValue, p2pVarValues[i], P2P_MAXSTR );
    else 
        strlcpy( retValue, defaultValue, P2P_MAXSTR );
     return( retValue );
}

//  ==============================================================================================
//  p2pInit
//
//  Clears the p2p namespace.  This must be called at least once at the start of the app.
//
void p2pInit( void )
{
    int i;

    for (i=0; i<P2P_MAXELEM; i++) {
        strclr( p2pVarNames[i] );
        strclr( p2pVarValues[i] );
    }
    return;
}

//  ==============================================================================================
//  p2pDebugDump
//
//  (DEBUG) dumps the p2p namespace for troubleshooting
//
void p2pDebugDump( void )
{
    int i;

    for (i=0; i<P2P_MAXELEM; i++) {
        if (0 != strlen( p2pVarNames[i] ) ) 
            printf("%06d::%s::%s\n", i, p2pVarNames[i], p2pVarValues[i] );
    }
    return;
}
   

//  ==============================================================================================
//  debug main
//
//  (DEBUG) for testing only
//
int p2pTestMain()
{
    p2pInit();

    p2pSetS( "test123", "string1" );
    p2pSetS( "test123", "string2" );
    p2pSetS( "test123", "string3" );
    p2pSetS( "test123", "string123" );
    printf("\n::%s::\n", p2pGetS( "test123", "default" ));

    p2pSetF( "test3210", 3.141 );
    p2pSetF( "test3210", 3.142 );
    p2pSetF( "test3210", 3.143 );
    p2pSetF( "test3210", 3.14 );
    printf("\n::%lf::\n", p2pGetF( "test321", 2.17 ));

    p2pDebugDump();

    return 0;
}


