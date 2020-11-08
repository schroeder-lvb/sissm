//  ==============================================================================================
//
//  Module: LOG
//
//  Description:
//  Generic file logger with adjustible severity level
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
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

#include "log.h"

//  ==============================================================================================
//  Data definition 
//

#define LOG_MAXSTRINGSIZE    (4096)
#define LOG_MAXFILENAMESTR   ( 255)

static char _logFileName[LOG_MAXFILENAMESTR];
static int  _logEchoToConsole = 0;
static int  _logLevel = 0;


//  ==============================================================================================
//  logPrintfInit
//
//  Initializes the internal data for the log system
//
void logPrintfInit( int logLevel, char *logFileName, int echoToConsole )
{
    strncpy( _logFileName, logFileName, sizeof( _logFileName) );
    _logEchoToConsole = echoToConsole;
    _logLevel = logLevel;
    return;
}


//  ==============================================================================================
//  logPrintf
//
//  write program trace data to log data where:
//      logLevel:  LOG_LEVEL_CRITICAL, WARN, INFO, DEBUG, or RAWDUMP
//       ident:     module name
//
void logPrintf( int logLevel, char *ident, const char * format, ... )
{
    time_t timer;
    char timeBuffer[26];
    struct tm* tm_info;
    static char buffer[LOG_MAXSTRINGSIZE];
    static FILE *fpw = NULL;
    int i, testEOL, bufLen;

    time( &timer );
    tm_info = localtime( &timer );
    strftime( timeBuffer, 26, "%Y-%m-%d %H:%M:%S", tm_info );

    if ( fpw == NULL ) 
        fpw = fopen( _logFileName, "at" );

    if ( fpw == NULL ) {
        printf( "Module log.c:  Logfile creation error, filename '%s'\n", _logFileName ); 
        exit( 1 );
    }

    va_list args;
    va_start( args, format );
    vsnprintf( buffer, LOG_MAXSTRINGSIZE, format, args );

    // If a there is a embedded \n or \r in the input string then 
    // replace it with <space> 
    // 
    if ( 0 != (bufLen = ( int ) strlen( buffer )) ) {
        for ( i = 0; i<bufLen; i++ ) {
            testEOL = buffer[i];
            if ( ( testEOL == 0x0d ) || ( testEOL == 0x0c ) ) {
                buffer[i] = 0x20;    
            }
        }
    }

    if ( fpw != NULL ) {
        if ( logLevel <= _logLevel ) { 
            fprintf( fpw, "%s::%s::%s\n", timeBuffer, ident, buffer );
            fflush( fpw );
            if ( _logEchoToConsole ) printf( "%s::%s::%s\n", timeBuffer, ident, buffer );
        }
    }
    va_end (args);
}

