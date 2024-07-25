/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sun_awt_X11GraphicsEnvironment */

#ifndef _Included_sun_awt_X11GraphicsEnvironment
#define _Included_sun_awt_X11GraphicsEnvironment
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    initGLX
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sun_awt_X11GraphicsEnvironment_initGLX
  (JNIEnv *, jclass);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    initXRender
 * Signature: (ZZ)Z
 */
JNIEXPORT jboolean JNICALL Java_sun_awt_X11GraphicsEnvironment_initXRender
  (JNIEnv *, jclass, jboolean, jboolean);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    checkShmExt
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_sun_awt_X11GraphicsEnvironment_checkShmExt
  (JNIEnv *, jclass);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getDisplayString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_sun_awt_X11GraphicsEnvironment_getDisplayString
  (JNIEnv *, jclass);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    initDisplay
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_sun_awt_X11GraphicsEnvironment_initDisplay
  (JNIEnv *, jclass, jboolean);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getNumScreens
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_sun_awt_X11GraphicsEnvironment_getNumScreens
  (JNIEnv *, jobject);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getDefaultScreenNum
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_sun_awt_X11GraphicsEnvironment_getDefaultScreenNum
  (JNIEnv *, jobject);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    pRunningXinerama
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sun_awt_X11GraphicsEnvironment_pRunningXinerama
  (JNIEnv *, jclass);

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getXineramaCenterPoint
 * Signature: ()Ljava/awt/Point;
 */
JNIEXPORT jobject JNICALL Java_sun_awt_X11GraphicsEnvironment_getXineramaCenterPoint
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif
#endif