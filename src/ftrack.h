//  ==============================================================================================
//
//  Module: FTRACK
//
//  Description:
//  Game logfile tracking (tail)
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  Reference:
//  https://stackoverflow.com/questions/11221186/how-do-i-find-a-filename-given-a-file-pointer
//
//  ==============================================================================================

#define FTRACK_FILENAME_MAX   (1024)

typedef struct {

#ifdef _WIN32
    FILE *fpr;                                   // file handle
#else
    int  fpr;                // low-level file handle ala open()
#endif

    char originalFileName[FTRACK_FILENAME_MAX];  // filename specified by the caller
    char baselineFileName[FTRACK_FILENAME_MAX];  // system readback filename after open (full path)
    unsigned long lastFileSize;                  // file size on last resync

}  ftrackObj, *ftrackPtr;


extern ftrackObj *ftrackOpen( char *fileName ) ;
extern int ftrackResync( ftrackObj *fPtr );
extern void ftrackClose( ftrackObj *fPtr );
extern int ftrackTailOfFile( ftrackObj *fPtr, char *strBuffer, int maxStringSize, int seekToEnd  );

