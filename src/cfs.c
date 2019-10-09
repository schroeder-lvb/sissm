//  ==============================================================================================
//
//  Module: CFS
//
//  Description:
//  Simple configuration file reader
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================


#define _GNU_SOURCE                                                     // required for strcasestr

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "winport.h"    // strcasestr()
#include "util.h"
#include "bsd.h"
#include "cfs.h"


//  ==============================================================================================
//  Data definition 
//


//  ==============================================================================================
//  cfsCreate
//
//  Allocates and returns a .cfs (.cfg) file reader object and returns a pointer.  This method
//  opens the configuration file.  If the file does not exist, the object is not created and
//  NULL is returned to the caller. 
//
cfsPtr cfsCreate( char *fileName ) 
{
    cfsPtr pCfs = NULL;
    FILE    *fpr;

    if ( NULL != (fpr = fopen( fileName, "rt" )) ) {
        if ( NULL != (pCfs = (cfsPtr) calloc ( 1, sizeof( cfsObj )))) {
            strlcpy( pCfs->cfsFilename, fileName, CFS_FILENAME_MAX );
            pCfs->fpr = fpr;
        }
    }
    return( pCfs );
}


//  ==============================================================================================
//  cfsDestroy
//
//  Closes the .cfs file and deallocates the handler object.
//
void cfsDestroy( cfsPtr pCfs )
{
    if ( NULL != pCfs )  {
        if ( NULL != pCfs ) fclose( pCfs->fpr );
        free( pCfs );
    }
    return;
}


//  ==============================================================================================
//  _cfsStrWash (local)
//
//  Filters the input string line read from the .cfg file:
//  1.  Eliminate trailing line-feed
//  2.  Remove comments //
//  3.  Replace tabs with spaces
//  4.  Remove trailing/leading spaces
//  5.  Remove quotatino marks within which the string was packed
//
//
static char *_cfsStrWash( char *w )
{
    static char retStr[CFS_FETCH_MAX];
    unsigned int i, j, inQuote;

    strlcpy( retStr, w, CFS_FETCH_MAX );

    // eliminate any terminating line feed
    //
    if ( retStr[ strlen( retStr )-1 ] == 0x0a ) {
        retStr[ strlen( retStr )-1 ] = 0;
    }

    // eliminate any comment "//" not inside the quote
    //
    if ( strlen(retStr) >= 2 ) {  // the slash-slash is at least 2 chars
        inQuote = 0;             // 0 is outside quote, 1 is inside quote
        for ( i=0; i<strlen(retStr)-1; i++ ) {
            if ( retStr[i] == '\"' ) {
                inQuote = !inQuote;
            }
            else if ( (retStr[i] == '/') && (retStr[i+1] == '/' )) {
                if (!inQuote) {
                    retStr[i] = 0;
                    break;
                }
            }
        }
    }

    // replace TAB with spaces
    //
    for (i=0; i<strlen(retStr); i++)
        if (retStr[i] == 9) retStr[i] = 32;

    // clean up leading and trailing whitespaces
    //
    strTrimInPlace( retStr );

    // remove quote marks for string input, and
    // pack-slide the characters left
    //
    i = 0;
    while ( retStr[i] != 0 ) {
        if (retStr[i] == 34) {
            j = i;
            while (1) {
                if ( retStr[j] == 0 ) break;
                retStr[j] = retStr[j+1];
                j++;
            }
        }
        i++;
    }

    // process the exception "empty string" case
    if (0 != ( i = strlen( retStr )))
        if ( retStr[i-1] == 34 ) retStr[i-1] = 0;

    return (retStr);
}



//  ==============================================================================================
//  cfsFetchStr
//
//  Fetches a string variable from the .cfg file.  The defaultValue is returned if
//  the named variable is missing from the .cfg file.
//
char *cfsFetchStr( cfsPtr pCfs, char *varName, char *defaultValue )
{
    static char retStr[CFS_FETCH_MAX], lineIn[CFS_FETCH_MAX];
    char *w, *v, *u;

    strlcpy( retStr, defaultValue, CFS_FETCH_MAX );        

    if ( pCfs != NULL ) {
        fseek( pCfs->fpr, 0L, SEEK_SET );

        while ( !feof( pCfs->fpr ) ) {
            if (NULL != fgets( lineIn, CFS_FETCH_MAX, pCfs->fpr )) {
                w = strcasestr( lineIn, varName );
                if ( w == lineIn ) {
                    v = _cfsStrWash( lineIn );
                    u = strstr( v, " " );
                    if (u != NULL) {
                        strTrimInPlace( u );
                        strlcpy( retStr, u, CFS_FETCH_MAX );
                        break;
                    }
                }
            }
        }
        
    }
    return( retStr );
}


//  ==============================================================================================
//  cfsFetchNum
//
//  Fetches a numeric variable from the .cfg file.  The defaultValue is returned if
//  the named variable in the .cfg file is missing, or is not numeric.
//
double cfsFetchNum( cfsPtr pCfs, char *varName, double defaultValue )
{
    double retValue;
    char   defaultValueString[256];
    char   *w;

    snprintf( defaultValueString, 256, "%lf", defaultValue );
    w = cfsFetchStr( pCfs, varName, defaultValueString );

    if ( 1 != sscanf( w, "%lf", &retValue )) retValue = defaultValue;

    return( retValue );
}




