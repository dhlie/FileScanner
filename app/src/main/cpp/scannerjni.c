#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "d_hl_filescann_FileScanner.h"
#include "filescanner.h"
#include <time.h>

char*       path = NULL;
char**      suffixs = NULL;
char**      pathes = NULL;
jmethodID   onStartId, onFindId, onFinishId;
JavaVM*     glJvm;
jobject     glCallbackObj = NULL;
jint        jniVer;
volatile int isScaning;

extern int suffixCount;
static int pathCount;

#if DEBUG
time_t      startTime;
#endif

//-----------------JavaVM-----------------
// jint        (*AttachCurrentThread)(JavaVM*, JNIEnv**, void*);
// jint        (*DetachCurrentThread)(JavaVM*);
// jint        (*GetEnv)(JavaVM*, void**, jint);

//-----------------JNIEnv-----------------
// void        (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
// jstring     (*NewStringUTF)(JNIEnv*, const char*);
// jsize       (*GetStringUTFLength)(JNIEnv*, jstring);
// const char* (*GetStringUTFChars)(JNIEnv*, jstring, jboolean*);
// void        (*ReleaseStringUTFChars)(JNIEnv*, jstring, const char*);
// jobject     (*NewGlobalRef)(JNIEnv*, jobject);
// jint        (*GetJavaVM)(JNIEnv*, JavaVM**);
// jint        (*GetVersion)(JNIEnv *);
// jsize       (*GetArrayLength)(JNIEnv*, jarray);
// jobject     (*GetObjectArrayElement)(JNIEnv*, jobjectArray, jsize);
// jclass      (*GetObjectClass)(JNIEnv*, jobject);
// jmethodID   (*GetMethodID)(JNIEnv*, jclass, const char*, const char*);
// void        (*DeleteGlobalRef)(JNIEnv*, jobject);

void onAttachThread() {
    JNIEnv* env;
    (*glJvm)->AttachCurrentThread(glJvm, &env, NULL);
}

void onDetachThread() {
    (*glJvm)->DetachCurrentThread(glJvm);
}

void onStart() {
    isScaning = 1;
#if DEBUG
    startTime = time(NULL);
    LOG("native callback-----scan start\n");
#endif

    if (glCallbackObj && onStartId) {
        JNIEnv* env;
        (*glJvm)->GetEnv(glJvm, (void**)&env, jniVer);
        (*env)->CallVoidMethod(env, glCallbackObj, onStartId);
    }
}

void onFind(pthread_t threadId, const char *file, off_t size, time_t modify) {
    if (glCallbackObj && onFindId) {
        JNIEnv* env;
        (*glJvm)->GetEnv(glJvm, (void**)&env, jniVer);
        jstring jstr = (*env)->NewStringUTF(env, file);
        (*env)->CallVoidMethod(env, glCallbackObj, onFindId, (jlong) threadId, jstr, (jlong) size, (jlong) modify);
        (*env)->DeleteLocalRef(env, jstr);
    }
}

void onFinish(int isCancel) {

#if DEBUG
    LOG("native callback-----scan finish   time:%ld\n", time(NULL)-startTime);
    fflush(stdout);
#endif

    if (glCallbackObj && onFinishId) {
        JNIEnv* env;
        (*glJvm)->GetEnv(glJvm, (void**)&env, jniVer);
        (*env)->CallVoidMethod(env, glCallbackObj, onFinishId, (jboolean) isCancel);
    }
    isScaning = 0;
}

char* jStringToStr(JNIEnv *env, jstring jstr) {
    int length = (*env)->GetStringUTFLength(env, jstr);
    if (length == 0) return 0;
    const char* str = (*env)->GetStringUTFChars(env, jstr, 0);
    char* s = (char*)malloc(strlen(str)+1);
    strcpy(s, str);
    (*env)->ReleaseStringUTFChars(env, jstr, str);
    return s;
}

void recycleScanner(JNIEnv *env) {
    if (path) {
        free(path);
        path = NULL;
    }

    if (glCallbackObj) {
        (*env)->DeleteGlobalRef(env, glCallbackObj);
        glCallbackObj = NULL;
    }

    if (suffixs) {
        int i;
        for (i = 0; i < suffixCount; i++) {
            free(*(suffixs+i));
        }
        free(suffixs);
        suffixs = NULL;
    }

    if (pathes) {
        int i;
        for (i = 0; i < pathCount; i++) {
            free(*(pathes+i));
        }
        free(pathes);
        pathes = NULL;
    }
}

JNIEXPORT void JNICALL Java_d_hl_filescann_FileScanner_nativeInitScanner
  (JNIEnv *env, jobject obj, jobjectArray sufArr, jint thdCount, jint depth, jboolean detail) {
    if (isScaning) return;
    if (suffixs) {
        int i;
        for (i = 0; i < suffixCount; i++) {
            free(*(suffixs+i));
        }
        free(suffixs);
        suffixs = NULL;
    }

    jniVer = (*env)->GetVersion(env);
    (*env)->GetJavaVM(env, &glJvm);
    jsize size = (*env)->GetArrayLength(env, sufArr);
    if (size > 0) {
        suffixs = (char**)malloc(sizeof(char*)*size);
        int i;
        for (i = 0; i < size; i++) {
            jobject jstr = (*env)->GetObjectArrayElement(env, sufArr, i);
            char* str = jStringToStr(env, (jstring)jstr);
#if DEBUG
        LOG("native scanner:match suffix:%s\n", str);
        fflush(stdout);
#endif
            *(suffixs+i) = str;
            (*env)->DeleteLocalRef(env, jstr);
        }
    }

    initScanner(size, suffixs, thdCount, depth, detail);
    setCallbacks(onStart, onFind, onFinish);
    setThreadAttachCallback(onAttachThread, onDetachThread);
    LOG("native scanner:init success\n");
}

JNIEXPORT void JNICALL Java_d_hl_filescann_FileScanner_nativeSetCallback
  (JNIEnv * env, jobject jobj, jobject jcallback) {
    if (isScaning) return;
    glCallbackObj = (*env)->NewGlobalRef(env, jcallback);
    jclass jclazz = (*env)->GetObjectClass(env, jcallback);
    onStartId = (*env)->GetMethodID(env, jclazz, "onStart", "()V");
    if (onStartId == NULL) {
        recycleScanner(env);
        LOG("native scanner:method onStart find fail\n");
        return;
    }

    onFindId = (*env)->GetMethodID(env, jclazz, "onFind", "(JLjava/lang/String;JJ)V");
    if (onFindId == NULL) {
        recycleScanner(env);
        LOG("native scanner:method onFind find fail\n");
        return;
    }

    onFinishId = (*env)->GetMethodID(env, jclazz, "onFinish", "(Z)V");
    if (onFinishId == NULL) {
        recycleScanner(env);
        LOG("native scanner:method onFinish find fail\n");
        return;
    }
    (*env)->DeleteLocalRef(env, jclazz);
}

/*
 * Class:     d_hl_filescann_FileScanner
 * Method:    nativeStartScan
 * Signature: ([Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_d_hl_filescann_FileScanner_nativeStartScan
  (JNIEnv *env, jobject obj, jobjectArray jarr) {
    if (isScaning) {
        LOG("native scanner:isScaning-------------\n");
        return;
    }
    isScaning = 1;

    if (pathes) {
        int i;
        for (i = 0; i < pathCount; i++) {
            free(*(pathes+i));
        }
        free(pathes);
        pathes = NULL;
    }

    jsize size = (*env)->GetArrayLength(env, jarr);
    if (size > 0) {
        pathes = (char**)malloc(sizeof(char*)*size);
        int i;
        for (i = 0; i < size; i++) {
            jobject jstr = (*env)->GetObjectArrayElement(env, jarr, i);
            char* str = jStringToStr(env, (jstring)jstr);
            //去掉路径结尾的'/'
            if (str[strlen(str) - 1] == '/') {
                size_t len = strlen(str);
                char* str2 = (char*)malloc(len);
                strncpy(str2, str, len);
                str2[len - 1] = '\0';
                *(pathes+i) = str2;
                free(str);
            } else {
                *(pathes+i) = str;
            }
            (*env)->DeleteLocalRef(env, jstr);
        }
        pathCount = size;
        if (startScan(size, pathes)) {
            isScaning = 0;
        }
    } else {
        isScaning = 0;
    }
}

JNIEXPORT void JNICALL Java_d_hl_filescann_FileScanner_nativeStopScan
  (JNIEnv *env, jobject jobj) {
    if (isScaning) {
        cancelScan();
    }
}

JNIEXPORT void JNICALL Java_d_hl_filescann_FileScanner_nativeRrecycle
  (JNIEnv *env, jobject jobj) {
    if (isScaning) {
        cancelScan();
    }
    recycleScanner(env);
}
