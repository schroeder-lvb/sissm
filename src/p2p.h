
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

extern void   p2pInit( void );

extern void   p2pSetF( char *varName, double value );
extern void   p2pSetL( char *varName, unsigned long value );
extern void   p2pSetS( char *varName, char *value );

extern double p2pGetF( char *varName, double defaultvalue );
extern unsigned long p2pGetL( char *varName, unsigned long defaultvalue );
extern char  *p2pGetS( char *varName, char *devaultValue );


