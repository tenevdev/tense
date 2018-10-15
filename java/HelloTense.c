#include <jni.h>
#include "tense.h"
#include "HelloTense.h"

JNIEXPORT jboolean JNICALL Java_HelloTense_initialise
  (JNIEnv *env, jobject thisObj) {
        if (tense_init() == -1)
                return JNI_FALSE;
        return JNI_TRUE;

}

JNIEXPORT jboolean JNICALL Java_HelloTense_destroy
  (JNIEnv *env, jobject thisObj) {
        if (tense_destroy() == -1)
                return JNI_FALSE;
        return JNI_TRUE; 
}

JNIEXPORT void JNICALL Java_HelloTense_health_1check
  (JNIEnv *env, jobject thisObj) {
        tense_health_check();
        return;
}
