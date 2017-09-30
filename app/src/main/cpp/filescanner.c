#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include "filescanner.h"

#if DEBUG
#include <stdio.h>
#endif

typedef struct node {
    char *name;
    int depth;
    struct node *next;
}Node;

static int threadCount = 1;     //扫描线程数
static int scanDepth = -1;      //目录扫描深度
static const char **fileSuffix; //需要扫描的文件后缀
int suffixCount = 0;            //文件类型个数
static int fDetail;             //是否获取文件大小,最后修改时间
static struct stat fStat;

static int finishThd;           //扫描完成的线程个数
static int isCancel = 0;        //是否取消扫描
static int isStart = 0;

//---need free---
static pthread_mutex_t mutex;   //线程锁
static Node **dirGroup;         //文件分组,不同线程各扫描一组目录
static Node *rootFiles = NULL;  //根目录中的文件
//---need free---

//回掉函数
void (*onScannerStart)(void) = NULL;
void (*onScannerFind)(pthread_t threadId, const char *file, off_t size, time_t modify) = NULL;
void (*onScannerFinish)(int isCancel) = NULL;
void (*attachCallback)(void) = NULL;
void (*detachCallback)(void) = NULL;

#if DEBUG
int mallocCount = 0;
int freeCount = 0;
int openDirCount = 0;
int closeDirCount = 0;
static int findCount;
#endif

int initScanner(int sufCount, const char **suf, int thdCount, int depth, int detail) {
    suffixCount = sufCount;
    fileSuffix = suf;
    threadCount = thdCount;
    scanDepth = depth;
    fDetail = detail;
    return 0;
}

void setCallbacks(void (*start)(void), void (*find)(pthread_t threadId, const char *file, off_t size, time_t modify), void (*finish)(int isCancel)) {
    onScannerStart = start;
    onScannerFind = find;
    onScannerFinish = finish;
}

void cancelScan() {
    pthread_mutex_lock(&mutex);
    isCancel = 1;
    pthread_mutex_unlock(&mutex);
}

void* myMalloc(size_t byte_count) {
#if DEBUG
    pthread_mutex_lock(&mutex);
    mallocCount++;
    pthread_mutex_unlock(&mutex);
#endif
    return malloc(byte_count);
}

void myFree(void* p) {
#if DEBUG
    pthread_mutex_lock(&mutex);
    if (p) freeCount++;
    pthread_mutex_unlock(&mutex);
#endif
    free(p);
}

static void freeNodes(Node *node) {
    while (node) {
        myFree(node->name);
        Node *tmp = node;
        node = node->next;
        myFree(tmp);
    }
}

void setThreadAttachCallback(void (*attach)(void), void (*detach)(void)) {
    attachCallback = attach;
    detachCallback = detach;
}

static void *scanDir(void * node);
static void checkFileSuffix(const char *dir, const char *fileName);
static void thdScanFinish(Node* nodes);

/**
 * 扫描路径
 * @param count : 路径个数
 * @param path : 路径数组
 * @param int : 0-开始扫描 -1未开始扫描
 */
int startScan(int count, char **path) {
    if (!path) return -1;

    if (pthread_mutex_init(&mutex, NULL)) {
        LOG("%s-%d:pthread mutex init fail\n", "filescanner.c", __LINE__);
        return -1;
    }
    LOG("%s-%d:init mutex\n", "filescanner.c", __LINE__);

    //重置变量
    pthread_mutex_lock(&mutex);
    isCancel = finishThd = isStart = 0;
    rootFiles = NULL;
#if DEBUG
    findCount = mallocCount = freeCount = openDirCount = closeDirCount = 0;
#endif
    pthread_mutex_unlock(&mutex);

    dirGroup = (Node **)myMalloc(sizeof(Node *)*threadCount);
    if (!dirGroup) {
        LOG("%s-%d:destroy mutex\n", "filescanner.c", __LINE__);
        return -1;
    }
    memset(dirGroup, 0, sizeof(Node *)*threadCount);

    int dirCount = 0;
    struct dirent *subfile;
    int i;
    for (i = 0; i < count; ++i) {
        const char* dirPath = *(path+i);
        DIR *dir = opendir(dirPath);
        if (!dir) {
            LOG("%s-%d:open fail:%s\n", "filescanner.c", __LINE__, dirPath);
            continue;
        }

#if DEBUG
        pthread_mutex_lock(&mutex);
        openDirCount++;
        pthread_mutex_unlock(&mutex);
#endif

        subfile = readdir(dir);
        while (subfile) {
            if (strcmp(subfile->d_name, ".") == 0 || strcmp(subfile->d_name, "..") == 0) {
                subfile = readdir(dir);
                continue;
            }
            Node *fileNode = (Node *)myMalloc(sizeof(Node));
            if (!fileNode) break;
            char *filename = myMalloc(strlen(subfile->d_name)+strlen(dirPath)+2);
            if (!filename) {
                myFree(fileNode);
                break;
            }
            strcpy(filename, dirPath);
            strcat(filename, "/");
            strcat(filename, subfile->d_name);
            fileNode->name = filename;

            if (subfile->d_type == DT_DIR) {
                int index = dirCount%threadCount;
                fileNode->next = dirGroup[index];
                fileNode->depth = 1;
                dirGroup[index] = fileNode;
                dirCount++;
            } else if (subfile->d_type == DT_REG) {
                fileNode->next = rootFiles;
                rootFiles = fileNode;
            }

            subfile = readdir(dir);
        }
        closedir(dir);

#if DEBUG
        pthread_mutex_lock(&mutex);
        closeDirCount++;
        pthread_mutex_unlock(&mutex);
#endif
    }

    pthread_mutex_lock(&mutex);
    if (isCancel || (dirCount == 0 && rootFiles == NULL)) {
        pthread_mutex_unlock(&mutex);
        LOG("%s-%d:destroy mutex\n", "filescanner.c", __LINE__);
        for (i = 0; i < threadCount; i++) {
            freeNodes(dirGroup[i]);
        }
        myFree(dirGroup);
        freeNodes(rootFiles);
        pthread_mutex_destroy(&mutex);
        return -1;
    }
    pthread_mutex_unlock(&mutex);

    int j = 0;
    for (i = 0; i < threadCount; i++) {
        pthread_t pt;
        if(pthread_create(&pt, NULL, scanDir, dirGroup[i])) {
            LOG("%s-%d:create thread fail\n", "filescanner.c", __LINE__);
            freeNodes(dirGroup[i]);
            continue;
        } else {
            j++;
        }
    }

    //线程创建失败,释放资源
    if (!j) {
        pthread_mutex_destroy(&mutex);
        LOG("%s-%d:destroy mutex\n", "filescanner.c", __LINE__);
        for (i = 0; i < threadCount; i++) {
            freeNodes(dirGroup[i]);
        }
        myFree(dirGroup);
        freeNodes(rootFiles);
        return -1;
    }
    return 0;
}

//thread run
static void *scanDir(void * node) {
    if (attachCallback) attachCallback();
    Node* recycled = NULL;
    if (threadCount == 1) {
        onScannerStart();

        Node* files = rootFiles;
        while (files) {
            checkFileSuffix(NULL, files->name);
            files = files->next;
        }
        recycled = rootFiles;
    } else {
        Node* files = NULL;
        pthread_mutex_lock(&mutex);
        if (!isStart) {
            isStart = 1;
            files = rootFiles;
            onScannerStart();
        }
        pthread_mutex_unlock(&mutex);

        if (files) {
            while (files) {
                checkFileSuffix(NULL, files->name);
                files = files->next;
            }
            recycled = rootFiles;
        }
    }

#if DEBUG
    pthread_mutex_lock(&mutex);
    Node* nd = node;
    LOG("\nthread:%lu, scan dirs:>>>>>>>>>>>>>>>>>>>>>>>>>\n", pthread_self());
    while (nd) {
        LOG("%s\n", nd->name);
        nd = nd->next;
    }
    LOG("thread:%lu, scan dirs:<<<<<<<<<<<<<<<<<<<<<<<<<\n\n", pthread_self());
    fflush(stdout);
    pthread_mutex_unlock(&mutex);
#endif

    Node *fileNode = (Node *)node;
    if (fileNode && (scanDepth == -1 || fileNode->depth < scanDepth)) {
        while(fileNode) {
            pthread_mutex_lock(&mutex);
            if (isCancel) {
                pthread_mutex_unlock(&mutex);
                freeNodes(fileNode);
                break;
            }
            pthread_mutex_unlock(&mutex);

            DIR *dir = opendir(fileNode->name);
            if (!dir) {

#if DEBUG
                LOG("%s-%d:%s ", "filescanner.c", __LINE__, fileNode->name);
                fflush(stdout);
                perror("open fail");
#endif

                Node* next = fileNode->next;
                fileNode->next = recycled;
                recycled = fileNode;
                fileNode = next;
                continue;
            }

#if DEBUG
            pthread_mutex_lock(&mutex);
            openDirCount++;
            pthread_mutex_unlock(&mutex);
#endif

            struct dirent *subfile = readdir(dir);
            while (subfile) {
                if (strcmp(subfile->d_name, ".") == 0 || strcmp(subfile->d_name, "..") == 0) {
                    subfile = readdir(dir);
                    continue;
                }

                if (subfile->d_type == DT_DIR) {
                    if (scanDepth == -1 || (fileNode->depth + 1) < scanDepth) {
                        Node* newDir = NULL;
                        if (recycled) {
                            newDir = recycled;
                            recycled = recycled->next;
                            myFree(newDir->name);
                        } else {
                            newDir = (Node*)myMalloc(sizeof(Node));
                        }
                        if (newDir) {
                            char *fileName = (char *)myMalloc(strlen(fileNode->name)+strlen(subfile->d_name)+2);
                            strcpy(fileName, fileNode->name);
                            strcat(fileName, "/");
                            strcat(fileName, subfile->d_name);

                            newDir->depth = fileNode->depth + 1;
                            newDir->name = fileName;
                            newDir->next = fileNode->next;
                            fileNode->next = newDir;
                        }
                    }
                } else if (subfile->d_type == DT_REG) {
                    checkFileSuffix(fileNode->name, subfile->d_name);
                }

                subfile = readdir(dir);
            }
            closedir(dir);

#if DEBUG
            pthread_mutex_lock(&mutex);
            closeDirCount++;
            pthread_mutex_unlock(&mutex);
#endif


            Node *tmp = fileNode;
            fileNode = fileNode->next;
            tmp->next = recycled;
            recycled = tmp;
        }
    }

    thdScanFinish(recycled);
    if (detachCallback) detachCallback();
}

/**
 * 检查文件类型
 * @param dir       :目录名,
 * @param fileName  :文件名,目录名为NULL时表示全路径
 */
static void checkFileSuffix(const char *dir, const char *fileName) {
    if (fileName == NULL) return;
    if (!suffixCount) {
        char *path = fileName;
        if (dir) {
            path = (char *)myMalloc(strlen(dir)+strlen(fileName)+2);
            strcpy(path, dir);
            strcat(path, "/");
            strcat(path, fileName);
        }
        if (fDetail && stat(path, &fStat) == 0) {
            onScannerFind(pthread_self(), path, fStat.st_size, fStat.st_mtim.tv_sec);
        } else {
            onScannerFind(pthread_self(), path, 0, 0);
        }
        if (dir) myFree(path);

#if DEBUG
        pthread_mutex_lock(&mutex);
        findCount++;
        pthread_mutex_unlock(&mutex);
#endif
        return;
    }

    char *suffix = strrchr(fileName, '.');
    if (!suffix) return;

    int i;
    for (i = 0; i < suffixCount; i++) {
        if (strcasecmp(suffix+1, *(fileSuffix+i)) == 0) {
            char *path = fileName;
            if (dir) {
                path = (char *)myMalloc(strlen(dir)+strlen(fileName)+2);
                strcpy(path, dir);
                strcat(path, "/");
                strcat(path, fileName);
            }
            if (fDetail && stat(path, &fStat) == 0) {
                onScannerFind(pthread_self(), path, fStat.st_size, fStat.st_mtim.tv_sec);
            } else {
                onScannerFind(pthread_self(), path, 0, 0);
            }
            if (dir) myFree(path);

#if DEBUG
            pthread_mutex_lock(&mutex);
            findCount++;
            pthread_mutex_unlock(&mutex);
#endif
            break;
        }
    }
}

static void thdScanFinish(Node* nodes) {
    pthread_mutex_lock(&mutex);
    finishThd++;
    if (finishThd == threadCount) {
        pthread_mutex_unlock(&mutex);
        onScannerFinish(isCancel);
        myFree(dirGroup);
        freeNodes(nodes);

#if DEBUG
        LOG("%s-%d: scan finished---destroy mutex, file total:%d\n", "filescanner.c", __LINE__, findCount);
        LOG("%s-%d:malloc-free count:%d - %d,  open-close count:%d - %d\n", "filescanner.c", __LINE__, mallocCount, freeCount, openDirCount, closeDirCount);
#endif
        pthread_mutex_destroy(&mutex);
        return;
    }
    pthread_mutex_unlock(&mutex);
    freeNodes(nodes);
}
