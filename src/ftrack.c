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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef  _WIN32
#include "winport.h"
#include <windows.h>
#include <tchar.h>
#include <fileapi.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#endif

#include "util.h"
#include "bsd.h"
#include "ftrack.h"
#include "log.h"


//  ==============================================================================================
//  Data definition 
//


//  ==============================================================================================
//  ftrackGetFilename (Linux)
//
//  Reads and returns the full-path of the filename currently opened by file handle 'fp'.
//  Error code is set if 'fp' is null (file was not already opened).
//

//  ==============================================================================================
//  ftrackGetFilename (Linux and MS Widnows versions)
//
//  Reads and returns the full-path of the filename currently opened by file handle 'fp'.
//  Error code is set if 'fp' is null (file was not already opened).
// 
//  References:
//  https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlea
//  fileapi.h (include Windows.h) -> #include <windows.h> #include <tchar.h> #include <stdio.h>
//
#ifdef _WIN32
static int ftrackGetFilename( FILE *fp, char *fileName )
{
    TCHAR Path[FTRACK_FILENAME_MAX];
    HANDLE hFile;
    // DWORD dwRet;
    int count;

    // arg4: VOLUME_NAME_DOS, VOLUME_NAME_GUID, VOLUME_NAME_NONE, VOLUME_NAME_NT
    // dwRet = GetFinalPathNameByHandle( hFile, Path, BUFSIZE, VOLUME_NAME_DOS );
    hFile = (HANDLE) _get_osfhandle( _fileno( fp ) );
    count = GetFinalPathNameByHandle( hFile, Path, FTRACK_FILENAME_MAX, VOLUME_NAME_NT );
    if ( count != 0 )
        strlcpy( fileName, (char *) Path, FTRACK_FILENAME_MAX );
    else
        logPrintf( LOG_LEVEL_CRITICAL, "ftrack", "OS Error getting filename ::%s::", GetLastError() );
    return( count == 0 );
#else
static int ftrackGetFilename( int fp, char *fileName )
{
    int     errCode = 1;
    char    proclnk[0x0fff];
    int     fNumber;
    ssize_t sizeName;

    strclr( fileName );
    // ssize_t r;
    if ( fp != -1 ) {
        errCode = 0;
        fNumber = fp;
        snprintf(proclnk, 0x0fff, "/proc/self/fd/%d", fNumber);
        sizeName = readlink(proclnk, fileName, 0x0fff);
        if ( sizeName > 0 ) {
            errCode = 0;
            fileName[ sizeName ] = 0;
        }
    }
    return( errCode );
#endif
}

//  ==============================================================================================
//  _ftrackIsChanged (local)
//
//  Detects log file rotation by checking for change in filename.  This conditions occurs
//  if the host application usess a rename ("mv") method to archive the active logfile to 
//  a rotated one.
//
#if 0
static int _ftrackIsChanged( FILE *fp, char *baselineName )
{
    int  isChanged = 1;
    char currFilename[0x0fff];
   
    if (0 == ftrackGetFilename( fp, currFilename ) ) {
        if (0 == strcmp( currFilename, baselineName )) {
            isChanged = 0;
        }
    }
    return( isChanged );
}
#endif


//  ==============================================================================================
//  ftrackOpen 
//
//  Creates and starts ftrack instance.  On open error, NULL is returned.
//
ftrackObj *ftrackOpen( char *fileName ) 
{
    ftrackObj *fPtr= NULL;
    char currFilename[256];

#ifdef _WIN32
    FILE *fpr;
    fpr = _fsopen( fileName, "r", _SH_DENYNO ); 
    if ( fpr != NULL )  {
#else
    int fpr;
    fpr = open( fileName, O_RDONLY | O_NONBLOCK | O_NDELAY  );
    if ( fpr != -1 )  {
#endif
        fPtr =  (ftrackObj *) calloc( 1, sizeof( ftrackObj ));
        if ( fPtr != NULL ) {
            fPtr->fpr = fpr;
            strlcpy( fPtr->originalFileName, fileName, FTRACK_FILENAME_MAX );
            if (0 == ftrackGetFilename( fpr, currFilename ) ) {
                strlcpy( fPtr->baselineFileName, currFilename, FTRACK_FILENAME_MAX );
                fPtr->lastFileSize = 0;
            }
            else {
                free( fPtr );
                fPtr = NULL;
            }
        }
    }
    return( fPtr ); 
}

//  ==============================================================================================
//  _ftrackGetFileSize (local)
//
//  Returns the filesize of the currently tracking file by filename (not file handle).  This
//  routine is used to detect log rotation as well as host application re-opening the log
//  file for write WITHOUT append (old log is wiped and size is reset to zero).
//
unsigned long int _ftrackGetFileSize( ftrackObj *fPtr )
{
#ifdef _WIN32
    HANDLE hFile;
    LARGE_INTEGER count;
    unsigned long retValue;

    hFile = (HANDLE) _get_osfhandle( _fileno( fPtr->fpr ) );
    GetFileSizeEx( hFile, &count );
    retValue = (unsigned long) count.LowPart;
    if (count.HighPart != 0L) {
        logPrintf( LOG_LEVEL_CRITICAL, "ftrack", "Tracking log file exceeded 4GB!" );
        retValue = 4294967295L;
    }
    return( retValue );
#else
    struct stat st;
    stat( fPtr->baselineFileName, &st );
    return( st.st_size );
#endif
}


//  ==============================================================================================
//  ftrackResync
//
//  Checks if the log file being "tailed" was rotated by the application.  Two methods are used:
//  1.  Looks for OS file name change (as a result of appplication 'mv' or renaming the file
//  2.  Looks for reduction in file size
//  When either is detected, the file is re-opened using the original file name
//
//  Returns:
//      non-zero value on error
//
int ftrackResync( ftrackObj *fPtr ) 
{
    int  i, errCode = 1;
    char currFileName[256];
    unsigned long fileSize;
    int fileChanged;

    if ( fPtr != NULL ) {
        errCode = 0;

        ftrackGetFilename( fPtr->fpr, currFileName );

        fileChanged = (0 != strcmp( currFileName, fPtr->baselineFileName ));

        if ( !fileChanged ) {
            fileSize = _ftrackGetFileSize( fPtr );
            if ( fileSize < fPtr->lastFileSize ) {
                fileChanged = 1;
                fPtr->lastFileSize = 0;
            }
            else {
                fPtr->lastFileSize = fileSize;
            }
        }

        if ( fileChanged ) {

            // printf("\n**%s**\n", fPtr->baselineFileName );
            // printf("\nclosing:  **%p**\n", fPtr->fpr );
            // fclose( fPtr->fpr );

	    logPrintf(LOG_LEVEL_INFO, "ftrack", "LogFile resynching due to possible file rotation by host");

#ifdef _WIN32
            // close the current file (if valid)
            if ( fPtr->fpr != NULL ) { fclose ( fPtr->fpr ); fPtr->fpr = NULL; };

            // try to open it up to 3x (3 seconds) before declaring fault
            for ( i=0; i<3; i++ ) {
                if (NULL != (fPtr->fpr = _fsopen( fPtr->originalFileName, "r", _SH_DENYNO ))) break;
                sleep( 1 );
            }
            if ( fPtr->fpr == NULL ) errCode = 1;
#else
            // close the current file (if valid)
            if ( fPtr->fpr != -1 ) { close( fPtr->fpr );  fPtr->fpr = -1; };

            // try to open it up to 3x (3 seconds) before declaring fault
            for ( i=0; i<3; i++ ) {
                if ( -1  != (fPtr->fpr = open( fPtr->baselineFileName, O_RDONLY | O_NONBLOCK | O_NDELAY  ))) break;
                sleep( 1 );
            }
            if ( fPtr->fpr == -1 ) errCode = 1;
#endif
            logPrintf(LOG_LEVEL_INFO, "ftrack", "LogFile reopen base name ::%s:: errCode %d", fPtr->baselineFileName, errCode );
        }
    }
    return( errCode );
}


//  ==============================================================================================
//  ftrackClose
//
//  Shuts down the file tracking instance, closes the file.
//
void ftrackClose( ftrackObj *fPtr )
{
#ifdef _WIN32
    if (fPtr->fpr != NULL) {
        fclose( fPtr->fpr );
        fPtr->fpr = NULL;
    }
#else
    if (fPtr->fpr != -1) {
        close( fPtr->fpr );
        fPtr->fpr = -1;
    }
#endif
    free( fPtr );
    return;
}

//  ==============================================================================================
//  ftrackTailOfFile
//  
//  This is the data 'read' method.  For cold start, set seekToEnd so that old log entries are
//  bypassed; for resync restart, clear seekToEnd which will process the file from the start.
//  If a (single) string line is successfully read, strBuffer is filled and the method returns
//  '0' indicating successful read.  As this method is a data poller, no new data will simply
//  return a non-zero value.
//
//  The calling routine should treat this as a non-blocking read function.  Typically the calling
//  routine will execute a delay (sleep/usleep) in the polling loop when data read returns
//  no data (i.e., when this function returns a non-zero value).
//
//
//  Returns:
//     0 = good data read
//     1 = no data (try again later), empty string is returned for strBuffer
//
int ftrackTailOfFile( ftrackObj *fPtr, char *strBuffer, int maxStringSize, int seekToEnd  )
{
#ifdef _WIN32
    int           x, noData;
    int           strIndex = 0;
    FILE          *fpr;

    fpr = fPtr->fpr;

    if ( fpr == NULL ) {
        strclr( strBuffer );
        return 1;
    }

    if ( seekToEnd ) {
        fseek( fpr, 0L, SEEK_END );
    }

    strclr( strBuffer );

    for ( ;; ) {
        x  = fgetc( fpr );
        if ( x != EOF) {
            // printf("%c\n", x);
            if (x == '\n')   {
                strBuffer[ strIndex ] = 0;
                strIndex = 0;
                noData = 0;
                break;
            }
            else {
                strBuffer[strIndex++] = x;
                if (strIndex > maxStringSize-2) strIndex = maxStringSize-2;
            }
        }
        else { 
            strclr( strBuffer );
            noData = 1;
            break; 
        }
    }
    return noData;
#else
    int           x, noData;
    int           strIndex = 0;
    int           fpr;
    unsigned char buf[8];

    fpr = fPtr->fpr;

    if ( fpr == -1 ) {
        strclr( strBuffer );
        return 1;
    }

    if ( seekToEnd ) {
        lseek( fpr, 0L, SEEK_END );
    }

    strclr( strBuffer );

    for ( ;; ) {
        x  = read( fpr, buf, (size_t) 1);
        if ( x == 1 ) {
            // printf("%c\n", x);
            if (buf[0] == '\n')   {
                strBuffer[ strIndex ] = 0;
                strIndex = 0;
                noData = 0;
                break;
            }
            else {
                strBuffer[strIndex++] = buf[0];
                if (strIndex > maxStringSize-2) strIndex = maxStringSize-2;
            }
        }
        else {
            strclr( strBuffer );
            noData = 1;
            break;
        }
    }
    return noData;
#endif
}



int __ftrack_main()
{
    ftrackObj *ftp;
    int stat;
    char strBuffer[1024];

    logPrintfInit( LOG_LEVEL_RAWDUMP, "zzz_delme.log", 1 );


    // ftp = ftrackOpen( "zzz_test.txt" );
    ftp = ftrackOpen( "C:/Program Files (x86)/Steam/steamapps/common/sandstorm_server/Insurgency/Saved/Logs/Insurgency.log" );

    if ( ftp == NULL ) {
        printf("\nOpen error\n");
        exit( 1 );
    }

    while ( 1 == 1 ) {
        stat = ftrackTailOfFile( ftp, strBuffer, 1024, 0 );
        if ( stat == 0 ) {
            printf("\n::%ld::%s::\n", _ftrackGetFileSize(ftp), strBuffer );
        }
        else {
            ftrackResync( ftp );
            usleep( 100000 );
        }
        
    }
    return 0;
}

