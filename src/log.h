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


#define LOG_LEVEL_CRITICAL   (0)   // super-quiet mode
#define LOG_LEVEL_WARN       (1)   // quiet mode
#define LOG_LEVEL_INFO       (2)   // ** default for production release **
#define LOG_LEVEL_DEBUG      (3)   // not for production release
#define LOG_LEVEL_RAWDUMP    (4)   // too much information

#define LOG_LEVEL_ALL        (3)   // same as DEBUG

extern void logPrintfInit( int logLevel, char *logFileName, int echoToConsole );
extern void logPrintf( int logLevel, char *ident, const char * format, ... );




