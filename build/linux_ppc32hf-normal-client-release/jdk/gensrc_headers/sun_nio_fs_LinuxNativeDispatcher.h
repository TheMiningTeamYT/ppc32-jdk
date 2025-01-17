/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sun_nio_fs_LinuxNativeDispatcher */

#ifndef _Included_sun_nio_fs_LinuxNativeDispatcher
#define _Included_sun_nio_fs_LinuxNativeDispatcher
#ifdef __cplusplus
extern "C" {
#endif
#undef sun_nio_fs_LinuxNativeDispatcher_SUPPORTS_OPENAT
#define sun_nio_fs_LinuxNativeDispatcher_SUPPORTS_OPENAT 2L
#undef sun_nio_fs_LinuxNativeDispatcher_SUPPORTS_FUTIMES
#define sun_nio_fs_LinuxNativeDispatcher_SUPPORTS_FUTIMES 4L
#undef sun_nio_fs_LinuxNativeDispatcher_SUPPORTS_BIRTHTIME
#define sun_nio_fs_LinuxNativeDispatcher_SUPPORTS_BIRTHTIME 65536L
/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    setmntent0
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_setmntent0
  (JNIEnv *, jclass, jlong, jlong);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    getmntent0
 * Signature: (JLsun/nio/fs/UnixMountEntry;JI)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_getmntent0
  (JNIEnv *, jclass, jlong, jobject, jlong, jint);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    endmntent
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_endmntent
  (JNIEnv *, jclass, jlong);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    getlinelen
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_getlinelen
  (JNIEnv *, jclass, jlong);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    fgetxattr0
 * Signature: (IJJI)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_fgetxattr0
  (JNIEnv *, jclass, jint, jlong, jlong, jint);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    fsetxattr0
 * Signature: (IJJI)V
 */
JNIEXPORT void JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_fsetxattr0
  (JNIEnv *, jclass, jint, jlong, jlong, jint);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    fremovexattr0
 * Signature: (IJ)V
 */
JNIEXPORT void JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_fremovexattr0
  (JNIEnv *, jclass, jint, jlong);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    flistxattr
 * Signature: (IJI)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_flistxattr
  (JNIEnv *, jclass, jint, jlong, jint);

/*
 * Class:     sun_nio_fs_LinuxNativeDispatcher
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sun_nio_fs_LinuxNativeDispatcher_init
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif
#endif
