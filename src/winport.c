//  ==============================================================================================
//
//  Module: WINPORT
//
//  Description:
//  Windows equivalent of Linux functions
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under the MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#ifdef _WIN32

#include <windows.h>

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ==============================================================================================
//  usleep
//
//  Sleep for n microseconds
//
//  Reference:
//  https://stackoverflow.com/questions/5801813/c-usleep-is-obsolete-workarounds-for-windows-mingw
//
//
void usleep( __int64 usec ) 
{ 
    HANDLE timer; 
    LARGE_INTEGER timeExpired; 

    timeExpired.QuadPart = -(10*usec);

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &timeExpired, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
    return;
}

// ==============================================================================================
//  sleep
//
//  Sleep for n seconds
//
void sleep( int secWait )
{
    // _sleep( secWait * 1000 ); // deprecated
    Sleep( secWait * 1000 );
    return;
}


// ==============================================================================================
//  strcasestr
//
//  Strstr function without case match
//
#define STRSTRMAX (4*1024)

char *strcasestr(const char *haystack, const char *needle)
{
    char haystackCopy[ STRSTRMAX ], needleCopy[ STRSTRMAX ];
    char *p;
    unsigned int i;

    memset( haystackCopy, 0, STRSTRMAX );
    memset( needleCopy,   0, STRSTRMAX );
    strncpy( haystackCopy, haystack, STRSTRMAX-1 );
    strncpy( needleCopy,   needle,   STRSTRMAX-1 );

    for (i=0; i<strlen( haystackCopy); i++) haystackCopy[i] = (char) tolower( haystackCopy[i] );
    for (i=0; i<strlen( needleCopy); i++)   needleCopy[i]   = (char) tolower( needleCopy[i] );

    p = strstr( haystackCopy, needleCopy );
    if ( p != NULL ){
        p = (char *) ((char *) haystack + ( (char *) p - (char *) haystackCopy ));
    }

    return( p );
}

#endif




