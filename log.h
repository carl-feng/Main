#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#define MAX_LIMIT 4096

#define USER_PRINT_BASE( format, args... )  printf( format, ##args )

#define USER_PRINT_0( format, args... ) \
{ \
        time_t timep; \
        time( &timep ); \
        struct tm *pTM = localtime( &timep ); \
        USER_PRINT_BASE( "[%04d-%02d-%02d %02d:%02d:%02d]"format, pTM->tm_year+1900, \
                pTM->tm_mon+1, pTM->tm_mday,\
                pTM->tm_hour, pTM->tm_min, pTM->tm_sec, ##args ) ; \
}

#define USER_PRINT_1( format, args... )  USER_PRINT_0( format, ##args )
#define USER_PRINT_2( format, args... )  USER_PRINT_1( "[TID:%d] "format, (int)syscall(SYS_gettid), ##args )
#define USER_PRINT( format, args...)  USER_PRINT_2( format, ##args )

#endif // LOG_H_INCLUDED
