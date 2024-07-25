/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class java_util_zip_Inflater */

#ifndef _Included_java_util_zip_Inflater
#define _Included_java_util_zip_Inflater
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     java_util_zip_Inflater
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_util_zip_Inflater_initIDs
  (JNIEnv *, jclass);

/*
 * Class:     java_util_zip_Inflater
 * Method:    init
 * Signature: (Z)J
 */
JNIEXPORT jlong JNICALL Java_java_util_zip_Inflater_init
  (JNIEnv *, jclass, jboolean);

/*
 * Class:     java_util_zip_Inflater
 * Method:    setDictionary
 * Signature: (J[BII)V
 */
JNIEXPORT void JNICALL Java_java_util_zip_Inflater_setDictionary
  (JNIEnv *, jclass, jlong, jbyteArray, jint, jint);

/*
 * Class:     java_util_zip_Inflater
 * Method:    inflateBytes
 * Signature: (J[BII)I
 */
JNIEXPORT jint JNICALL Java_java_util_zip_Inflater_inflateBytes
  (JNIEnv *, jobject, jlong, jbyteArray, jint, jint);

/*
 * Class:     java_util_zip_Inflater
 * Method:    getAdler
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_java_util_zip_Inflater_getAdler
  (JNIEnv *, jclass, jlong);

/*
 * Class:     java_util_zip_Inflater
 * Method:    reset
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_java_util_zip_Inflater_reset
  (JNIEnv *, jclass, jlong);

/*
 * Class:     java_util_zip_Inflater
 * Method:    end
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_java_util_zip_Inflater_end
  (JNIEnv *, jclass, jlong);

#ifdef __cplusplus
}
#endif
#endif
