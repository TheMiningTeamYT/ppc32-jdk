/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sun_nio_ch_FileDispatcherImpl */

#ifndef _Included_sun_nio_ch_FileDispatcherImpl
#define _Included_sun_nio_ch_FileDispatcherImpl
#ifdef __cplusplus
extern "C" {
#endif
#undef sun_nio_ch_FileDispatcherImpl_NO_LOCK
#define sun_nio_ch_FileDispatcherImpl_NO_LOCK -1L
#undef sun_nio_ch_FileDispatcherImpl_LOCKED
#define sun_nio_ch_FileDispatcherImpl_LOCKED 0L
#undef sun_nio_ch_FileDispatcherImpl_RET_EX_LOCK
#define sun_nio_ch_FileDispatcherImpl_RET_EX_LOCK 1L
#undef sun_nio_ch_FileDispatcherImpl_INTERRUPTED
#define sun_nio_ch_FileDispatcherImpl_INTERRUPTED 2L
/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    read0
 * Signature: (Ljava/io/FileDescriptor;JI)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_read0
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    pread0
 * Signature: (Ljava/io/FileDescriptor;JIJ)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_pread0
  (JNIEnv *, jclass, jobject, jlong, jint, jlong);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    readv0
 * Signature: (Ljava/io/FileDescriptor;JI)J
 */
JNIEXPORT jlong JNICALL Java_sun_nio_ch_FileDispatcherImpl_readv0
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    write0
 * Signature: (Ljava/io/FileDescriptor;JI)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_write0
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    pwrite0
 * Signature: (Ljava/io/FileDescriptor;JIJ)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_pwrite0
  (JNIEnv *, jclass, jobject, jlong, jint, jlong);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    writev0
 * Signature: (Ljava/io/FileDescriptor;JI)J
 */
JNIEXPORT jlong JNICALL Java_sun_nio_ch_FileDispatcherImpl_writev0
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    force0
 * Signature: (Ljava/io/FileDescriptor;Z)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_force0
  (JNIEnv *, jclass, jobject, jboolean);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    seek0
 * Signature: (Ljava/io/FileDescriptor;J)J
 */
JNIEXPORT jlong JNICALL Java_sun_nio_ch_FileDispatcherImpl_seek0
  (JNIEnv *, jclass, jobject, jlong);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    truncate0
 * Signature: (Ljava/io/FileDescriptor;J)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_truncate0
  (JNIEnv *, jclass, jobject, jlong);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    size0
 * Signature: (Ljava/io/FileDescriptor;)J
 */
JNIEXPORT jlong JNICALL Java_sun_nio_ch_FileDispatcherImpl_size0
  (JNIEnv *, jclass, jobject);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    lock0
 * Signature: (Ljava/io/FileDescriptor;ZJJZ)I
 */
JNIEXPORT jint JNICALL Java_sun_nio_ch_FileDispatcherImpl_lock0
  (JNIEnv *, jclass, jobject, jboolean, jlong, jlong, jboolean);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    release0
 * Signature: (Ljava/io/FileDescriptor;JJ)V
 */
JNIEXPORT void JNICALL Java_sun_nio_ch_FileDispatcherImpl_release0
  (JNIEnv *, jclass, jobject, jlong, jlong);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    close0
 * Signature: (Ljava/io/FileDescriptor;)V
 */
JNIEXPORT void JNICALL Java_sun_nio_ch_FileDispatcherImpl_close0
  (JNIEnv *, jclass, jobject);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    preClose0
 * Signature: (Ljava/io/FileDescriptor;)V
 */
JNIEXPORT void JNICALL Java_sun_nio_ch_FileDispatcherImpl_preClose0
  (JNIEnv *, jclass, jobject);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    closeIntFD
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_sun_nio_ch_FileDispatcherImpl_closeIntFD
  (JNIEnv *, jclass, jint);

/*
 * Class:     sun_nio_ch_FileDispatcherImpl
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sun_nio_ch_FileDispatcherImpl_init
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif
#endif
