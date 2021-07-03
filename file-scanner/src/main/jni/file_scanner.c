#include "file_scanner.h"

#if DEBUG || AND_DEBUG

#include <stdio.h>
#include <sys/time.h>

typedef signed long long s64;
typedef s64 zl_time;

zl_time systemTimeMillis() {
    struct timeval tm;
    gettimeofday(&tm, NULL);
    return tm.tv_sec * 1000 + tm.tv_usec / 1000;
}

pthread_mutex_t debugMutex = PTHREAD_MUTEX_INITIALIZER;
zl_time startTime;
int mallocCount = 0;
int freeCount = 0;
int openDirCount = 0;
int closeDirCount = 0;
static int findCount;
#endif


void *myMalloc(size_t byte_count) {
#if DEBUG || AND_DEBUG
    pthread_mutex_lock(&debugMutex);
    mallocCount++;
    pthread_mutex_unlock(&debugMutex);
#endif

    return malloc(byte_count);
}

void myFree(void *p) {
#if DEBUG || AND_DEBUG
    pthread_mutex_lock(&debugMutex);
    if (p) freeCount++;
    pthread_mutex_unlock(&debugMutex);
#endif

    free(p);
}

static void logStart() {
#if DEBUG || AND_DEBUG
    startTime = systemTimeMillis();
#endif
}

static void logFinish() {
#if DEBUG || AND_DEBUG
    LOG("scan finished---file total:%d, time:%lld\n", findCount, (systemTimeMillis() - startTime));
#endif
}

static void freePathNodes(PathNode *nodes) {
    while (nodes) {
        myFree(nodes->path);
        PathNode *tmp = nodes;
        nodes = nodes->next;
        myFree(tmp);
    }
}

void setThreadAttachCallback(Scanner *scanner, void (*attach)(Scanner *scanner),
                             void (*detach)(Scanner *scanner)) {
    scanner->attachJVMThreadCallback = attach;
    scanner->detachJVMThreadCallback = detach;
}


static void *threadScan(Scanner *scanner);

static void checkFileExt(Scanner *scanner, const char *dir, const char *fileName, int isNoMediaPath);

static void doFindFile(Scanner *scanner, char *dir, char *fileName);

static PathNode *takePathNode(Scanner *scanner);

static void pushPathNode(Scanner *scanner, PathNode *pathNode);

static int isNoMediaPath(Scanner *scanner, char * path);

static int filterNoMedia(Scanner *scanner, char *ext);


Scanner *createScanner() {

#if DEBUG || AND_DEBUG
    pthread_mutex_lock(&debugMutex);
    findCount = mallocCount = freeCount = openDirCount = closeDirCount = 0;
    pthread_mutex_unlock(&debugMutex);
#endif

    Scanner *scanner = myMalloc(sizeof(Scanner));
    memset(scanner, 0, sizeof(Scanner));
    scanner->scanHidden = 1;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    scanner->mutex = myMalloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(scanner->mutex, &attr)) {
        LOG("%s-%d:pthread_mutex_init fail\n", __FILE__, __LINE__);
        releaseScanner(scanner);
        return NULL;
    }

    scanner->cond = myMalloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(scanner->cond, NULL)) {
        LOG("%s-%d:pthread_cond_init fail\n", __FILE__, __LINE__);
        releaseScanner(scanner);
        return NULL;
    }

    return scanner;
}

void releaseScanner(Scanner *scanner) {
    if (!scanner) return;
    if (scanner->fileExts) {
        int i;
        for (i = 0; i < scanner->extCount; i++) {
            myFree(*(scanner->fileExts + i));
        }
        myFree(scanner->fileExts);
        scanner->fileExts = NULL;
    }

    if (scanner->filteredNoMediaExts) {
        int i;
        for (i = 0; i < scanner->filteredNoMediaExtCount; i++) {
            myFree(*(scanner->filteredNoMediaExts + i));
        }
        myFree(scanner->filteredNoMediaExts);
        scanner->filteredNoMediaExts = NULL;
    }

    if (scanner->mutex) {
        pthread_mutex_destroy(scanner->mutex);
        myFree(scanner->mutex);
        scanner->mutex = NULL;
    }

    if (scanner->cond) {
        pthread_cond_destroy(scanner->cond);
        myFree(scanner->cond);
        scanner->cond = NULL;
    }

    if (scanner->pathNodes) {
        freePathNodes(scanner->pathNodes);
        scanner->pathNodes = NULL;
    }

#if DEBUG || AND_DEBUG
    int createThd = scanner->createThreadCount;
    int exitThd = scanner->exitThreadCount;
#endif

    myFree(scanner);

#if DEBUG || AND_DEBUG
    pthread_mutex_lock(&debugMutex);
    int allRelease = 0;
    if (mallocCount == freeCount && openDirCount == closeDirCount && createThd == exitThd) {
        allRelease = 1;
    }
    LOG("release scanner\n");
    LOG("malloc-free allRelease:%d, threadCount:%d - %d, count: %d - %d,  open-close dir count:%d - %d\n", allRelease, createThd, exitThd, mallocCount, freeCount,
        openDirCount, closeDirCount);
    findCount = mallocCount = freeCount = openDirCount = closeDirCount = 0;
    pthread_mutex_unlock(&debugMutex);
#endif
}

int isScanning(Scanner *scanner) {
    pthread_mutex_lock(scanner->mutex);
    if (scanner->exitThreadCount < scanner->createThreadCount) {
        pthread_mutex_unlock(scanner->mutex);
        return 1;
    }
    pthread_mutex_unlock(scanner->mutex);
    return 0;
}

void setScanParams(Scanner *scanner, int extCount, char **exts, int filteredNoMediaExtCount, char **filteredNoMediaExts, int thdCount, int scanDepth, int detail) {
    if (!scanner) return;
    if (isScanning(scanner)) return;

    if (scanner->fileExts) {
        int i;
        for (i = 0; i < scanner->extCount; i++) {
            myFree(*(scanner->fileExts + i));
        }
        myFree(scanner->fileExts);
        scanner->fileExts = NULL;
    }
    if (scanner->filteredNoMediaExts) {
        int i;
        for (i = 0; i < scanner->filteredNoMediaExtCount; i++) {
            myFree(*(scanner->filteredNoMediaExts + i));
        }
        myFree(scanner->filteredNoMediaExts);
        scanner->filteredNoMediaExts = NULL;
    }
    scanner->extCount = extCount;
    scanner->fileExts = exts;
    scanner->filteredNoMediaExtCount = filteredNoMediaExtCount;
    scanner->filteredNoMediaExts = filteredNoMediaExts;
    scanner->threadCount = thdCount;
    scanner->scanDepth = scanDepth;
    scanner->fetchDetail = detail;
}

void setScanHiddenEnable(Scanner *scanner, int scanHidden) {
    if (!scanner) return;
    if (isScanning(scanner)) return;

    scanner->scanHidden = scanHidden;
}

void setScanPath(Scanner *scanner, int count, char **path) {
    if (!scanner || !path) return;
    if (isScanning(scanner)) return;

    if (scanner->pathNodes) {
        freePathNodes(scanner->pathNodes);
        scanner->pathNodes = NULL;
    }

    //init scan dirs
    PathNode *scanNodes = NULL;
    int i;
    for (i = 0; i < count; ++i) {
        const char *dirPath = *(path + i);
        char *localDirPath = myMalloc(strlen(dirPath) + 1);
        if (!localDirPath) {
            continue;
        }
        strcpy(localDirPath, dirPath);

        PathNode *fileNode = (PathNode *) myMalloc(sizeof(PathNode));
        if (!fileNode) {
            myFree(localDirPath);
            continue;
        }

        fileNode->path = localDirPath;
        fileNode->depth = 1;
        fileNode->isNoMediaPath = isNoMediaPath(scanner, localDirPath);
        fileNode->next = scanNodes;

        scanNodes = fileNode;
    }
    scanner->pathNodes = scanNodes;
}

void setCallbacks(Scanner *scanner,
                  void (*start)(Scanner *scanner),
                  void (*find)(Scanner *scanner, pthread_t threadId, const char *file, off_t size, time_t modify),
                  void (*finish)(Scanner *scanner, int isCancel)) {
    if (!scanner) return;
    scanner->onStart = start;
    scanner->onFind = find;
    scanner->onFinish = finish;
}

void cancelScan(Scanner *scanner) {
    if (!scanner) return;
    pthread_mutex_lock(scanner->mutex);
    if (scanner->exitThreadCount < scanner->createThreadCount) {
        scanner->status = SCAN_STATUS_CANCEL;
        pthread_cond_broadcast(scanner->cond);
    }
    pthread_mutex_unlock(scanner->mutex);
}

int startScan(Scanner *scanner) {
    if (!scanner) return -1;

    pthread_mutex_lock(scanner->mutex);
    if (scanner->exitThreadCount < scanner->createThreadCount) {
        pthread_mutex_unlock(scanner->mutex);
        return -1;
    }

    //not set scan dir
    if (scanner->pathNodes == NULL) {
        LOG("%s-%d:scan pathes is NULL\n", __FILE__, __LINE__);
        pthread_mutex_unlock(scanner->mutex);
        return -1;
    }

    scanner->createThreadCount = 0;
    scanner->waitingThreadCount = 0;
    scanner->exitThreadCount = 0;
    scanner->recycleOnFinish = 0;

    int i;
    for (i = 0; i < scanner->threadCount; i++) {
        pthread_t pt;
        if (pthread_create(&pt, NULL, threadScan, scanner)) {
            LOG("%s-%d:create thread fail\n", __FILE__, __LINE__);
            continue;
        }
        scanner->createThreadCount++;
    }

    //create thread fail
    if (!scanner->createThreadCount) {
        LOG("%s-%d:createThreadCount = 0\n", __FILE__, __LINE__);
        pthread_mutex_unlock(scanner->mutex);
        return -1;
    }

    logStart();

    scanner->status = SCAN_STATUS_SCAN;
    scanner->onStart(scanner);
    LOG("native callback-----scan start\n");
    pthread_mutex_unlock(scanner->mutex);
    return 0;
}

//thread run
static void *threadScan(Scanner *scanner) {
    if (scanner->attachJVMThreadCallback) scanner->attachJVMThreadCallback(scanner);

    while (1) {

        PathNode *dirNode = takePathNode(scanner);

        //thread will exit
        if (!dirNode) {
            pthread_mutex_lock(scanner->mutex);
            if (scanner->exitThreadCount + 1 == scanner->createThreadCount) {
                scanner->onFinish(scanner, scanner->status == SCAN_STATUS_CANCEL ? 1 : 0);
                logFinish();
                if (scanner->recycleOnFinish) {
                    if (scanner->detachJVMThreadCallback) scanner->detachJVMThreadCallback(scanner);
                    scanner->exitThreadCount++;
                    releaseScanner(scanner);
                    pthread_mutex_unlock(scanner->mutex);
                    pthread_detach(pthread_self());
                    return NULL;
                }
            }
            if (scanner->detachJVMThreadCallback) scanner->detachJVMThreadCallback(scanner);
            scanner->exitThreadCount++;
            pthread_cond_signal(scanner->cond);
            pthread_mutex_unlock(scanner->mutex);
            pthread_detach(pthread_self());
            break;
        }

        DIR *dir = opendir(dirNode->path);
        if (!dir) {
            LOG("dir open fail:%s\n", dirNode->path);
            freePathNodes(dirNode);
            continue;
        }

#if DEBUG || AND_DEBUG
        pthread_mutex_lock(&debugMutex);
        openDirCount++;
        pthread_mutex_unlock(&debugMutex);
#endif

        struct dirent *subFile = readdir(dir);
        while (subFile) {
            if (strcmp(subFile->d_name, ".") == 0 || strcmp(subFile->d_name, "..") == 0) {
                subFile = readdir(dir);
                continue;
            }

            if (!scanner->scanHidden && strncmp(subFile->d_name, ".", 1) == 0) {
                //LOG("ignore hidden dir:%s/%s\n", dirNode->path, subFile->d_name);
                subFile = readdir(dir);
                continue;
            }

            if (subFile->d_type == DT_DIR) {
                if (scanner->scanDepth == -1 || (dirNode->depth + 1) <= scanner->scanDepth) {
                    PathNode *subDir = (PathNode *) myMalloc(sizeof(PathNode));
                    if (subDir) {
                        char *fileName = (char *) myMalloc(
                                strlen(dirNode->path) + strlen(subFile->d_name) + 2);
                        strcpy(fileName, dirNode->path);
                        strcat(fileName, "/");
                        strcat(fileName, subFile->d_name);

                        subDir->path = fileName;
                        subDir->depth = dirNode->depth + 1;
                        if (dirNode->isNoMediaPath == 1 || isNoMediaPath(scanner, fileName)) {
                            subDir->isNoMediaPath = 1;
                        } else {
                            subDir->isNoMediaPath = 0;
                        }

                        pushPathNode(scanner, subDir);
                    }
                }
            } else if (subFile->d_type == DT_REG) {
                checkFileExt(scanner, dirNode->path, subFile->d_name, dirNode->isNoMediaPath);
            }

            subFile = readdir(dir);
        }
        closedir(dir);
        freePathNodes(dirNode);

#if DEBUG || AND_DEBUG
        pthread_mutex_lock(&debugMutex);
        closeDirCount++;
        pthread_mutex_unlock(&debugMutex);
#endif

    }
}

/**
 * 目录中是否有 .nomeida 文件
 * @param path
 * @return
 */
static int isNoMediaPath(Scanner *scanner, char * path) {
    if (scanner->filteredNoMediaExtCount < 1) {
        return 0;
    }
    char nomeida[strlen(path) + 10];
    strcpy(nomeida, path);
    strcat(nomeida, "/.nomedia");

    if (access(nomeida, F_OK) == 0) {
        return 1;
    }
    return 0;
}

static void pushPathNode(Scanner *scanner, PathNode *pathNode) {
    pthread_mutex_lock(scanner->mutex);
    pathNode->next = scanner->pathNodes;
    scanner->pathNodes = pathNode;
    pthread_cond_signal(scanner->cond);
    pthread_mutex_unlock(scanner->mutex);
}

/**
 * 从链表中取一个扫描路径,取不到会阻塞线程
 * @param scanner
 * @return
 */
static PathNode *takePathNode(Scanner *scanner) {
    while (1) {
        pthread_mutex_lock(scanner->mutex);

        if (scanner->status != SCAN_STATUS_SCAN) {
            pthread_mutex_unlock(scanner->mutex);
            return NULL;
        }

        PathNode *pathNode = scanner->pathNodes;
        if (pathNode) {
            scanner->pathNodes = scanner->pathNodes->next;
            pathNode->next = NULL;
            pthread_mutex_unlock(scanner->mutex);
            return pathNode;
        } else {
            if (scanner->waitingThreadCount + 1 == scanner->createThreadCount) {
                scanner->status = SCAN_STATUS_FINISH;
                pthread_cond_broadcast(scanner->cond);
            } else {
                scanner->waitingThreadCount++;
                pthread_cond_wait(scanner->cond, scanner->mutex);
                scanner->waitingThreadCount--;
            }
            pthread_mutex_unlock(scanner->mutex);
        }
    }
}

/**
 * 检查文件类型
 * @param dir       :目录名,
 * @param fileName  :文件名,目录名为NULL时文件名为全路径
 * @param isNoMediaPath  :是否是 nomedia path
 */
static void checkFileExt(Scanner *scanner, const char *dir, const char *fileName, int isNoMediaPath) {
    if (fileName == NULL) return;

    if (!scanner->extCount) {
        doFindFile(scanner, dir, fileName);
        return;
    }

    char *ext = strrchr(fileName, '.');
    if (!ext) return;

    int i;
    for (i = 0; i < scanner->extCount; i++) {
        if (strcasecmp(ext + 1, *(scanner->fileExts + i)) == 0) {
            if (isNoMediaPath && scanner->filteredNoMediaExtCount && filterNoMedia(scanner, ext)) {
                break;
            }

            doFindFile(scanner, dir, fileName);
            break;
        }
    }
}

/**
 * 是否过滤掉
 * @param scanner
 * @param ext 文件后缀,包含'.'
 * @return
 */
static int filterNoMedia(Scanner *scanner, char *ext) {
    int i;
    for (i = 0; i < scanner->filteredNoMediaExtCount; i++) {
        if (strcasecmp(ext + 1, *(scanner->filteredNoMediaExts + i)) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * find file
 * @param dir       :目录名,
 * @param fileName  :文件名,目录名为NULL时文件名为全路径
 */
static void doFindFile(Scanner *scanner, char *dir, char *fileName) {
    char *path = fileName;
    if (dir) {
        path = (char *) myMalloc(strlen(dir) + strlen(fileName) + 2);
        strcpy(path, dir);
        strcat(path, "/");
        strcat(path, fileName);
    }
    if (scanner->fetchDetail) {
        struct stat fStat;
        if (stat(path, &fStat) == 0) {
            scanner->onFind(scanner, pthread_self(), path, fStat.st_size, fStat.st_mtim.tv_sec);
        } else {
            scanner->onFind(scanner, pthread_self(), path, 0, 0);
        }
    } else {
        scanner->onFind(scanner, pthread_self(), path, 0, 0);
    }
    if (dir) myFree(path);

#if DEBUG || AND_DEBUG
    pthread_mutex_lock(&debugMutex);
    findCount++;
    pthread_mutex_unlock(&debugMutex);
#endif

}