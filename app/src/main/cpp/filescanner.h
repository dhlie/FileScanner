#ifndef __FILESCANNER__
#define __FILESCANNER__

#define AND_DEBUG   1
#define DEBUG       1

#if AND_DEBUG
#include <android/log.h>
//int __android_log_print(int prio, const char *tag,  const char *fmt, ...)
#define  LOG(x...)  __android_log_print(ANDROID_LOG_INFO,"scanner-native",x)
#elif DEBUG
#define LOG(x...) printf(x)
#else
#define LOG(str, x...)
#endif

/**
 *
 * @param attach
 * @param detach
 * */
void setThreadAttachCallback(void (*attach)(void), void (*detach)(void));

/**
 * init scanner
 *  sufCount    - 要查找的文件类型的个数,0表示匹配所有文件
 *  suf         - 要查找的文件类型
 *  thdCount    - 扫描线程个数,至少1个
 *  depth       - 目录扫描深度,-1表示无限制
 *  detail      - 是否获取命中文件的大小,最后修改时间(默认只返回路径)
 * */
int initScanner(int sufCount, const char **suf, int thdCount, int depth, int detail);

/**
 * 设置回调函数
 * */
void setCallbacks(void (*start)(void), void (*find)(const char *file, off_t size, time_t modify), void (*cancel)(void), void (*finish)(void));

/**
 * 开始扫描
 *  path        - 扫描目录
 *  int         - 0:开始扫描 -1:未开始扫描
 * */
int startScan(int count, const char **path);

/**
 * 取消扫描
 */
void cancelScan();

#endif
