#include "file_scanner.h"


typedef struct callback {
    JavaVM *javaVM;
    jobject glCallback;
    jmethodID onStartMethodId;
    jmethodID onFindMethodId;
    jmethodID onFinishMethodId;
} Callback;

void releaseCallback(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        if (callback->glCallback) {
            (*env)->DeleteGlobalRef(env, callback->glCallback);
            callback->glCallback = NULL;
        }

        myFree(callback);
        scanner->jniCallbackClass = NULL;
    }

}

void onAttachThread(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && callback->javaVM) {
        JNIEnv *env;
        (*callback->javaVM)->AttachCurrentThread(callback->javaVM, &env, NULL);
    }
}

void onDetachThread(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && callback->javaVM) {
        (*callback->javaVM)->DetachCurrentThread(callback->javaVM);
    }
}

void onStart(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && callback->javaVM && callback->glCallback && callback->onStartMethodId) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        (*env)->CallVoidMethod(env, callback->glCallback, callback->onStartMethodId);
    }
}

void onFind(Scanner *scanner, pthread_t threadId, const char *file, off_t size, time_t modify) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
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
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && callback->javaVM) {
        JNIEnv *env;
        (*callback->javaVM)->GetEnv(callback->javaVM, (void **) &env, JNI_VERSION_1_6);
        if (callback->glCallback && callback->onFinishMethodId) {
            (*env)->CallVoidMethod(env, callback->glCallback, callback->onFinishMethodId, (jboolean) isCancel);
        }
    }
    releaseCallback(scanner);
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
    }
}

JNIEXPORT void
jniSetScanParams(JNIEnv *env, jobject obj, jlong handle, jobjectArray sufArr, jint thdCount,
                 jint depth, jboolean detail) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

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

    setScanParams(scanner, size, exts, thdCount, depth, detail);
    setCallbacks(scanner, onStart, onFind, onFinish);
    setThreadAttachCallback(scanner, onAttachThread, onDetachThread);
}

JNIEXPORT void jniSetScanHideDir(JNIEnv *env, jobject obj, jlong handle, jboolean scan) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    setScanHideDir(scanner, scan);
}

JNIEXPORT void jniSetScanNoMediaDir(JNIEnv *env, jobject obj, jlong handle, jboolean scan) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    setScanNoMediaDir(scanner, scan);
}

JNIEXPORT void jniSetScanPath(JNIEnv *env, jobject obj, jlong handle, jobjectArray jPathArr) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

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

        setScanPath(scanner, size, pathArr);

        for (i = 0; i < size; i++) {
            myFree(*(pathArr + i));
        }
        myFree(pathArr);
    }
}

JNIEXPORT int jniStartScan(JNIEnv *env, jobject obj, jlong handle, jobject jCallback) {
    if (!handle) return -1;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return -1;

    releaseCallback(scanner);

    if (jCallback == NULL) {
        return -1;
    }

    Callback *callback = myMalloc(sizeof(Callback));
    memset(callback, 0, sizeof(Callback));
    scanner->jniCallbackClass = callback;

    (*env)->GetJavaVM(env, &(callback->javaVM));
    callback->glCallback = (*env)->NewGlobalRef(env, jCallback);

    jclass jClass = (*env)->GetObjectClass(env, jCallback);
    callback->onStartMethodId = (*env)->GetMethodID(env, jClass, "onStart", "()V");
    if (callback->onStartMethodId == NULL) {
        LOG("native scanner:method onStart find fail\n");
        return -1;
    }

    callback->onFindMethodId = (*env)->GetMethodID(env, jClass, "onFind", "(JLjava/lang/String;JJ)V");
    if (callback->onFindMethodId == NULL) {
        LOG("native scanner:method onFind find fail\n");
        return -1;
    }

    callback->onFinishMethodId = (*env)->GetMethodID(env, jClass, "onFinish", "(Z)V");
    if (callback->onFinishMethodId == NULL) {
        LOG("native scanner:method onFinish find fail\n");
        return -1;
    }
    (*env)->DeleteLocalRef(env, jClass);

    return startScan(scanner);
}

JNIEXPORT void jniStopScan(JNIEnv *env, jobject obj, jlong handle) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;

    cancelScan(scanner);
}

static JNINativeMethod sScannerMethods[] = {
        {"nativeCreate",              "()J",                                              (void *) jniCreate},
        {"nativeRelease",             "(J)V",                                             (void *) jniRelease},
        {"nativeSetScanParams",       "(J[Ljava/lang/String;IIZ)V",                       (void *) jniSetScanParams},
        {"nativeSetHideDirEnable",    "(JZ)V",                                            (void *) jniSetScanHideDir},
        {"nativeSetNoMediaDirEnable", "(JZ)V",                                            (void *) jniSetScanNoMediaDir},
        {"nativeSetScanPath",         "(J[Ljava/lang/String;)V",                          (void *) jniSetScanPath},
        {"nativeStartScan",           "(JLd/hl/filescan/FileScanner$ScanCallback;)V",     (void *) jniStartScan},
        {"nativeStopScan",            "(J)V",                                             (void *) jniStopScan},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
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
