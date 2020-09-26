LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := fileScanner
LOCAL_SRC_FILES := jni_init.c file_scanner.c

LOCAL_LDLIBS :=-llog

LOCAL_CFLAGS := -fvisibility=hidden #只有使用 JNIEXPORT 关键字声明的函数才会出现在符号表中

include $(BUILD_SHARED_LIBRARY)
