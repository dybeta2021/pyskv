#ifndef __PROCESS_MUTEX_H__
#define __PROCESS_MUTEX_H__

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef __APPLE__
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory.h>
#include <semaphore.h>
#include <unistd.h>
#endif

#ifdef __linux__
#include <fcntl.h>
#include <memory.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif


class CProcessMutex {
public:
    /* 默认创建匿名的互斥 */
    explicit CProcessMutex(const char *name = nullptr);

    ~CProcessMutex();

    bool Lock();

    bool UnLock();

private:
#ifdef _WIN32
    void *m_pMutex;
#endif

#ifdef __APPLE__
    sem_t *m_pSem;
#endif

#ifdef linux
    sem_t *m_pSem;
#endif

    char m_cMutexName[30]{};
};


#endif