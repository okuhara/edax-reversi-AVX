LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := aEdax # should be renamed to lib..aEdax..so afterwords
LOCAL_CFLAGS += -DUNICODE -DANDROID=1
LOCAL_SRC_FILES := all.c board_sse.c.neon eval_sse.c.neon flip_neon_bitscan.c.neon
LOCAL_ARM_NEON := true
# cmd-strip :=
include $(BUILD_EXECUTABLE)
