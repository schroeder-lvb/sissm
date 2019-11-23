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

extern char *getWord( char *strIn, int wordIndex, char *delim );
extern int foundMatch( char *line, char *table[], int caseConvert );
extern char *reformatIP( char *originalIP );
extern void strclr(char * s);
extern void strTrimInPlace(char * s);
extern void strToLowerInPlace(char * s);
extern int isReadable( char *fileName );
extern void  replaceDoubleColonWithBell( char *strInOut );

