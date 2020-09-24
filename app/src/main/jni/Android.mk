LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := scanner
LOCAL_SRC_FILES := jni_init.c filescanner.c

LOCAL_LDLIBS :=-llog

include $(BUILD_SHARED_LIBRARY)
