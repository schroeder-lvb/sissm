//  ==============================================================================================
//
//  Module: SISSM "Simple Insurgency Sandstorm Server Manager"
//
//  Description:
//  Top level, init, main loop, event & polling dispatcher
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================
//

#define SISSM_TEST    (0)

#define SISSM_MAXPLAYERS (64)

extern char *sissmGetConfigPath( void );
extern void sissmServerRestart( void );
extern char *sissmVersion( void );
extern int  sissmRestartServer( void );


