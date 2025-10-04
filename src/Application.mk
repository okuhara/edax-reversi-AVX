APP_ABI := armeabi-v7a arm64-v8a x86 x86_64
APP_PLATFORM := android-16
APP_BUILD_SCRIPT := Android.mk
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"	# Android NDK r26 or earlier
LOCAL_LDFLAGS += "-Wl,-z,common-page-size=16384"	# Android NDK r22 or earlier
