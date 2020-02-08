//  ==============================================================================================
//
//  Module: RDRV
//
//  Description:
//  Game RCON interface driver (TCP/IP)
//
//  Original Author:
//  J.S. Schroeder (schroeder-lvb@outlook.com)    2019.08.14
//
//  Released under MIT License
//  ID Authenticator: c4c5a1eda6815f65bb2eefd15c5b5058f996add99fa8800831599a7eb5c2a04c
//
//  ==============================================================================================

#include <sys/types.h>
#ifndef _WIN32                 // Linux
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else                          // Windows
#include <winsock2.h>
#endif

#define  BUFSIZE_R    (16*1024)
#define  BUFSIZE_T    ( 1*1024)

#define  RDRV_DEBUGPRINT     0

#define  RCONPASSMAX  (80)
#define  RCONHOSTMAX  (256)
#define  RCVHDRSIZE   (12)             // 26

typedef struct {
    // set by init
    char               hostName[RCONHOSTMAX];
    int                portNo;
    char               rconPassword[RCONPASSMAX];
    // set by open
    int                sockfd;
    struct sockaddr_in serveraddr;
    int                serverlen;
    int                isConnected;
} rdrvObj, *rdrvPtr;


extern void rdrvLogFile( char *buf, int n, char *fileName );
extern int rdrvSend( rdrvObj *cPtr, int msgtype, char *rconcmd );
extern int rdrvReceive( rdrvObj *cPtr, char *bufin );
extern int rdrvConnect( rdrvObj *cPtr );
extern rdrvObj *rdrvInit( char *hostName, int portNo, char *rconPassword );
extern int rdrvDisconnect( rdrvObj *cPtr );
extern int rdrvDestroy( rdrvObj *cPtr );
extern int rdrvXmtRcv( rdrvObj *cPtr, int msgType, char *rconCmd, char *rconResp );
extern int rdrvCommand( rdrvObj *cPtr, int msgType, char *rconCmd, char *rconResp, int *bytesRead );
extern int rdrvTerminal( char *hostName, char *portNo, char *rconPassword );
