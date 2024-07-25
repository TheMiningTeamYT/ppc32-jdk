/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sun_tracing_dtrace_JVM */

#ifndef _Included_sun_tracing_dtrace_JVM
#define _Included_sun_tracing_dtrace_JVM
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     sun_tracing_dtrace_JVM
 * Method:    activate0
 * Signature: (Ljava/lang/String;[Lsun/tracing/dtrace/DTraceProvider;)J
 */
JNIEXPORT jlong JNICALL Java_sun_tracing_dtrace_JVM_activate0
  (JNIEnv *, jclass, jstring, jobjectArray);

/*
 * Class:     sun_tracing_dtrace_JVM
 * Method:    dispose0
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_sun_tracing_dtrace_JVM_dispose0
  (JNIEnv *, jclass, jlong);

/*
 * Class:     sun_tracing_dtrace_JVM
 * Method:    isEnabled0
 * Signature: (Ljava/lang/reflect/Method;)Z
 */
JNIEXPORT jboolean JNICALL Java_sun_tracing_dtrace_JVM_isEnabled0
  (JNIEnv *, jclass, jobject);

/*
 * Class:     sun_tracing_dtrace_JVM
 * Method:    isSupported0
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sun_tracing_dtrace_JVM_isSupported0
  (JNIEnv *, jclass);

/*
 * Class:     sun_tracing_dtrace_JVM
 * Method:    defineClass0
 * Signature: (Ljava/lang/ClassLoader;Ljava/lang/String;[BII)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_sun_tracing_dtrace_JVM_defineClass0
  (JNIEnv *, jclass, jobject, jstring, jbyteArray, jint, jint);

#ifdef __cplusplus
}
#endif
#endif
