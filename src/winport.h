//
//  Module: WINPORT
//
//  Description:
//  Windows equivalent of Linux functions
//
//  ==============================================================================================

#ifdef _WIN32

extern void usleep(__int64 usec);
extern void sleep( int secWait );
extern char *strcasestr( char *haystack, char *needle );
extern int strcasecmp( char *str1, char *str2 );

#endif



