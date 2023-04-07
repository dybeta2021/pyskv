#include "process_mutex.h"

#ifdef _WIN32

CProcessMutex::CProcessMutex(const char *name) {
    memset(m_cMutexName, 0, sizeof(m_cMutexName));
    int min = strlen(name) > (sizeof(m_cMutexName) - 1) ? (sizeof(m_cMutexName) - 1) : strlen(name);
    strncpy(m_cMutexName, name, min);

    // Convert char * to LPWSTR
    // https://stackoverflow.com/questions/6858524/convert-char-to-lpwstr
    wchar_t wtext[128];
    mbstowcs(wtext, name, strlen(name) + 1);//Plus null
    LPWSTR ptr_str = wtext;

    m_pMutex = CreateMutex(NULL, false, ptr_str);
}

CProcessMutex::~CProcessMutex() {
    CloseHandle(m_pMutex);
}

bool CProcessMutex::Lock() {
    //互斥锁创建失败
    if (NULL == m_pMutex) {
        return false;
    }

    DWORD nRet = WaitForSingleObject(m_pMutex, INFINITE);
    if (nRet != WAIT_OBJECT_0) {
        return false;
    }

    return true;
}

bool CProcessMutex::UnLock() {
    return ReleaseMutex(m_pMutex);
}

#endif

#ifdef __APPLE__

CProcessMutex::CProcessMutex(const char *name) {
    memset(m_cMutexName, 0, sizeof(m_cMutexName));
    int min = strlen(name) > (sizeof(m_cMutexName) - 1) ? (sizeof(m_cMutexName) - 1) : strlen(name);
    strncpy(m_cMutexName, name, min);
    m_pSem = sem_open(name, O_RDWR | O_CREAT, 0644, 1);
}

CProcessMutex::~CProcessMutex() {
    int ret = sem_close(m_pSem);
    if (0 != ret) {
        printf("sem_close error %d\n", ret);
    }
    sem_unlink(m_cMutexName);
}

bool CProcessMutex::Lock() {
    int ret = sem_wait(m_pSem);
    if (ret != 0) {
        return false;
    }
    return true;
}

bool CProcessMutex::UnLock() {
    int ret = sem_post(m_pSem);
    if (ret != 0) {
        return false;
    }
    return true;
}

#endif


#ifdef linux

CProcessMutex::CProcessMutex(const char *name) {
    memset(m_cMutexName, 0, sizeof(m_cMutexName));
    int min = strlen(name) > (sizeof(m_cMutexName) - 1) ? (sizeof(m_cMutexName) - 1) : strlen(name);
    strncpy(m_cMutexName, name, min);
    m_pSem = sem_open(name, O_RDWR | O_CREAT, 0644, 1);
}

CProcessMutex::~CProcessMutex() {
    int ret = sem_close(m_pSem);
    if (0 != ret) {
        printf("sem_close error %d\n", ret);
    }
    sem_unlink(m_cMutexName);
}

bool CProcessMutex::Lock() {
    int ret = sem_wait(m_pSem);
    if (ret != 0) {
        return false;
    }
    return true;
}

bool CProcessMutex::UnLock() {
    int ret = sem_post(m_pSem);
    if (ret != 0) {
        return false;
    }
    return true;
}

#endif