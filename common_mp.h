/*
 *  Copyright (C) someonegg .
 *
 */
#ifndef __COMMON_MP_H__
#define __COMMON_MP_H__


#if defined(_WIN32) && defined(_MSC_VER)
 typedef __int8            int8_t;
 typedef unsigned __int8   uint8_t;
 typedef __int16           int16_t;
 typedef unsigned __int16  uint16_t;
 typedef __int32           int32_t;
 typedef unsigned __int32  uint32_t;
 typedef __int64           int64_t;
 typedef unsigned __int64  uint64_t;
#else
 #include <stdint.h>
#endif

#ifndef ASSERT
 #include <assert.h>
 #define ASSERT assert
#endif

#include <stdlib.h>  // malloc, free...
#include <string.h>  // memset...
#include <stdio.h>

#if defined(_WIN32) && defined(_MSC_VER)
 #define fseeko _fseeki64
 #define ftello _ftelli64
 #define atoll _atoi64
#endif

#ifndef EXIT_SUCCESS
 #define EXIT_SUCCESS     0
#endif
#ifndef EXIT_FAILURE
 #define EXIT_FAILURE     1
#endif
#define EXIT_FAILURE_ARG (EXIT_FAILURE + 1)


#ifdef __cplusplus

// simple helper class
class FileWrapper
{
    FILE* f;

public:
    FileWrapper(FILE* f = 0)
    {
        this->f = f;
    }

    FileWrapper& operator=(FILE* f)
    {
        close();

        this->f = f;

        return *this;
    }

    ~FileWrapper()
    {
        close();
    }

    operator FILE*()
    {
        return f;
    }

    void close()
    {
        if ( f != 0 )
        {
            fclose(f);
            f = 0;
        }
    }

private:
    FileWrapper(const FileWrapper &r);
    FileWrapper& operator=(const FileWrapper &r);
};

#endif


#endif // !__COMMON_MP_H__

