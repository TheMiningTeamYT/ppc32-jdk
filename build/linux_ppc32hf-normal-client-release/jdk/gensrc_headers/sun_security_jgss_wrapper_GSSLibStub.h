/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sun_security_jgss_wrapper_GSSLibStub */

#ifndef _Included_sun_security_jgss_wrapper_GSSLibStub
#define _Included_sun_security_jgss_wrapper_GSSLibStub
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    init
 * Signature: (Ljava/lang/String;Z)Z
 */
JNIEXPORT jboolean JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_init
  (JNIEnv *, jclass, jstring, jboolean);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getMechPtr
 * Signature: ([B)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getMechPtr
  (JNIEnv *, jclass, jbyteArray);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    indicateMechs
 * Signature: ()[Lorg/ietf/jgss/Oid;
 */
JNIEXPORT jobjectArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_indicateMechs
  (JNIEnv *, jclass);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    inquireNamesForMech
 * Signature: ()[Lorg/ietf/jgss/Oid;
 */
JNIEXPORT jobjectArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_inquireNamesForMech
  (JNIEnv *, jobject);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    releaseName
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_releaseName
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    importName
 * Signature: ([BLorg/ietf/jgss/Oid;)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_importName
  (JNIEnv *, jobject, jbyteArray, jobject);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    compareName
 * Signature: (JJ)Z
 */
JNIEXPORT jboolean JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_compareName
  (JNIEnv *, jobject, jlong, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    canonicalizeName
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_canonicalizeName
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    exportName
 * Signature: (J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_exportName
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    displayName
 * Signature: (J)[Ljava/lang/Object;
 */
JNIEXPORT jobjectArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_displayName
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    acquireCred
 * Signature: (JII)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_acquireCred
  (JNIEnv *, jobject, jlong, jint, jint);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    releaseCred
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_releaseCred
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getCredName
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getCredName
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getCredTime
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getCredTime
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getCredUsage
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getCredUsage
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    importContext
 * Signature: ([B)Lsun/security/jgss/wrapper/NativeGSSContext;
 */
JNIEXPORT jobject JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_importContext
  (JNIEnv *, jobject, jbyteArray);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    initContext
 * Signature: (JJLorg/ietf/jgss/ChannelBinding;[BLsun/security/jgss/wrapper/NativeGSSContext;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_initContext
  (JNIEnv *, jobject, jlong, jlong, jobject, jbyteArray, jobject);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    acceptContext
 * Signature: (JLorg/ietf/jgss/ChannelBinding;[BLsun/security/jgss/wrapper/NativeGSSContext;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_acceptContext
  (JNIEnv *, jobject, jlong, jobject, jbyteArray, jobject);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    inquireContext
 * Signature: (J)[J
 */
JNIEXPORT jlongArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_inquireContext
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getContextMech
 * Signature: (J)Lorg/ietf/jgss/Oid;
 */
JNIEXPORT jobject JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getContextMech
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getContextName
 * Signature: (JZ)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getContextName
  (JNIEnv *, jobject, jlong, jboolean);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getContextTime
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getContextTime
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    deleteContext
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_deleteContext
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    wrapSizeLimit
 * Signature: (JIII)I
 */
JNIEXPORT jint JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_wrapSizeLimit
  (JNIEnv *, jobject, jlong, jint, jint, jint);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    exportContext
 * Signature: (J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_exportContext
  (JNIEnv *, jobject, jlong);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    getMic
 * Signature: (JI[B)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_getMic
  (JNIEnv *, jobject, jlong, jint, jbyteArray);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    verifyMic
 * Signature: (J[B[BLorg/ietf/jgss/MessageProp;)V
 */
JNIEXPORT void JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_verifyMic
  (JNIEnv *, jobject, jlong, jbyteArray, jbyteArray, jobject);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    wrap
 * Signature: (J[BLorg/ietf/jgss/MessageProp;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_wrap
  (JNIEnv *, jobject, jlong, jbyteArray, jobject);

/*
 * Class:     sun_security_jgss_wrapper_GSSLibStub
 * Method:    unwrap
 * Signature: (J[BLorg/ietf/jgss/MessageProp;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_sun_security_jgss_wrapper_GSSLibStub_unwrap
  (JNIEnv *, jobject, jlong, jbyteArray, jobject);

#ifdef __cplusplus
}
#endif
#endif
