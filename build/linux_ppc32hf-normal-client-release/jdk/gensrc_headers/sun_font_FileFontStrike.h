/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class sun_font_FileFontStrike */

#ifndef _Included_sun_font_FileFontStrike
#define _Included_sun_font_FileFontStrike
#ifdef __cplusplus
extern "C" {
#endif
#undef sun_font_FileFontStrike_INTMASK
#define sun_font_FileFontStrike_INTMASK 4294967295LL
#undef sun_font_FileFontStrike_complexTX
#define sun_font_FileFontStrike_complexTX 124L
#undef sun_font_FileFontStrike_INVISIBLE_GLYPHS
#define sun_font_FileFontStrike_INVISIBLE_GLYPHS 65534L
#undef sun_font_FileFontStrike_UNINITIALISED
#define sun_font_FileFontStrike_UNINITIALISED 0L
#undef sun_font_FileFontStrike_INTARRAY
#define sun_font_FileFontStrike_INTARRAY 1L
#undef sun_font_FileFontStrike_LONGARRAY
#define sun_font_FileFontStrike_LONGARRAY 2L
#undef sun_font_FileFontStrike_SEGINTARRAY
#define sun_font_FileFontStrike_SEGINTARRAY 3L
#undef sun_font_FileFontStrike_SEGLONGARRAY
#define sun_font_FileFontStrike_SEGLONGARRAY 4L
#undef sun_font_FileFontStrike_SEGSHIFT
#define sun_font_FileFontStrike_SEGSHIFT 5L
#undef sun_font_FileFontStrike_SEGSIZE
#define sun_font_FileFontStrike_SEGSIZE 32L
/*
 * Class:     sun_font_FileFontStrike
 * Method:    initNative
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sun_font_FileFontStrike_initNative
  (JNIEnv *, jclass);

/*
 * Class:     sun_font_FileFontStrike
 * Method:    _getGlyphImageFromWindows
 * Signature: (Ljava/lang/String;IIIZI)J
 */
JNIEXPORT jlong JNICALL Java_sun_font_FileFontStrike__1getGlyphImageFromWindows
  (JNIEnv *, jobject, jstring, jint, jint, jint, jboolean, jint);

#ifdef __cplusplus
}
#endif
#endif
