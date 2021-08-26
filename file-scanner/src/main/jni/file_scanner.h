#ifndef __FILESCANNER__
#define __FILESCANNER__

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <jni.h>
#include <unistd.h>

#define AND_DEBUG   0
#define DEBUG       0

#if AND_DEBUG
#include <android/log.h>
//int __android_log_print(int prio, const char *tag,  const char *fmt, ...)
#define  LOG(x...)  __android_log_print(ANDROID_LOG_INFO,"scanner-native",x)
#elif DEBUG
#define LOG(x...) printf(x)
#else
#define LOG(str, x...)
#endif

#ifndef SCAN_STATUS_IDLE
#define SCAN_STATUS_IDLE 0
#define SCAN_STATUS_SCAN 1
#define SCAN_STATUS_CANCEL 2
#define SCAN_STATUS_FINISH 3
#endif

typedef struct path_node {
    struct path_node *next;
    char *path;
    int depth;
    int isNoMediaPath;// 0或1, 是否是 nomedia 目录. nomedia 目录是当前目录或任何父级目录中有 .nomedia 文件的目录
} PathNode;

typedef struct scanner {
    pthread_mutex_t *mutex; //线程锁
    pthread_cond_t *cond;
    PathNode *pathNodes;    //文件路径

    //回调函数
    void (*onStart)(struct scanner *scanner);

    void (*onFind)(struct scanner *scanner, pthread_t threadId, const char *file, int64_t size, time_t modify);

    void (*onFinish)(struct scanner *scanner, int isCancel);

    void (*attachJVMThreadCallback)(struct scanner *scanner);

    void (*detachJVMThreadCallback)(struct scanner *scanner);

    void *javaVM;

    void *jniCallbackClass;

    char **fileExts;        //需要扫描的文件后缀
    char **filteredNoMediaExts;//nomeida 过滤类型
    int extCount;           //文件类型个数
    int filteredNoMediaExtCount;//nomeida 过滤类型个数
    int threadCount;        //扫描线程数
    int createThreadCount;  //实际创建成功的线程数
    int waitingThreadCount; //等待状态的线程数
    int exitThreadCount;    //扫描结束的线程数
    int scanDepth;          //目录扫描深度
    int status;             //扫描状态
    int fetchDetail;        //是否获取文件大小,最后修改时间
    int scanHidden;         //是否扫描隐藏目录和文件
    int recycleOnFinish;

} Scanner;

void *myMalloc(size_t byte_count);

void myFree(void *p);

/**
 *
 * @param attach
 * @param detach
 * */
void setThreadAttachCallback(Scanner *scanner, void (*attach)(Scanner *scanner), void (*detach)(Scanner *scanner));

Scanner *createScanner();

void releaseScanner(Scanner *scanner);

int isScanning(Scanner *scanner);

/**
 * set scanner params
 *  extCount                    - 要查找的文件类型的个数,0表示匹配所有文件
 *  exts                        - 要查找的文件类型
 *  filteredNoMediaExtCount     - .nomedia 所在目录要过滤掉的文件类型个数
 *  filteredNoMediaExts         - .nomedia 所在目录要过滤掉的文件类型
 *  thdCount                    - 扫描线程个数,至少1个
 *  scanDepth                   - 目录扫描深度,-1表示无限制
 *  detail                      - 是否获取命中文件的大小,最后修改时间(默认只返回路径)
 * */
void setScanParams(Scanner *scanner, int extCount, char **exts, int filteredNoMediaExtCount, char **filteredNoMediaExts, int thdCount, int scanDepth, int detail);

/**
 * 是否扫描隐藏目录或文件
 * @param scanner
 * @param scan
 */
void setScanHiddenEnable(Scanner *scanner, int scan);

/**
 * 设置扫描路径
 * @param scanner
 * @param count
 * @param path
 */
void setScanPath(Scanner *scanner, int count, char **path);

/**
 * 设置回调函数
 * */
void setCallbacks(Scanner *scanner,
                  void (*start)(Scanner *scanner),
                  void (*find)(Scanner *scanner, pthread_t threadId, const char *file, off_t size, time_t modify),
                  void (*finish)(Scanner *scanner, int isCancel));

/**
 * 开始扫描
 * @param scanner
 * @return 0 成功, 非 0 失败
 */
int startScan(Scanner *scanner);

/**
 * 取消扫描
 */
void cancelScan(Scanner *scanner);

#endif
