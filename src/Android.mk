LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := aEdax # should be renamed to lib..aEdax..so afterwords
LOCAL_CFLAGS += -DUNICODE
LOCAL_SRC_FILES := all.c
# LOCAL_ARM_NEON := true
# cmd-strip :=
include $(BUILD_EXECUTABLE)
