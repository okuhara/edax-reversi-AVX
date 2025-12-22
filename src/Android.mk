LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := aEdax # should be renamed to lib..aEdax..so afterwords
LOCAL_CFLAGS += -DUNICODE
LOCAL_SRC_FILES := all.c board_sse.c.neon eval_sse.c.neon flip_neon_bitscan.c.neon android/cpu-features.c
# LOCAL_ARM_NEON := false
# LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"	# for pre-r26 NDK
# LOCAL_LDFLAGS += "-Wl,-z,common-page-size=16384"	# for pre-r22 NDK
# cmd-strip :=
include $(BUILD_EXECUTABLE)
