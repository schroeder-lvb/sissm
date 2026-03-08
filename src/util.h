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


#define STRQUE_ELEM    (32)
#define STRQUE_STRLEN  (80)

extern char *getWord( char *strIn, int wordIndex, char *delim );
extern int foundMatch( char *line, char *table[], int caseConvert );
extern char *reformatIP( char *originalIP );
extern void strclr(char * s);
extern void strRemoveInPlace(char * s, char *toremove );
extern void strTrimInPlace(char * s);
extern void strToLowerInPlace(char * s);
extern int  strSubReplace( char *strIn, char *strFind, char *strReplace, int strMaxSize, char *strOut );
extern int  strSubReplaceWord( char delim, char *strIn, char *strFind, char *strReplace, int strMaxSize, char *strOut );
extern int  isReadable( char *fileName );
extern void  replaceDoubleColonWithBell( char *strInOut );
extern char *computeElapsedTime( unsigned long timeMark, unsigned long timeNow );

extern void strClean( char *strIn );

extern int debugPoke( char *fileName, int *valueOut );

// string queue collection
//
typedef struct {
   char strData[STRQUE_ELEM][STRQUE_STRLEN];
   int  head, tail, limit;
} strQue, *strQuePtr;
extern void strInitQue( strQuePtr qp, int queDepthLimit );
extern char *strDeQue( strQuePtr qp );
extern int strEnQue( strQuePtr qp, char *strIn );


