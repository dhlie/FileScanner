#include "file_scanner.h"

#if DEBUG || AND_DEBUG
int attachCount = 0;
int detachCount = 0;
#endif

typedef struct callback {
    jobject glCallback;
    jmethodID onStartMethodId;
    jmethodID onFindMethodId;
    jmethodID onFinishMethodId;
} Callback;

void releaseCallback(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback) {
        if (callback->glCallback) {
            JNIEnv *env;
            JavaVM *javaVm = (JavaVM*)scanner->javaVM;
            (*javaVm)->GetEnv(javaVm, (void **) &env, JNI_VERSION_1_6);
            (*env)->DeleteGlobalRef(env, callback->glCallback);
            callback->glCallback = NULL;
        }

        myFree(callback);
        scanner->jniCallbackClass = NULL;
    }
}

void onAttachThread(Scanner *scanner) {
    if (scanner->javaVM) {
        JNIEnv *env;
        JavaVM *javaVm = (JavaVM*)scanner->javaVM;

//        //设置线程名
//        JavaVMAttachArgs args;
//        args.version = JNI_VERSION_1_6;
//        char name[50] = { 0 };
//        char suffix[2] = {scanner->createThreadCount + 1 + '0', '\0'};
//        strcpy(name, "fileScanner-");
//        strcat(name, suffix);
//        args.name = name;
//        args.group = NULL;
//        (*javaVm)->AttachCurrentThread(javaVm, &env, &args);
        (*javaVm)->AttachCurrentThread(javaVm, &env, NULL);

#if DEBUG || AND_DEBUG
        pthread_mutex_lock(scanner->mutex);
        attachCount++;
        LOG("AttachCurrentThread thread count:%d\n", attachCount);
        pthread_mutex_unlock(scanner->mutex);
#endif
    }
}

void onDetachThread(Scanner *scanner) {
    if (scanner->javaVM) {
        JavaVM *javaVm = (JavaVM*)scanner->javaVM;
        (*javaVm)->DetachCurrentThread(javaVm);

#if DEBUG || AND_DEBUG
        pthread_mutex_lock(scanner->mutex);
        detachCount++;
        LOG("DetachCurrentThread thread count:%d\n", detachCount);
        pthread_mutex_unlock(scanner->mutex);
#endif
    }
}

void onStart(Scanner *scanner) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && scanner->javaVM && callback->glCallback && callback->onStartMethodId) {
        JNIEnv *env;
        JavaVM *javaVm = (JavaVM*)scanner->javaVM;
        (*javaVm)->GetEnv(javaVm, (void **) &env, JNI_VERSION_1_6);
        (*env)->CallVoidMethod(env, callback->glCallback, callback->onStartMethodId);
    }
}

void onFind(Scanner *scanner, pthread_t threadId, const char *file, int64_t size, time_t modify) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && scanner->javaVM && callback->glCallback && callback->onFindMethodId) {
        JNIEnv *env;
        JavaVM *javaVm = (JavaVM*)scanner->javaVM;
        (*javaVm)->GetEnv(javaVm, (void **) &env, JNI_VERSION_1_6);
        jstring pathJStr = (*env)->NewStringUTF(env, file);
        (*env)->CallVoidMethod(env, callback->glCallback, callback->onFindMethodId,
                               (jlong) threadId, pathJStr, (jlong) size, (jlong) modify);
        (*env)->DeleteLocalRef(env, pathJStr);
    }
}

void onFinish(Scanner *scanner, int isCancel) {
    Callback *callback = (Callback *) scanner->jniCallbackClass;
    if (callback && scanner->javaVM && callback->glCallback && callback->onFinishMethodId) {
        JNIEnv *env;
        JavaVM *javaVm = (JavaVM*)scanner->javaVM;
        (*javaVm)->GetEnv(javaVm, (void **) &env, JNI_VERSION_1_6);
        (*env)->CallVoidMethod(env, callback->glCallback, callback->onFinishMethodId, (jboolean) isCancel);
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
    JavaVM* javaVm;
    (*env)->GetJavaVM(env, &javaVm);
    scanner->javaVM = javaVm;
    return (jlong) scanner;
}

JNIEXPORT void jniRelease(JNIEnv *env, jobject obj, jlong handle) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    pthread_mutex_lock(scanner->mutex);
    scanner->recycleOnFinish = 1;
    if (!isScanning(scanner)) {
        pthread_mutex_unlock(scanner->mutex);
        releaseScanner(scanner);
    } else {
        pthread_mutex_unlock(scanner->mutex);
    }
}

JNIEXPORT void
jniSetScanParams(JNIEnv *env, jobject obj, jlong handle, jobjectArray sufArr, jobjectArray filteredNoMediaSufArr, jint thdCount,
                 jint depth, jboolean detail) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    (*env)->PushLocalFrame(env, 2);
    jsize size = 0;
    if (sufArr != NULL) {
        size = (*env)->GetArrayLength(env, sufArr);
    }
    char **exts = NULL;
    if (size > 0) {
        (*env)->PushLocalFrame(env, size);
        exts = (char **) myMalloc(sizeof(char *) * size);
        int i;
        for (i = 0; i < size; i++) {
            jobject extJStr = (*env)->GetObjectArrayElement(env, sufArr, i);
            char *ext = jStringToStr(env, (jstring) extJStr);
            *(exts + i) = ext;
        }
        (*env)->PopLocalFrame(env, NULL);
    }

    jsize filteredSize = 0;
    if (filteredNoMediaSufArr != NULL) {
        filteredSize = (*env)->GetArrayLength(env, filteredNoMediaSufArr);
    }
    char **filteredExts = NULL;
    if (filteredSize > 0) {
        (*env)->PushLocalFrame(env, filteredSize);
        filteredExts = (char **) myMalloc(sizeof(char *) * filteredSize);
        int i;
        for (i = 0; i < filteredSize; i++) {
            jobject extJStr = (*env)->GetObjectArrayElement(env, filteredNoMediaSufArr, i);
            char *ext = jStringToStr(env, (jstring) extJStr);
            *(filteredExts + i) = ext;
        }
        (*env)->PopLocalFrame(env, NULL);
    }

    setScanParams(scanner, size, exts, filteredSize, filteredExts, thdCount, depth, detail);
    setCallbacks(scanner, onStart, onFind, onFinish);
    setThreadAttachCallback(scanner, onAttachThread, onDetachThread);
    (*env)->PopLocalFrame(env, NULL);
}

JNIEXPORT void jniSetScanHiddenEnable(JNIEnv *env, jobject obj, jlong handle, jboolean scanHidden) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    setScanHiddenEnable(scanner, scanHidden);
}

JNIEXPORT void jniSetScanPath(JNIEnv *env, jobject obj, jlong handle, jobjectArray jPathArr) {
    if (!handle) return;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return;

    (*env)->PushLocalFrame(env, 2);
    jsize size = (*env)->GetArrayLength(env, jPathArr);
    if (size > 0) {
        (*env)->PushLocalFrame(env, size);
        char **pathArr = (char **) myMalloc(sizeof(char *) * size);
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

        setScanPath(scanner, size, pathArr);

        for (i = 0; i < size; i++) {
            myFree(*(pathArr + i));
        }
        myFree(pathArr);
        (*env)->PopLocalFrame(env, NULL);
    }
    (*env)->PopLocalFrame(env, NULL);
}

JNIEXPORT int jniStartScan(JNIEnv *env, jobject obj, jlong handle, jobject jCallback) {
    if (!handle) return -1;
    Scanner *scanner = (Scanner *) handle;
    if (isScanning(scanner)) return -1;

    releaseCallback(scanner);

    if (jCallback == NULL) {
        return -1;
    }

    (*env)->PushLocalFrame(env, 5);
    Callback *callback = myMalloc(sizeof(Callback));
    memset(callback, 0, sizeof(Callback));
    scanner->jniCallbackClass = callback;

    callback->glCallback = (*env)->NewGlobalRef(env, jCallback);

    jclass jClass = (*env)->GetObjectClass(env, jCallback);
    callback->onStartMethodId = (*env)->GetMethodID(env, jClass, "onStart", "()V");
    if (callback->onStartMethodId == NULL) {
        LOG("native scanner:method onStart find fail\n");
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }

    callback->onFindMethodId = (*env)->GetMethodID(env, jClass, "onFind", "(JLjava/lang/String;JJ)V");
    if (callback->onFindMethodId == NULL) {
        LOG("native scanner:method onFind find fail\n");
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }

    callback->onFinishMethodId = (*env)->GetMethodID(env, jClass, "onFinish", "(Z)V");
    if (callback->onFinishMethodId == NULL) {
        LOG("native scanner:method onFinish find fail\n");
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }
    (*env)->PopLocalFrame(env, NULL);

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
        {"nativeSetScanParams",       "(J[Ljava/lang/String;[Ljava/lang/String;IIZ)V",    (void *) jniSetScanParams},
        {"nativeSetScanHiddenEnable", "(JZ)V",                                            (void *) jniSetScanHiddenEnable},
        {"nativeSetScanPath",         "(J[Ljava/lang/String;)V",                          (void *) jniSetScanPath},
        {"nativeStartScan",           "(JLcom/dhl/filescanner/FileScanner$ScanCallback;)I",(void *) jniStartScan},
        {"nativeStopScan",            "(J)V",                                             (void *) jniStopScan},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    static const char *const className = "com/dhl/filescanner/FileScanner";

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
