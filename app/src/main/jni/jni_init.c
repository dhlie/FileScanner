#include "file_scanner.h"

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


typedef struct callback {
    JavaVM *javaVM;
    jobject glCallback;
    jmethodID onStartMethodId;
    jmethodID onFindMethodId;
    jmethodID onFinishMethodId;
} Callback;

void releaseJniRef(JNIEnv *env, Scanner *scanner) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback) {
        if (callback->glCallback) {
            (*env)->DeleteGlobalRef(env, callback->glCallback);
            callback->glCallback = NULL;
        }

//    if (callback->onStartMethodId) {
//        (*env)->DeleteLocalRef(env, callback->onStartMethodId);
//    }
//    if (callback->onFindMethodId) {
//        (*env)->DeleteLocalRef(env, callback->onFindMethodId);
//    }
//    if (callback->onFinishMethodId) {
//        (*env)->DeleteLocalRef(env, callback->onFinishMethodId);
//    }

        myFree(callback);
    }
}

void onAttachThread(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback && callback->javaVM) {
        JNIEnv *env;
        (*callback->javaVM)->AttachCurrentThread(callback->javaVM, &env, NULL);
    }
}

void onDetachThread(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback && callback->javaVM) {
        (*callback->javaVM)->DetachCurrentThread(callback->javaVM);
    }
}

void onStart(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback && callback->javaVM && callback->glCallback && callback->onStartMethodId) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        (*env)->CallVoidMethod(env, callback->glCallback, callback->onStartMethodId);
    }
}

void onFind(Scanner *scanner, pthread_t threadId, const char *file, off_t size, time_t modify) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback && callback->javaVM && callback->glCallback && callback->onFindMethodId) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        jstring pathJStr = (*env)->NewStringUTF(env, file);
        (*env)->CallVoidMethod(env, callback->glCallback, callback->onFindMethodId,
                               (jlong) threadId, pathJStr, (jlong) size, (jlong) modify);
        (*env)->DeleteLocalRef(env, pathJStr);
    }
}

void onFinish(Scanner *scanner, int isCancel) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback && callback->javaVM) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        if (callback->glCallback && callback->onFinishMethodId) {
            (*env)->CallVoidMethod(env, callback->glCallback, callback->onFinishMethodId,
                                   (jboolean) isCancel);
        }
    }

}

void recycleCallback(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->callback;
    if (callback) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        if (callback->glCallback) {
            (*env)->DeleteGlobalRef(env, callback->glCallback);
            callback->glCallback = NULL;
        }

        myFree(callback);
    }

}

static char *jStringToStr(JNIEnv *env, jstring jstr) {
    int length = (*env)->GetStringUTFLength(env, jstr);
    if (length == 0) return 0;
    const char *str = (*env)->GetStringUTFChars(env, jstr, 0);
    char *chrArr = (char *) myMalloc(strlen(str) + 1);
    strcpy(chrArr, str);
    (*env)->ReleaseStringUTFChars(env, jstr, str);
    return chrArr;
}

JNIEXPORT jlong jniCreate(JNIEnv *env, jobject obj) {
    Scanner *scanner = createScanner();
    LOG("native scanner:jniCreate\n");
    return (jlong) scanner;
}

JNIEXPORT void jniRelease(JNIEnv *env, jobject obj, jlong handle) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    scanner->recycleOnFinish = 1;
    if (isScanning(scanner)) {
        cancelScan(scanner);
    } else {
        releaseScanner(scanner);
        LOG("native scanner:jniRelease\n");
    }
}

JNIEXPORT void jniScanHideDir(JNIEnv *env, jobject obj, jlong handle, jboolean scan) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    scanner->scanHideDir = scan;
    LOG("native scanner:jniScanHideDir:%d\n", scan);
}

JNIEXPORT void jniScanNoMediaDir(JNIEnv *env, jobject obj, jlong handle, jboolean scan) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    scanner->scanNoMediaDir = scan;
    LOG("native scanner:jniScanNoMediaDir:%d\n", scan);
}

JNIEXPORT void
jniInitScanner(JNIEnv *env, jobject obj, jlong handle, jobjectArray sufArr, jint thdCount,
               jint depth,
               jboolean detail) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;
    LOG("native scanner:jniInitScanner\n");

    jsize size = (*env)->GetArrayLength(env, sufArr);
    char **exts = NULL;
    if (size > 0) {
        exts = (char **) myMalloc(sizeof(char *) * size);
        (*env)->PushLocalFrame(env, size);
        int i;
        for (i = 0; i < size; i++) {
            jobject extJStr = (*env)->GetObjectArrayElement(env, sufArr, i);
            char *ext = jStringToStr(env, (jstring) extJStr);
            *(exts + i) = ext;
        }
        (*env)->PopLocalFrame(env, NULL);
    }

    initScanner(scanner, size, exts, thdCount, depth, detail);
    setCallbacks(scanner, onStart, onFind, onFinish, recycleCallback);
    setThreadAttachCallback(scanner, onAttachThread, onDetachThread);
}

JNIEXPORT void jniStartScan(JNIEnv *env, jobject obj, jlong handle, jobjectArray jPathArr) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;
    LOG("native scanner:jniStartScan\n");

    jsize size = (*env)->GetArrayLength(env, jPathArr);
    if (size > 0) {
        char **pathArr = (char **) myMalloc(sizeof(char *) * size);
        (*env)->PushLocalFrame(env, size);
        int i;
        for (i = 0; i < size; i++) {
            jobject jPathStr = (*env)->GetObjectArrayElement(env, jPathArr, i);
            char *str = jStringToStr(env, (jstring) jPathStr);
            size_t len = strlen(str);
            //去掉路径结尾的'/'
            if (len > 1 && str[len - 1] == '/') {
                str[len - 1] = '\0';
            }
            *(pathArr + i) = str;
        }
        (*env)->PopLocalFrame(env, NULL);

        startScan(scanner, size, pathArr);

        for (i = 0; i < size; i++) {
            myFree(*(pathArr + i));
        }
        myFree(pathArr);
    }

}

JNIEXPORT void jniSetCallback(JNIEnv *env, jobject obj, jlong handle, jobject jCallback) {

    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;
    LOG("native scanner:jniSetCallback\n");

    recycleCallback(scanner);

    if (jCallback == NULL) {
        return;
    }

    Callback *callback = myMalloc(sizeof(Callback));
    memset(callback, 0, sizeof(Callback));
    scanner->callback = callback;

    (*env)->GetJavaVM(env, &(callback->javaVM));
    callback->glCallback = (*env)->NewGlobalRef(env, jCallback);

    jclass jClass = (*env)->GetObjectClass(env, jCallback);
    callback->onStartMethodId = (*env)->GetMethodID(env, jClass, "onStart", "()V");
    if (callback->onStartMethodId == NULL) {
        LOG("native scanner:method onStart find fail\n");
        return;
    }

    callback->onFindMethodId = (*env)->GetMethodID(env, jClass, "onFind", "(JLjava/lang/String;JJ)V");
    if (callback->onFindMethodId == NULL) {
        LOG("native scanner:method onFind find fail\n");
        return;
    }

    callback->onFinishMethodId = (*env)->GetMethodID(env, jClass, "onFinish", "(Z)V");
    if (callback->onFinishMethodId == NULL) {
        LOG("native scanner:method onFinish find fail\n");
        return;
    }
    (*env)->DeleteLocalRef(env, jClass);
}

JNIEXPORT void jniStopScan(JNIEnv *env, jobject obj, jlong handle) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    LOG("native scanner:jniStopScan\n");

    cancelScan(scanner);
}

static JNINativeMethod sScannerMethods[] = {
        {"nativeCreate",              "()J",                                              (void *) jniCreate},
        {"nativeRelease",             "(J)V",                                             (void *) jniRelease},
        {"nativeInitScanner",         "(J[Ljava/lang/String;IIZ)V",                       (void *) jniInitScanner},
        {"nativeSetHideDirEnable",    "(JZ)V",                                            (void *) jniScanHideDir},
        {"nativeSetNoMediaDirEnable", "(JZ)V",                                            (void *) jniScanNoMediaDir},
        {"nativeStartScan",           "(J[Ljava/lang/String;)V",                          (void *) jniStartScan},
        {"nativeSetCallback",         "(JLd/hl/filescan/FileScanner$ScanCallback;)V",     (void *) jniSetCallback},
        {"nativeStopScan",            "(J)V",                                             (void *) jniStopScan},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOG("native scanner:JNI_OnLoad\n");
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    static const char *const className = "d/hl/filescan/FileScanner";

    jclass clazz = (*env)->FindClass(env, className);
    if (clazz == NULL) {
        return -1;
    }
    if ((*env)->RegisterNatives(env, clazz, sScannerMethods,
                                sizeof(sScannerMethods) / sizeof(sScannerMethods[0])) != JNI_OK) {
        return -1;
    }

    return JNI_VERSION_1_6;
}
