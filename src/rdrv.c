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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef _WIN32            // Linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#else                     // Windows
#include <winsock2.h>
#include "winport.h"
#define true  (1)
#define false (0)
#endif

#include "bsd.h"
#include "log.h"
#include "rdrv.h"
#include "util.h"

#if 1      // optimized
#define RDRV_DELAY_PASSWORD         (1000000)     // delay between password send and response
#define RDRV_DELAY_XMT2RCV          (  10000)     // delay between command transmit and receive
#define RDRV_DELAY_RETRANSMIT        (500000)     // delay before re-transmit on no response
#define RDRV_DELAY_CONN2XMT         ( 250000)     // delay between connect and first command out
#define RDRV_DELAY_RETRY_DISC2CONN  (1000000)     // delay between retry disconnect & reconnect
#define RDRV_DELAY_RETRY_CONN2XMT   (1000000)     // delay between retry reconnect and first send
#define RDRV_DELAY_RCVPOLL20         (100000)     // receive polling interval per iteration (retried 5x)
#endif

#if 0       // stable but slow
#define RDRV_DELAY_PASSWORD         (1000000)     // delay between password send and response
#define RDRV_DELAY_XMT2RCV          (1000000)     // delay between command transmit and receive
#define RDRV_DELAY_RETRANSMIT        (500000)     // delay before re-transmit on no response
#define RDRV_DELAY_CONN2XMT         (1000000)     // delay between connect and first command out
#define RDRV_DELAY_RETRY_DISC2CONN  (1000000)     // delay between retry disconnect & reconnect
#define RDRV_DELAY_RETRY_CONN2XMT   (1000000)     // delay between retry reconnect and first send
#define RDRV_DELAY_RCVPOLL20         (100000)     // receive polling interval per iteration (retried 10x)
#endif


//  ==============================================================================================
//  SetSocketBlockingEnabled
//
//  Enable/disable blocking read
//
static int SetSocketBlockingEnabled(int fd, int blocking)
{
    /** Returns true on success, or false if there was an error */
    if (fd < 0) return 0;

#ifdef _WIN32
    unsigned long mode = blocking ? 0 : 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return 0 ;
    flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
    return (fcntl(fd, F_SETFL, flags) == 0) ? 1 : 0;
#endif
}

//  ==============================================================================================
//  rdrvLogFile
//
//  Debug aid: store captured buffer to a file
//
void rdrvLogFile( char *buf, int n, char *fileName )
{
    int   i;
    FILE *fpw;

    if (fileName != NULL) {
        if (strlen( fileName ) != 0) {
            if (NULL != (fpw = fopen( fileName, "wb" ))) {
                for ( i = 0; i<n; i++ ) { fprintf( fpw, "%c", buf[i] ); }
                fclose( fpw );
            }
        }
    }
    return;
}


//  ==============================================================================================
//  rdrvSend
//
//  RCON packet transmit
//
int rdrvSend( rdrvObj *rPtr, int msgtype, char *rconcmd )
{
    char buf[BUFSIZE_T];
    int  errCode, outlen;
    // int  i;

    // bzero( buf, BUFSIZE_T );
    memset( buf, 0, BUFSIZE_T );
    buf[8] = msgtype;
    strlcpy( &buf[12], rconcmd, BUFSIZE_T-12 );
    buf[ strlen( rconcmd ) + 13 ] = 0 ;
    buf[ strlen( rconcmd ) + 14 ] = 0 ;
    outlen = strlen( rconcmd ) + 14;
    buf[0] = outlen - 4;
    buf[4] = 1; buf[5] = 2; buf[6] = 3; buf[7] = 4;

    rPtr->serverlen = sizeof( rPtr->serveraddr );
    // sendto(fd, buf, outlen, 0, (const struct sockaddr *) &serveraddr, serverlen);
#ifdef _WIN32
    errCode = 0;
    // send( rPtr->sockfd, buf, outlen, 0);
    errCode = (-1 == send( rPtr->sockfd, buf, outlen, 0 ));
#else
    // errCode = write( rPtr->sockfd, buf, outlen );
    errCode = (-1 == send( rPtr->sockfd, buf, outlen, MSG_NOSIGNAL));
#endif

#if RDRV_DEBUGPRINT
    printf("Sent: ");
    for (i=0; i<outlen; i++) printf("%02x ", 0xff & buf[i]);  // printf("%03d %02x\n", i, buf[i]);
    printf("\n");
#endif

    return( errCode == -1 );
}


//  ==============================================================================================
//  rdrvReceive
//
//  RCON packet receive
//
int rdrvReceive( rdrvObj *rPtr, char *bufin )
{
    int n, i;

    for ( i = 0; i<20; i++ ) {
#ifdef _WIN32
        n = recv( rPtr->sockfd, bufin, BUFSIZE_R, 0);
#else
        n = read( rPtr->sockfd, bufin, BUFSIZE_R );
#endif
        if ( n != -1 ) break;
        usleep( RDRV_DELAY_RCVPOLL20 ); 
   }
    if ( n != -1 ) {

#if RDRV_DEBUGPRINT
        printf("Rcvd: ");
        for (i=0; i<n; i++) printf("%02x ", 0xff & bufin[i]);  // printf("%03d %02x\n", i, bufin[i]);
        printf("\n");
#endif

        if ( n < RCVHDRSIZE ) {
            n = -1;
        }
        else {
            n = n - RCVHDRSIZE;
            if ( n != 0 ) {
                for (i = 0; i<n; i++) {
                    bufin[i] = bufin[i+RCVHDRSIZE];
                }
            }
        }
    }
    return( n );
}


//  ==============================================================================================
//  rdrvConnect
//
//  RCON connect to server - this includes opening a new port 
//  and authentication (server RCON password)
//
int rdrvConnect( rdrvObj *rPtr )
{
    struct hostent *server = NULL;
    int errCode = 1;
    char buf[BUFSIZE_T];

    // Create a socket
    //
#ifdef _WIN32
    rPtr->sockfd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
#else 
    rPtr->sockfd = socket( AF_INET, SOCK_STREAM, 0 );
#endif
    rPtr->isConnected = 0;
    if ( rPtr->sockfd < 0 ) {
        logPrintf(LOG_LEVEL_CRITICAL, "rdrv", "Critical Error, socket creation error");
    }
    else {
        // gethostbyname: get the server's DNS entry 
        // 
        server = gethostbyname( rPtr->hostName );
        if ( server == NULL ) {
            logPrintf(LOG_LEVEL_CRITICAL, "rdrv", "Critical Error, no such host ::%s::", rPtr->hostName );
#ifdef _WIN32
            closesocket( rPtr->sockfd );
#else
            close( rPtr->sockfd );
#endif
            rPtr->sockfd = -1;
        }
    }

    // Connect to the server
    //
    if ( rPtr->sockfd >= 0 ) {

        // Build the Internet address
        // 
        // bzero( (char *) &rPtr->serveraddr, sizeof( rPtr->serveraddr) );
        memset( (char *) &rPtr->serveraddr, 0, sizeof( rPtr->serveraddr) );
        rPtr->serveraddr.sin_family = AF_INET;
        // bcopy( (char *) server->h_addr, (char *) &rPtr->serveraddr.sin_addr.s_addr, server->h_length );
        memcpy( (char *) &rPtr->serveraddr.sin_addr.s_addr, (char *) server->h_addr, server->h_length );
        rPtr->serveraddr.sin_port = htons( rPtr->portNo );

        if ( connect( rPtr->sockfd, (struct sockaddr *) &rPtr->serveraddr, sizeof( rPtr->serveraddr ) ) < 0) {
            logPrintf(LOG_LEVEL_CRITICAL, "rdrv", "Warning: unable to connect to host via TCP/IP");
#ifdef _WIN32
            closesocket( rPtr->sockfd );
#else
            close( rPtr->sockfd );
#endif
            rPtr->sockfd = -1;
        }
        else {
            SetSocketBlockingEnabled( rPtr->sockfd, 0 );
            rPtr->isConnected = 1;
            errCode = 0;
        }
    }

    // Send RCON Password to the server
    //
    if (0 == errCode ) {
        rdrvSend( rPtr, 3, rPtr->rconPassword );
        usleep( RDRV_DELAY_PASSWORD );
        rdrvReceive( rPtr, buf );
    }

    return( errCode );
}

//  ==============================================================================================
//  rdrvInit
//
//  Create a RDRV instance and initialize
//
rdrvObj *rdrvInit( char *hostName, int portNo, char *rconPassword )
{
#ifdef _WIN32
    WSADATA  wsaData;
#endif
    rdrvObj *rPtr;

    rPtr = (rdrvObj *) calloc( 1, sizeof( rdrvObj ) );
    rPtr->isConnected = 0;
    if (NULL != rPtr) {
        rPtr->sockfd = -1;
        strlcpy(rPtr->rconPassword, rconPassword, RCONPASSMAX);
        rPtr->portNo = portNo;
        strlcpy(rPtr->hostName, hostName, RCONHOSTMAX);
#ifdef _WIN32
        WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
    }
    return( rPtr );
}

//  ==============================================================================================
//  rdrvDisconnect
//
//  Disconnect from a server
//
int rdrvDisconnect( rdrvObj *rPtr )
{
#ifdef _WIN32
    closesocket( rPtr->sockfd );
#else
    close( rPtr->sockfd );
#endif
    rPtr->sockfd = -1;
    rPtr->isConnected = 0;
    return 0;
}

//  ==============================================================================================
//  rdrvDestroy
//
//  Destroy RDRV instance (opposite of rdrvInit)
//
int rdrvDestroy( rdrvObj *rPtr )
{
    free( rPtr );
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}


//  ==============================================================================================
//  rdrvXmtRcv
//
//  Obsolete: legacy Xmit-Rcv pair, still here for testing purposes only
//  Use rdrvCommand instead.
//
int rdrvXmtRcv( rdrvObj *rPtr, int msgType, char *rconCmd, char *rconResp )
{
    int n, errCode = 0, retryCount = 0;

    while ( retryCount++ < 5 ) {
        if ( 0 == (errCode = rdrvSend( rPtr, msgType, rconCmd ))) {
            usleep( RDRV_DELAY_XMT2RCV );
            n = rdrvReceive( rPtr, rconResp );
            if ( n >= 0 ) break;
            if ( n <  0 ) usleep( RDRV_DELAY_RETRANSMIT );
        }
    }
    if ( errCode || ( retryCount >= 5) ) n = -1;
    
    return( n );
}

//  ==============================================================================================
//  rdrvCommand
//
//  RCON send-receive method that automates Command-Status interface with re-open of connection 
//  that lost comms and implementing a "Koraktor" multi-packet workaround as described in 
//  the Steam RCON Protocol document: 
//  https://developer.valvesoftware.com/wiki/Source_RCON_Protocol#Multiple-packet_Responses
// 
static int _rdrvCommand( rdrvObj *rPtr, int msgType, char *rconCmd, char *rconResp, int *bytesRead )
{
    int n, exitFlag = 0, retryCount = 0, errCode = 0;

    *bytesRead = 0;

    while ( !exitFlag ) {

        // if no handle (initial or reset condition)
        // try to open the connection.
        //
        if ( rPtr->isConnected == 0 ) {

            // (re-)connect
            //
            rdrvConnect( rPtr );             // ignore the return error
            usleep( RDRV_DELAY_CONN2XMT );
        }

        if ( rPtr->isConnected ) {
            errCode = rdrvSend( rPtr, msgType, rconCmd );
            usleep( RDRV_DELAY_XMT2RCV );
            if ( 0 == errCode ) {
                n = rdrvReceive( rPtr, rconResp );
                if (n > 0)  {
                    *bytesRead = n;
                    exitFlag = 1;
                }
                else {
                    *bytesRead = 0;
                    errCode = 1;
                }
            }
            if ( 0 != errCode ) {
                // disconnect and reconnect
                //
                logPrintf( LOG_LEVEL_CRITICAL, "rdrv", "Warning: RCON comms lost - re-opening with new channel");
                rdrvDisconnect( rPtr );  // ignore this error
                usleep( RDRV_DELAY_RETRY_DISC2CONN );
                rdrvConnect( rPtr );     // ignore this error
                usleep( RDRV_DELAY_RETRY_CONN2XMT );
            }
        }

        ++retryCount;
        if ( retryCount > 3 ) {
            exitFlag = 1;
        }
    }
    if ( retryCount > 3 ) errCode = 2;
    return( errCode );
}

int rdrvCommand( rdrvObj *rPtr, int msgType, char *rconCmd, char *rconResp, int *bytesRead )
{
    int         bytesRead2, errCode;
    static char rconRespCont[BUFSIZE_R];                // for receiving continuation buffer
    static char *rconCmdBlank = "";                               // for sending null string  

    errCode = _rdrvCommand( rPtr, msgType, rconCmd, rconResp, bytesRead );

    while (0 == errCode ) {

        // The above response code postfixes the data with "0x00 0x00" representing end of message
        // BUT this "0x00 0x00" will be missing if data has additional continuation packet(s)
        //
        errCode = _rdrvCommand( rPtr, 0, rconCmdBlank, rconRespCont, &bytesRead2 );
        if ( bytesRead2 <= 2) break;    // if there is no continuation, break out

        logPrintf(LOG_LEVEL_INFO, "rdrv", "Continuation RCON packet %02x %02x %02x containing ::%s::", 
            rconRespCont[0], rconRespCont[1], rconRespCont[2], &rconRespCont[3] );

        // Concatenate the continuation data to the end of master receive buffer
        // First 3 bytes of cont response should be { 0x00 0x00 0x80 } so we skip that
        //
        if (( *bytesRead + bytesRead2 - 3 ) >=  BUFSIZE_R ) break;   // memory bounds safety check
        memcpy( &rconResp[ *bytesRead ], &rconRespCont[ 3 ], bytesRead2 - 3 );
        *bytesRead += (bytesRead2 - 3);          // update totoal size not including 3 byte header
        
    }

    return( errCode );
}

//  ==============================================================================================
//  rdrvTerminal
//  
//  Simple RCON terminal 
//
int rdrvTerminal( char *hostName, char *portNoStr, char *rconPassword )
{
    int         i, exitLoop = 0, errCode = 1, bytesRead, portNo, isConnected = 0;
    size_t      rconCmdSize = 1024;
    char        *rconCmd = NULL;
    static char rconResp[1024*16];
    rdrvObj     *rPtr = NULL;
    
    // Parse Parameters, allocate getline() buffer [required] 
    //
    if ( 1 == sscanf( portNoStr, "%d", &portNo )) {
        rconCmd = (char *) malloc( rconCmdSize );
        if (rconCmd != NULL) errCode = 0;
    }
  
    // Open and Connect to the server 
    //
    if ( !errCode ) {
        // printf("\nSISSM RconTerm ::%s::%d::%s::\n", hostName, portNo, rconPassword );
        rPtr = rdrvInit( hostName, portNo, rconPassword );
        if ( rPtr != NULL ) {
            errCode = rdrvConnect( rPtr );
            if ( !errCode ) { 
                printf("\nConnection successful. Type 'q' to exit\n");
                isConnected = 1;
            }
        }
    }

    // Command Loop until 'q' is pressed
    //
    if ( isConnected ) {
        while ( !exitLoop ) {
#ifdef _WIN32
            memset( rconCmd, 0, rconCmdSize );
            gets_s( rconCmd, rconCmdSize );
            rconCmd[ strlen( rconCmd ) - 0 ] = 0x0d; 
            rconCmd[ strlen( rconCmd ) + 1 ] = 0x00;
#else
            getline( &rconCmd, &rconCmdSize, stdin );
            rconCmd[ strlen( rconCmd ) - 1 ] = 0x0d;
            rconCmd[ strlen( rconCmd ) - 0 ] = 0x00; 
#endif     
            if  (( rconCmd[0] == 'q' ) && ( rconCmd[1] == 0x0d )) 
                exitLoop = 1;
            else {
                errCode = rdrvCommand( rPtr, 2, rconCmd, rconResp, &bytesRead );
                if ( (0 == errCode) && (bytesRead > 0) ) {
                    for (i=0; i<bytesRead; i++) printf("%c", rconResp[i]);
                    printf("\n");
                }
                else {
                    printf("\nNo Response/Error Response\n");
                }
            }
        }
    }

    // Close the RCON port
    //
    if ( rPtr != NULL ) {
        if ( isConnected ) rdrvDisconnect( rPtr );
        rdrvDestroy( rPtr );
    }

    return( errCode );
}


