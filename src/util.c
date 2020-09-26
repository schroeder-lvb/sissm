//  ==============================================================================================
//
//  Module: UTIL
//
//  Description:
//  Generic tools subroutines
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>

#include "bsd.h"
#include "util.h"
#include "winport.h"

//  ==============================================================================================
//  elapsedTime
//
//  Returns a string of elapsed time between two time structure (unsigned long)
//  
char *computeElapsedTime( unsigned long timeMark, unsigned long timeNow )
{
    static char retElapsedString[256];
    unsigned long timeElapsed, timeHours, timeMinutes, timeSeconds;

    if ( timeMark >= timeNow ) {
        strclr( retElapsedString );
    }
    else {
        timeElapsed = timeNow - timeMark;
        timeHours   = timeElapsed / 3600L;
        timeMinutes = (timeElapsed - (timeHours * 3600L)) / 60L;
        timeSeconds = (timeElapsed - (timeHours * 3600L + timeMinutes * 60L)) % 60L;
        snprintf( retElapsedString, 256, "%ld:%02ld:%02ld", timeHours, timeMinutes, timeSeconds );
    }
    return( retElapsedString );
}


//  ==============================================================================================
//  getWord
//
//  Returns n-th word from a string using specified delimiter(s).  First word is wordIndex=0.
//  
//
#define BUFSIZE ( 16*1024 )

char *getWord( char *strIn, int wordIndex, char *delim )
{
    static char retStr[BUFSIZE];

    char  *pch = NULL;
    int   i = 0;

    retStr[BUFSIZE-1] = 0;
    strlcpy( retStr, strIn, BUFSIZE-2);
    pch = strtok( retStr, delim );
    while (pch != NULL) {
        if ( i++ == wordIndex ) {
            break;
        }
        pch = strtok( NULL, delim );
    }
    return pch;
}

//  ==============================================================================================
//  foundMatch
//
//  Returns true (!=0) if *line contains any of substrings
//  in table[].  If 'caseConvert' is true, then case match
//  is disabled.
//
int foundMatch( char *line, char *table[], int caseConvert )
{
    int foundMe = 0;
    int p;
    static char lineCopy[4096];

    strlcpy( lineCopy, line, 4096 );

    if ( caseConvert ) {
        for (p=0; p<4096; p++) {
           lineCopy[p] = tolower(line[p]);
           if (line[p] == 0) break;
        }
        lineCopy[4095] = 0;
    }

    p = 0;
    while (table[p][0] != '*') {
        if (NULL != strstr( lineCopy, table[p] )) {
            foundMe = 1;
            break;
        }
        p++;
    }
    return( foundMe );
}

//  ==============================================================================================
//  strTrimInPlace
//
//  Fast-trim utility to remove leading and training white-spaces in the same memory
//  space passed as the parameter
//
void strTrimInPlace(char * s)
{
    char * p = s;
    int l = strlen( p );

    while ( isspace(p[l - 1]) )  p[--l] = 0;
    while ( * p && isspace(*p) ) ++p, --l;

    memmove(s, p, l + 1);
    return;
}

//  ==============================================================================================
//  strClean
//
//  Remove LF and CR terminators in string, inline 
//
void strClean( char *strIn )
{
    int w = strlen( strIn );
    
    if ( w > 0 ) {
        if ( strIn[ w-1 ] == 0x0d ) strIn[ w-1 ] = 0;
        else if ( strIn[ w-1 ] == 0x0a ) strIn[ w-1 ] = 0;
    }
    return;
}

//  ==============================================================================================
//  strRemoveInPlace
//
//  In-line remove a substring, if found
//
void strRemoveInPlace(char * s, const char *toremove )
{
    while ( NULL != ( s = strcasestr( s, toremove ) ))
        memmove( s, s+strlen( toremove ), 1+strlen( s+strlen( toremove ) ));
    return;
}


//  ==============================================================================================
//  strclr
//
//  clear String, same as: strcpy( str, "" )
//
void strclr(char * s)
{
    s[0] = 0;
    return;
}


//  ==============================================================================================
//  strToLowerInPlace
//
//  Convert to lower string in place 
//
void strToLowerInPlace(char * s)
{
    register int i;
    int j;
    j = strlen( s );
    for (i=0; i<j; i++) s[i] = tolower(s[i]);
    return;
}


//  ==============================================================================================
//  reformatIP
//
//  Covert IP# string e.g., "111.22.3.4" to "111.022.003.004"
//  which makes for convenient conversion at string level.
//  On formatting error, or subnet exceeding 255, "000.000.000.000" is
//  returned with error code set.
//
char *reformatIP( char *originalIP )
{
    static char expandedIP[256];
    unsigned int n1, n2, n3, n4;
    strlcpy( expandedIP,  "000.000.000.000", 256 );
    if ( 4 == sscanf( originalIP, "%d.%d.%d.%d", &n1, &n2, &n3, &n4 )) {
        if ((n1<256) && (n2<256) && (n3<256) && (n4<256)) {
            snprintf( expandedIP, 256, "%03d.%03d.%03d.%03d", n1, n2, n3, n4 );
        }
    }
    return( expandedIP );
}

//  ==============================================================================================
//  isReadable
//
//  Checks if this file is readable by opening it, and reading one byte.
//  Returns 1 if readable, and 0 if not readable
//
int isReadable( char *fileName )
{
    int fileReadOk = 0;
    FILE *fpr;
    char tmpBuffer[16];

    if ( NULL != (fpr = fopen( fileName, "rb" ))) {
        if ( 1 == fread( tmpBuffer, 1, 1, fpr ) ) 
            fileReadOk = 1;
        fclose( fpr );
    }
    return fileReadOk;
}



//  ==============================================================================================
//  replaceDoubleColonWithBell
//  Replaces "::" with "bell" character for easy parsing.  This replacement happens
//  in-place, writing over the same string.
//
void  replaceDoubleColonWithBell( char *strInOut )
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


//  ==============================================================================================
//  debugPoke
//  For debug/development use only  - used to wedge forced parameter during runtime from
//  command line e.g., echo 12345 > filename.dbg
//  
int debugPoke( char *fileName, int *valueOut )
{
    int errCode = 1;
    FILE *fpr;
    static char tmpBuf[256];

    if ( NULL != (fpr = fopen( fileName, "rt" )) ) {
        if ( NULL != fgets( tmpBuf, 256, fpr ) ) {
            if ( 1 == sscanf( tmpBuf, "%d", valueOut )) {
                errCode = 0;
            }
        }
        fclose( fpr );
    }
    return( errCode ); 
}

