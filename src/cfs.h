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


#define CFS_FILENAME_MAX   (1024) 
#define CFS_FETCH_MAX      (1024)

typedef struct {

    char cfsFilename[CFS_FILENAME_MAX];
    FILE *fpr;

} cfsObj, *cfsPtr;

extern cfsPtr cfsCreate( char *fileName );
extern void cfsDestroy( cfsPtr pCfs );
extern char *cfsFetchStr( cfsPtr pCfz, char *varName, char *defaultValue );
extern double cfsFetchNum( cfsPtr pCfz, char *varName, double defaultValue );


