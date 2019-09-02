//  ==============================================================================================
//
//  Module: ALARM
//
//  Description:
//  Alarm event handling with callback feature
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================



#define SISSM_MAXALARMS       (64)

typedef struct {

    unsigned long alarmTime;
    int (*alarmCallbackFunction)( char * );
    int usedIndex;
    
} alarmObj, *alarmPtr;


extern void alarmInit( void );
extern alarmObj *alarmCreate( int (*alarmCBfunc)( char * ) );
extern int alarmDestroy( alarmObj *aPtr );
extern int alarmReset( alarmObj *aPtr, unsigned long timeSec);
extern int alarmCancel( alarmObj *aPtr );
extern long int alarmStatus( alarmObj *aPtr );
extern void alarmDispatch( void );

